/*
 * Copyright (c) 2017 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <mpi.h>

#include <margo.h>
#include <mercury.h>
#include <abt.h>
#include <ssg.h>
#include <ssg-mpi.h>
#include <mercury_proc_string.h>

#include "sds-tests-config.h"

struct options {
    int          iterations;
    unsigned int mercury_timeout_client;
    unsigned int mercury_timeout_server;
    char*        diag_file_name;
    char*        na_transport;
    int          warmup_iterations;
    int          margo_forward_timed_flag;
    int          xfer_size;
};

MERCURY_GEN_PROC(noop_in_t, ((hg_string_t)(payload)))
MERCURY_GEN_PROC(noop_out_t, ((hg_string_t)(payload)))

static int  parse_args(int argc, char** argv, struct options* opts);
static void usage(void);
static int  run_benchmark(int               warmup_iterations,
                          int               iterations,
                          int               payload_size,
                          hg_id_t           id,
                          ssg_member_id_t   target,
                          ssg_group_id_t    gid,
                          margo_instance_id mid,
                          double*           measurement_array,
                          int               margo_forward_timed_flag);
static void bench_routine_print(const char* op,
                                int         size,
                                int         iterations,
                                int         warmup_iterations,
                                double*     measurement_array);
static int  measurement_cmp(const void* a, const void* b);
DECLARE_MARGO_RPC_HANDLER(noop_ult);

static hg_id_t        noop_id;
static int            rpcs_serviced = 0;
static ABT_eventual   rpcs_serviced_eventual;
static struct options g_opts;

int main(int argc, char** argv)
{
    margo_instance_id      mid;
    int                    nranks;
    int                    my_mpi_rank;
    ssg_group_id_t         gid;
    char*                  gid_buffer;
    size_t                 gid_buffer_size;
    int                    gid_buffer_size_int;
    double*                measurement_array;
    int                    namelen;
    char                   processor_name[MPI_MAX_PROCESSOR_NAME];
    struct margo_init_info mii;
    struct hg_init_info    hii;
    int                    group_size;
    int                    ret;

    MPI_Init(&argc, &argv);

    /* 2 process rtt measurements only */
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);
    if (nranks != 2) {
        usage();
        exit(EXIT_FAILURE);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &my_mpi_rank);
    MPI_Get_processor_name(processor_name, &namelen);

    ret = parse_args(argc, argv, &g_opts);
    if (ret < 0) {
        if (my_mpi_rank == 0) usage();
        exit(EXIT_FAILURE);
    }

    memset(&mii, 0, sizeof(mii));
    memset(&hii, 0, sizeof(hii));
    mii.hg_init_info = &hii;
    if ((my_mpi_rank == 0 && g_opts.mercury_timeout_server == 0)
        || (my_mpi_rank == 1 && g_opts.mercury_timeout_client == 0)) {
        /* If mercury timeout of zero is requested, then set
         * init option to NO_BLOCK.  This allows some transports to go
         * faster because they do not have to set up or maintain the data
         * structures necessary for signaling completion on blocked
         * operations.
         */
        hii.na_init_info.progress_mode = NA_NO_BLOCK;
    }

    /* actually start margo */
    mid = margo_init_ext(g_opts.na_transport, MARGO_SERVER_MODE, &mii);
    assert(mid);

    if (g_opts.diag_file_name) margo_diag_start(mid);

    /* adjust mercury timeout in Margo if requested */
    if (my_mpi_rank == 0 && g_opts.mercury_timeout_server != UINT_MAX) {
        char timeout_s[64];
        snprintf(timeout_s, 64, "%u", g_opts.mercury_timeout_server);
        margo_set_param(mid, "progress_timeout_ub_msec", timeout_s);
    }
    if (my_mpi_rank == 1 && g_opts.mercury_timeout_client != UINT_MAX) {
        char timeout_s[64];
        snprintf(timeout_s, 64, "%u", g_opts.mercury_timeout_client);
        margo_set_param(mid, "progress_timeout_ub_msec", timeout_s);
    }

    noop_id = MARGO_REGISTER(mid, "noop_rpc", noop_in_t, noop_out_t, noop_ult);

    ret = ssg_init();
    assert(ret == SSG_SUCCESS);

    if (my_mpi_rank == 0) {
        /* set up server "group" on rank 0 */
        ret = ssg_group_create_mpi(mid, "bake-bench", MPI_COMM_SELF, NULL, NULL,
                                   NULL, &gid);
        assert(ret == SSG_SUCCESS);

        /* load group info into a buffer */
        ssg_group_id_serialize(gid, 1, &gid_buffer, &gid_buffer_size);
        assert(gid_buffer && (gid_buffer_size > 0));
        gid_buffer_size_int = (int)gid_buffer_size;
    }

    /* broadcast server group info to clients */
    MPI_Bcast(&gid_buffer_size_int, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (my_mpi_rank == 1) {
        /* client ranks allocate a buffer for receiving GID buffer */
        gid_buffer = calloc((size_t)gid_buffer_size_int, 1);
        assert(gid_buffer);
    }
    MPI_Bcast(gid_buffer, gid_buffer_size_int, MPI_CHAR, 0, MPI_COMM_WORLD);

    /* client observes server group */
    if (my_mpi_rank == 1) {
        int count = 1;
        ssg_group_id_deserialize(gid_buffer, gid_buffer_size_int, &count, &gid);
        assert(gid != SSG_GROUP_ID_INVALID);

        ret = ssg_group_observe(mid, gid);
        assert(ret == SSG_SUCCESS);
    }

    /* sanity check group size on server/client */
    ret = ssg_get_group_size(gid, &group_size);
    assert(ret == SSG_SUCCESS);

    if (my_mpi_rank == 1) {
        ssg_member_id_t target;
        /* rank 1 runs client benchmark */

        measurement_array
            = calloc(g_opts.iterations, sizeof(*measurement_array));
        assert(measurement_array);

        ret = ssg_get_group_member_id_from_rank(gid, 0, &target);
        assert(ret == SSG_SUCCESS);
        ret = run_benchmark(g_opts.warmup_iterations, g_opts.iterations,
                            g_opts.xfer_size, noop_id, target, gid, mid,
                            measurement_array, g_opts.margo_forward_timed_flag);
        assert(ret == 0);

        printf(
            "# <op> <iterations> <warmup_iterations> <size> <min> <q1> <med> "
            "<avg> <q3> <max>\n");
        bench_routine_print("noop", g_opts.xfer_size, g_opts.iterations,
                            g_opts.warmup_iterations, measurement_array);
        free(measurement_array);

        ret = ssg_group_unobserve(gid);
        assert(ret == SSG_SUCCESS);
    } else {
        /* rank 0 acts as server, waiting until iterations have been completed
         */

        ret = ABT_eventual_create(0, &rpcs_serviced_eventual);
        assert(ret == 0);

        ABT_eventual_wait(rpcs_serviced_eventual, NULL);
        assert(rpcs_serviced == (g_opts.iterations + g_opts.warmup_iterations));
        sleep(3);

        ret = ssg_group_destroy(gid);
        assert(ret == SSG_SUCCESS);
    }

    ssg_finalize();

    if (g_opts.diag_file_name) margo_diag_dump(mid, g_opts.diag_file_name, 1);

    free(gid_buffer);

    margo_finalize(mid);
    MPI_Finalize();

    return 0;
}

