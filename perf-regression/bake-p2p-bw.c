/*
 * Copyright (c) 2018 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

/* Effective streaming bandwidth test between a single bake server and a
 * single bake client.
 */
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <mpi.h>

#include <margo.h>
#include <mercury.h>
#include <abt.h>
#include <ssg.h>
#include <ssg-mpi.h>
#include <bake-server.h>

struct options
{
    int xfer_size;
    int duration_seconds;
    int concurrency;
    int threads;
    unsigned int mercury_timeout_client;
    unsigned int mercury_timeout_server;
    char* diag_file_name;
    char* na_transport;
    char* bake_pool;
};

/* defealt to 512 MiB total xfer unless specified otherwise */
#define DEF_BW_TOTAL_MEM_SIZE 524288000UL
/* defealt to 1 MiB xfer sizes unless specified otherwise */
#define DEF_BW_XFER_SIZE 1048576UL

static int parse_args(int argc, char **argv, struct options *opts);
static void usage(void);

static struct options g_opts;
static char *g_buffer = NULL;
static hg_size_t g_buffer_size = DEF_BW_TOTAL_MEM_SIZE;

DECLARE_MARGO_RPC_HANDLER(bench_stop_ult);
static hg_id_t bench_stop_id;
static ABT_eventual bench_stop_eventual;