static int parse_args(int argc, char** argv, struct options* opts)
{
    int opt;
    int ret;

    memset(opts, 0, sizeof(*opts));

    /* default to using whatever the standard timeout is in margo */
    opts->mercury_timeout_client = UINT_MAX;
    opts->mercury_timeout_server = UINT_MAX;

    /* unless otherwise specified, do 100 iterations before timing */
    opts->warmup_iterations = 100;

    while ((opt = getopt(argc, argv, "n:i:d:t:w:Tx:")) != -1) {
        switch (opt) {
        case 'T':
            opts->margo_forward_timed_flag = 1;
            break;
        case 'd':
            opts->diag_file_name = strdup(optarg);
            if (!opts->diag_file_name) {
                perror("strdup");
                return (-1);
            }
            break;
        case 'i':
            ret = sscanf(optarg, "%d", &opts->iterations);
            if (ret != 1) return (-1);
            break;
        case 'w':
            ret = sscanf(optarg, "%d", &opts->warmup_iterations);
            if (ret != 1) return (-1);
            break;
        case 't':
            ret = sscanf(optarg, "%u,%u", &opts->mercury_timeout_client,
                         &opts->mercury_timeout_server);
            if (ret != 2) return (-1);
            break;
        case 'n':
            opts->na_transport = strdup(optarg);
            if (!opts->na_transport) {
                perror("strdup");
                return (-1);
            }
            break;
        case 'x':
            ret = sscanf(optarg, "%d", &opts->xfer_size);
            if (ret != 1) return (-1);
            break;
        default:
            return (-1);
        }
    }

    if (opts->iterations < 1 || opts->warmup_iterations < 0
        || !opts->na_transport || opts->xfer_size < 0)
        return (-1);

    return (0);
}

static void usage(void)
{
    fprintf(
        stderr,
        "Usage: "
        "margo-p2p-latency -i <iterations> -n <na>\n"
        "\t-i <iterations> - number of RPC iterations\n"
        "\t-n <na> - na transport\n"
        "\t[-x <xfer_size>] - size of req/resp payload in bytes (defaults to "
        "0)\n"
        "\t[-d filename] - enable diagnostics output\n"
        "\t[-t client_progress_timeout,server_progress_timeout] # use \"-t "
        "0,0\" to busy spin\n"
        "\t[-w <warmup_iterations>] - number of warmup iterations before "
        "measurement (defaults to 100)\n"
        "\t[-T] - use _timed() variant of margo_forward functions\n"
        "\t\texample: mpiexec -n 2 ./margo-p2p-latency -i 10000 -n verbs://\n"
        "\t\t(must be run with exactly 2 processes\n");
    return;
}

/* service a remote RPC for a no-op */
static void noop_ult(hg_handle_t handle)
{
    int        ret;
    noop_in_t  in;
    noop_out_t out;

    ret = margo_get_input(handle, &in);
    assert(ret == 0);

    out.payload = in.payload;

    margo_respond(handle, &out);
    margo_free_input(handle, &in);
    margo_destroy(handle);

    rpcs_serviced++;
    if (rpcs_serviced == (g_opts.iterations + g_opts.warmup_iterations)) {
        ABT_eventual_set(rpcs_serviced_eventual, NULL, 0);
    }

    return;
}
DEFINE_MARGO_RPC_HANDLER(noop_ult)

static int run_benchmark(int               warmup_iterations,
                         int               iterations,
                         int               payload_size,
                         hg_id_t           id,
                         ssg_member_id_t   target,
                         ssg_group_id_t    gid,
                         margo_instance_id mid,
                         double*           measurement_array,
                         int               margo_forward_timed_flag)
{
    hg_handle_t handle;
    hg_addr_t   target_addr;
    int         i;
    int         ret;
    double      tm1, tm2;
    noop_in_t   in;
    noop_out_t  out;

    ret = ssg_get_group_member_addr(gid, target, &target_addr);
    assert(ret == SSG_SUCCESS);

    ret = margo_create(mid, target_addr, id, &handle);
    assert(ret == 0);

    if (payload_size == 0)
        in.payload = NULL;
    else {
        in.payload = calloc(payload_size, 1);
        assert(in.payload);
        /* put a character into every element of payload except last one
         * (null terminator for HG string encoder)
         */
        for (i = 0; i < (payload_size - 1); i++) in.payload[i] = 'a';
    }

    for (i = 0; i < warmup_iterations; i++) {
        ret = margo_forward(handle, &in);
        assert(ret == 0);

        ret = margo_get_output(handle, &out);
        assert(ret == 0);
        margo_free_output(handle, &out);
    }

    for (i = 0; i < iterations; i++) {
        tm1 = ABT_get_wtime();
        if (margo_forward_timed_flag)
            ret = margo_forward_timed(handle, &in, 2000);
        else
            ret = margo_forward(handle, &in);
        assert(ret == 0);

        ret = margo_get_output(handle, &out);
        assert(ret == 0);
        margo_free_output(handle, &out);

        tm2                  = ABT_get_wtime();
        measurement_array[i] = tm2 - tm1;
    }

    if (in.payload) free(in.payload);

    margo_destroy(handle);

    return (0);
}

static void bench_routine_print(const char* op,
                                int         size,
                                int         iterations,
                                int         warmup_iterations,
                                double*     measurement_array)
{
    double min, max, q1, q3, med, avg, sum;
    int    bracket1, bracket2;
    int    i;

    qsort(measurement_array, iterations, sizeof(double), measurement_cmp);

    min = measurement_array[0];
    max = measurement_array[iterations - 1];

    sum = 0;
    for (i = 0; i < iterations; i++) { sum += measurement_array[i]; }
    avg = sum / (double)iterations;

    bracket1 = iterations / 2;
    if (iterations % 2)
        bracket2 = bracket1 + 1;
    else
        bracket2 = bracket1;
    med = (measurement_array[bracket1] + measurement_array[bracket2])
        / (double)2;

    bracket1 = iterations / 4;
    if (iterations % 4)
        bracket2 = bracket1 + 1;
    else
        bracket2 = bracket1;
    q1 = (measurement_array[bracket1] + measurement_array[bracket2])
       / (double)2;

    bracket1 *= 3;
    if (iterations % 4)
        bracket2 = bracket1 + 1;
    else
        bracket2 = bracket1;
    q3 = (measurement_array[bracket1] + measurement_array[bracket2])
       / (double)2;

    printf("%s\t%d\t%d\t%d\t%.9f\t%.9f\t%.9f\t%.9f\t%.9f\t%.9f\n", op,
           iterations, warmup_iterations, size, min, q1, med, avg, q3, max);
#if 0
    for(i=0; i<iterations; i++)
    {
        printf("\t%.9f", measurement_array[i]);
    }
    printf("\n");
#endif
    fflush(NULL);

    return;
}

static int measurement_cmp(const void* a, const void* b)
{
    const double* d_a = a;
    const double* d_b = b;

    if (*d_a < *d_b)
        return (-1);
    else if (*d_a > *d_b)
        return (1);
    else
        return (0);
}