int main(int argc, char **argv) 
{
    margo_instance_id mid;
    int nranks;
    int ret;
    ssg_group_id_t gid;
    ssg_member_id_t self;
    int rank;
    int namelen;
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int i;
    ABT_xstream *bw_worker_xstreams = NULL;
    ABT_sched *bw_worker_scheds = NULL;
    struct hg_init_info hii;

    MPI_Init(&argc, &argv);

    /* TODO: relax this, maybe 1 server N clients? */
    /* 2 processes only */
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);
    if(nranks != 2)
    {
        usage();
        exit(EXIT_FAILURE);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Get_processor_name(processor_name,&namelen);
    printf("Process %d of %d is on %s\n",
	rank, nranks, processor_name);

    ret = parse_args(argc, argv, &g_opts);
    if(ret < 0)
    {
        if(rank == 0)
            usage();
        exit(EXIT_FAILURE);
    }

    /* allocate one big buffer for writes on client */
    if(rank == 0)
    {
        g_buffer = calloc(g_buffer_size, 1);
        if(!g_buffer)
        {
            fprintf(stderr, "Error: unable to allocate %lu byte buffer.\n", g_buffer_size);
            return(-1);
        }
    }

    memset(&hii, 0, sizeof(hii));
    if((rank == 0 && g_opts.mercury_timeout_client == 0) ||
       (rank == 1 && g_opts.mercury_timeout_server == 0))
    {
        
        /* If mercury timeout of zero is requested, then set
         * init option to NO_BLOCK.  This allows some transports to go
         * faster because they do not have to set up or maintain the data
         * structures necessary for signaling completion on blocked
         * operations.
         */
        hii.na_init_info.progress_mode = NA_NO_BLOCK;
    }

    /* actually start margo */
    mid = margo_init_opt(g_opts.na_transport, MARGO_SERVER_MODE, &hii, 0, -1);
    assert(mid);

    if(g_opts.diag_file_name)
        margo_diag_start(mid);

    /* adjust mercury timeout in Margo if requested */
    if(rank == 0 && g_opts.mercury_timeout_client != UINT_MAX)
        margo_set_param(mid, MARGO_PARAM_PROGRESS_TIMEOUT_UB, &g_opts.mercury_timeout_client);
    if(rank == 1 && g_opts.mercury_timeout_server != UINT_MAX)
        margo_set_param(mid, MARGO_PARAM_PROGRESS_TIMEOUT_UB, &g_opts.mercury_timeout_server);

    bench_stop_id = MARGO_REGISTER(
        mid, 
        "bench_stop_rpc", 
        void,
        void,
        bench_stop_ult);

    /* set up group */
    ret = ssg_init(mid);
    assert(ret == 0);
    gid = ssg_group_create_mpi("margo-p2p-latency", MPI_COMM_WORLD, NULL, NULL);
    assert(gid != SSG_GROUP_ID_NULL);

    assert(ssg_get_group_size(gid) == 2);

    self = ssg_get_group_self_id(gid);

    if(self == 1)
    {
        bake_provider_t provider;
        bake_target_id_t tid;

        /* server side */

        ret = bake_provider_register(mid, 1, BAKE_ABT_POOL_DEFAULT, &provider);
        assert(ret == 0);

        ret = bake_provider_add_storage_target(provider, g_opts.bake_pool, &tid);
        if(ret != 0)
        {
            fprintf(stderr, "Error: failed to add bake pool %s\n", g_opts.bake_pool);
            abort();
        }

        ret = ABT_eventual_create(0, &bench_stop_eventual);
        assert(ret == 0);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if(self == 0)
    {
        /* ssg id 0 (client) initiates benchmark */
        hg_handle_t handle;
        hg_addr_t target_addr;

        /* TODO: implement benchmark */
        sleep(3);

        /* tell the server we are done */
        target_addr = ssg_get_addr(gid, 1);
        assert(target_addr != HG_ADDR_NULL);
        
        ret = margo_create(mid, target_addr, bench_stop_id, &handle);
        assert(ret == 0);

        ret = margo_forward(handle, NULL);
        assert(ret == 0);

        margo_destroy(handle);
    }
    else
    {
        /* ssg id 1 (server) services requests until told to stop */
        ABT_eventual_wait(bench_stop_eventual, NULL);
        sleep(3);
    }

    ssg_group_destroy(gid);
    ssg_finalize();

    if(g_opts.diag_file_name)
        margo_diag_dump(mid, g_opts.diag_file_name, 1);

    if(rank == 0)
        free(g_buffer);

    margo_finalize(mid);
    MPI_Finalize();

    return 0;
}

static int parse_args(int argc, char **argv, struct options *opts)
{
    int opt;
    int ret;

    memset(opts, 0, sizeof(*opts));

    opts->concurrency = 1;

    /* default to using whatever the standard timeout is in margo */
    opts->mercury_timeout_client = UINT_MAX;
    opts->mercury_timeout_server = UINT_MAX; 

    while((opt = getopt(argc, argv, "n:x:c:T:d:t:p:")) != -1)
    {
        switch(opt)
        {
            case 'p':
                opts->bake_pool = strdup(optarg);
                if(!opts->bake_pool)
                {
                    perror("strdup");
                    return(-1);
                }
                break;
            case 'd':
                opts->diag_file_name = strdup(optarg);
                if(!opts->diag_file_name)
                {
                    perror("strdup");
                    return(-1);
                }
                break;
            case 'x':
                ret = sscanf(optarg, "%d", &opts->xfer_size);
                if(ret != 1)
                    return(-1);
                break;
            case 'c':
                ret = sscanf(optarg, "%d", &opts->concurrency);
                if(ret != 1)
                    return(-1);
                break;
            case 'T':
                ret = sscanf(optarg, "%d", &opts->threads);
                if(ret != 1)
                    return(-1);
                break;
            case 't':
                ret = sscanf(optarg, "%u,%u", &opts->mercury_timeout_client, &opts->mercury_timeout_server);
                if(ret != 2)
                    return(-1);
                break;
            case 'n':
                opts->na_transport = strdup(optarg);
                if(!opts->na_transport)
                {
                    perror("strdup");
                    return(-1);
                }
                break;
            default:
                return(-1);
        }
    }

    if(opts->xfer_size < 1 || opts->concurrency < 1 || !opts->na_transport 
     || !opts->bake_pool)
    {
        return(-1);
    }

    return(0);
}

static void usage(void)
{
    fprintf(stderr,
        "Usage: "
        "bake-p2p-bw -x <xfer_size> -n <na>\n"
        "\t-x <xfer_size> - size of each bulk tranfer in bytes\n"
        "\t-n <na> - na transport\n"
        "\t-p <bake pool> - existing pool created with bake-mkpool\n"
        "\t[-c concurrency] - number of concurrent operations to issue with ULTs\n"
        "\t[-T <os threads] - number of dedicated operating system threads to run ULTs on\n"
        "\t[-d filename] - enable diagnostics output\n"
        "\t[-t client_progress_timeout,server_progress_timeout] # use \"-t 0,0\" to busy spin\n"
        "\t\texample: mpiexec -n 2 ./bake-p2p-bw -x 4096 -n verbs://\n"
        "\t\t(must be run with exactly 2 processes\n");
    
    return;
}

/* tell server process that the benchmark is done */
static void bench_stop_ult(hg_handle_t handle)
{
    margo_respond(handle, NULL);
    margo_destroy(handle);

    ABT_eventual_set(bench_stop_eventual, NULL, 0);

    return;
}
DEFINE_MARGO_RPC_HANDLER(bench_stop_ult)


