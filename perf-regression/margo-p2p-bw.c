/*
 * Copyright (c) 2017 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

/* Effective streaming bandwidth test, as measured by client including RPC
 * used to start and complete the streaming operation.
 *
 * NOTE: This test is not as clean as it could be.  Because it is set up as
 * an MPI program, the server is able to make assumptions about the pattern;
 * it assumes that it should set a fill pattern after the first RPC.
 * It assumes it can read all params from argv.
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

struct options
{
    int xfer_size;
    int duration_seconds;
    int concurrency;
    int threads;
    char* mmap_filename;
    unsigned int mercury_timeout_client;
    unsigned int mercury_timeout_server;
    char* diag_file_name;
    char* na_transport;
    unsigned long g_buffer_size;
    int align_buffer;
    int warmup_seconds;
};

#define BW_TOTAL_MEM_SIZE 2147483648UL

static void* custom_mmap_alloc(const char* filename, size_t size, int rank);
static void  custom_mmap_free(const char* filename, void* addr, size_t size, int rank);

static int parse_args(int argc, char **argv, struct options *opts);
static void usage(void);

MERCURY_GEN_PROC(bw_rpc_in_t,
        ((hg_bulk_t)(bulk_handle))\
        ((int32_t)(op))\
        ((int32_t)(shutdown))\
        ((int32_t)(duration)))
MERCURY_GEN_PROC(bw_rpc_out_t,
        ((hg_size_t)(bytes_moved)))
DECLARE_MARGO_RPC_HANDLER(bw_ult);

static int run_benchmark(hg_id_t id, ssg_member_id_t target,
    ssg_group_id_t gid, margo_instance_id mid, int shutdown_flag, int duration, int print_flag);

struct bw_worker_arg
{
    double start_tm;
    margo_instance_id mid;
    ABT_mutex *cur_off_mutex;
    size_t *cur_off;
    hg_bulk_t *client_bulk_handle;
    const hg_addr_t *target_addr;
    hg_size_t bytes_moved;
    hg_bulk_op_t op;
    int duration;
};

static void bw_worker(void *_arg);

static hg_id_t g_bw_id;
static ABT_pool g_transfer_pool;
static ABT_eventual g_bw_done_eventual;
static struct options g_opts;
static char *g_buffer = NULL;
static hg_bulk_t g_bulk_handle = HG_BULK_NULL;

int main(int argc, char **argv) 
{
    margo_instance_id mid;
    int nranks;
    int my_mpi_rank;
    ssg_group_id_t gid;
    char *gid_buffer;
    size_t gid_buffer_size;
    int gid_buffer_size_int;
    int namelen;
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int i;
    ABT_xstream *bw_worker_xstreams = NULL;
    ABT_sched *bw_worker_scheds = NULL;
    struct hg_init_info hii;
    int group_size;
    int ret;

    MPI_Init(&argc, &argv);

    /* 2 process one-way bandwidth measurement only */
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);
    if(nranks != 2)
    {
        usage();
        exit(EXIT_FAILURE);
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &my_mpi_rank);
    MPI_Get_processor_name(processor_name,&namelen);

    ret = parse_args(argc, argv, &g_opts);
    if(ret < 0)
    {
        if(my_mpi_rank == 0)
            usage();
        exit(EXIT_FAILURE);
    }

    /* allocate one big buffer for rdma transfers */
    /* On server side, optionally use an mmap'd buffer.  Always calloc on
     * client. */
    if(my_mpi_rank == 0 && g_opts.mmap_filename)
    {
        g_buffer = custom_mmap_alloc(g_opts.mmap_filename, g_opts.g_buffer_size, my_mpi_rank);
    }
    else
    {
        g_buffer = NULL;
        if(g_opts.align_buffer) {
            ret = posix_memalign((void**)(&g_buffer), 4096, g_opts.g_buffer_size);
            if (ret != 0) fprintf(stderr, "Error in posix_memalign: %s\n", strerror(ret));
        }
        else
            g_buffer = calloc(g_opts.g_buffer_size, 1);
    }

    if(!g_buffer)
    {
        fprintf(stderr, "Error: unable to allocate %lu byte buffer.\n", g_opts.g_buffer_size);
        return(-1);
    }

    memset(&hii, 0, sizeof(hii));
    if((my_mpi_rank == 0 && g_opts.mercury_timeout_server == 0) ||
       (my_mpi_rank == 1 && g_opts.mercury_timeout_client == 0))
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
    if(my_mpi_rank == 0 && g_opts.mercury_timeout_server != UINT_MAX)
        margo_set_param(mid, MARGO_PARAM_PROGRESS_TIMEOUT_UB, &g_opts.mercury_timeout_server);
    if(my_mpi_rank == 1 && g_opts.mercury_timeout_client != UINT_MAX)
        margo_set_param(mid, MARGO_PARAM_PROGRESS_TIMEOUT_UB, &g_opts.mercury_timeout_client);

    g_bw_id = MARGO_REGISTER(
        mid, 
        "bw_rpc", 
        bw_rpc_in_t,
        bw_rpc_out_t,
        bw_ult);

    /* set up group */
    ret = ssg_init();
    assert(ret == SSG_SUCCESS);

    if(my_mpi_rank == 0)
    {
        /* set up server "group" on rank 0 */
        gid = ssg_group_create_mpi(mid, "margo-p2p-bw", MPI_COMM_SELF, NULL, NULL, NULL);
        assert(gid != SSG_GROUP_ID_INVALID);

        /* load group info into a buffer */
        ssg_group_id_serialize(gid,1,  &gid_buffer, &gid_buffer_size);
        assert(gid_buffer && (gid_buffer_size > 0));
        gid_buffer_size_int = (int)gid_buffer_size;
    }

    /* broadcast server group info to clients */
    MPI_Bcast(&gid_buffer_size_int, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (my_mpi_rank == 1)
    {
        /* client ranks allocate a buffer for receiving GID buffer */
        gid_buffer = calloc((size_t)gid_buffer_size_int, 1);
        assert(gid_buffer);
    }
    MPI_Bcast(gid_buffer, gid_buffer_size_int, MPI_CHAR, 0, MPI_COMM_WORLD);

    /* client observes server group */
    if (my_mpi_rank == 1)
    {
        int count=1;
        ssg_group_id_deserialize(gid_buffer, gid_buffer_size_int, &count, &gid);
        assert(gid != SSG_GROUP_ID_INVALID);

        ret = ssg_group_observe(mid, gid);
        assert(ret == SSG_SUCCESS);
    }

    /* sanity check group size on server/client */
    group_size = ssg_get_group_size(gid);
    assert(group_size == 1);

    if(my_mpi_rank == 0)
    {
        /* server side: prep everything before letting the client initiate
         * benchmark
         */
        void* buffer = g_buffer;

        /* register memory for xfer */
        ret = margo_bulk_create(mid, 1, &buffer, &g_opts.g_buffer_size, HG_BULK_READWRITE, &g_bulk_handle);
        assert(ret == 0);

        /* set up abt pool */
        if(g_opts.threads == 0)
        {
            ABT_pool pool;
            ABT_xstream xstream;
            
            /* run bulk transfers from primary pool on server */

            ret = ABT_xstream_self(&xstream);
            assert(ret == 0);

            ret = ABT_xstream_get_main_pools(xstream, 1, &pool);
            assert(ret == 0);

            g_transfer_pool = pool;
        }
        else
        {
            /* run bulk transfers from a dedicated pool */
            bw_worker_xstreams = malloc(
                    g_opts.threads * sizeof(*bw_worker_xstreams));
            bw_worker_scheds = malloc(
                    g_opts.threads * sizeof(*bw_worker_scheds));
            assert(bw_worker_xstreams && bw_worker_scheds);

            ret = ABT_pool_create_basic(ABT_POOL_FIFO_WAIT, ABT_POOL_ACCESS_MPMC,
                    ABT_TRUE, &g_transfer_pool);
            assert(ret == ABT_SUCCESS);
            for(i = 0; i < g_opts.threads; i++)
            {
                ret = ABT_sched_create_basic(ABT_SCHED_BASIC_WAIT, 1, &g_transfer_pool,
                        ABT_SCHED_CONFIG_NULL, &bw_worker_scheds[i]);
                assert(ret == ABT_SUCCESS);
                ret = ABT_xstream_create(bw_worker_scheds[i], &bw_worker_xstreams[i]);
                assert(ret == ABT_SUCCESS);
            }
        }

        /* signaling mechanism for server to exit at conclusion of test */
        ret = ABT_eventual_create(0, &g_bw_done_eventual);
        assert(ret == 0);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if(my_mpi_rank == 1)
    {
        /* rank 1 (client) initiates benchmark */

        /* warmup */
        if(g_opts.warmup_seconds)
            ret = run_benchmark(g_bw_id, ssg_get_group_member_id_from_rank(gid, 0),
                gid, mid, 0, g_opts.warmup_seconds, 0);
        assert(ret == 0);

        ret = run_benchmark(g_bw_id, ssg_get_group_member_id_from_rank(gid, 0),
            gid, mid, 1, g_opts.duration_seconds, 1);
        assert(ret == 0);

        ret = ssg_group_unobserve(gid);
        assert(ret == SSG_SUCCESS);
    }
    else
    {
        /* rank 0 (server) waits for test RPC to complete */

        ABT_eventual_wait(g_bw_done_eventual, NULL);

        /* cleanup dedicated pool if needed */
        for (i = 0; i < g_opts.threads; i++) {
            ABT_xstream_join(bw_worker_xstreams[i]);
            ABT_xstream_free(&bw_worker_xstreams[i]);
        }
        if(bw_worker_xstreams)
            free(bw_worker_xstreams);
        if(bw_worker_scheds)
            free(bw_worker_scheds);
    
        margo_bulk_free(g_bulk_handle);

        ret = ssg_group_destroy(gid);
        assert(ret == SSG_SUCCESS);
    }

    ssg_finalize();

    if(g_opts.diag_file_name)
        margo_diag_dump(mid, g_opts.diag_file_name, 1);

    free(gid_buffer);

    if(g_opts.mmap_filename == NULL) {
        free(g_buffer);
    } else {
        custom_mmap_free(g_opts.mmap_filename, g_buffer, g_opts.g_buffer_size, my_mpi_rank);
    }

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
    opts->g_buffer_size = BW_TOTAL_MEM_SIZE;
    /* warm up for 1 second by default */
    opts->warmup_seconds = 1;

    while((opt = getopt(argc, argv, "n:x:c:T:d:t:D:m:X:aw:")) != -1)
    {
        switch(opt)
        {
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
            case 'w':
                ret = sscanf(optarg, "%d", &opts->warmup_seconds);
                if(ret != 1)
                    return(-1);
                break;
            case 'X':
                ret = sscanf(optarg, "%lu", &opts->g_buffer_size);
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
            case 'D':
                ret = sscanf(optarg, "%d", &opts->duration_seconds);
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
            case 'm':
                opts->mmap_filename = strdup(optarg);
                if(!opts->mmap_filename) {
                    perror("strdup");
                    return -1;
                }
                break;
            case 'a':
                opts->align_buffer = 1;
                break;
            default:
                return(-1);
        }
    }

    if(opts->xfer_size < 1 || opts->concurrency < 1 || opts->duration_seconds < 1 || !opts->na_transport || opts->warmup_seconds < 0)
    {
        return(-1);
    }

    return(0);
}

static void usage(void)
{
    fprintf(stderr,
        "Usage: "
        "margo-p2p-bw -x <xfer_size> -D <duration> -n <na>\n"
        "\t-x <xfer_size> - size of each bulk tranfer in bytes\n"
        "\t-D <duration> - duration of test in seconds\n"
        "\t-n <na> - na transport\n"
        "\t[-c <concurrency>] - number of concurrent operations to issue with ULTs\n"
        "\t[-T <os threads>] - number of dedicated operating system threads to run ULTs on\n"
        "\t[-d <filename>] - enable diagnostics output\n"
        "\t[-t <client_progress_timeout,server_progress_timeout>] # use \"-t 0,0\" to busy spin\n"
        "\t[-m <filename>] - use memory-mapped file as buffers instead of malloc\n"
        "\t[-X <xfer_memory>] - size of total memory buffer to allocate (and iterate over during transfer) in each process\n"
        "\t[-a] - explicitly align memory buffer to page size\n"
        "\t[-w] - number of seconds to warm up before benchmark measurement\n"
        "\t\texample: mpiexec -n 2 ./margo-p2p-bw -x 4096 -D 30 -n verbs://\n"
        "\t\t(must be run with exactly 2 processes\n");
    
    return;
}

/* service an RPC that runs the bandwidth test */
static void bw_ult(hg_handle_t handle)
{
    int i;
    bw_rpc_in_t in;
    bw_rpc_out_t out;
    ABT_thread *tid_array;
    struct bw_worker_arg *arg_array;
    int ret;
    double start_time;
    margo_instance_id mid;
    const struct hg_info *hgi;
    size_t cur_off = 0;
    ABT_mutex cur_off_mutex;
    unsigned long bytes_to_check = 0;
    hg_size_t x;

    ABT_mutex_create(&cur_off_mutex);
    
    /* get handle info and margo instance */
    hgi = margo_get_info(handle);
    assert(hgi);
    mid = margo_hg_info_get_instance(hgi);
    assert(mid != MARGO_INSTANCE_NULL);

    ret = margo_get_input(handle, &in);
        assert(ret == HG_SUCCESS);

    tid_array = malloc(g_opts.concurrency * sizeof(*tid_array));
    assert(tid_array);
    arg_array = calloc(g_opts.concurrency, sizeof(*arg_array));
    assert(arg_array);

    start_time = ABT_get_wtime();
    /* create requested number of workers to run transfer */
    for(i=0; i<g_opts.concurrency; i++)
    {
        arg_array[i].start_tm = start_time;
        arg_array[i].mid = mid;
        arg_array[i].cur_off = &cur_off;
        arg_array[i].cur_off_mutex = &cur_off_mutex;
        arg_array[i].client_bulk_handle = &in.bulk_handle;
        arg_array[i].target_addr = &hgi->addr;
        arg_array[i].op = in.op;
        arg_array[i].duration = in.duration;

        ret = ABT_thread_create(g_transfer_pool, bw_worker, &arg_array[i], ABT_THREAD_ATTR_NULL, &tid_array[i]);
        assert(ret == 0);
    }

    out.bytes_moved = 0;
    for(i=0; i<g_opts.concurrency; i++)
    {
        ABT_thread_join(tid_array[i]);
        ABT_thread_free(&tid_array[i]);
        
        out.bytes_moved += arg_array[i].bytes_moved;
    }
    
    margo_respond(handle, &out);

    if(in.op == HG_BULK_PULL)
    {
        /* calculate how many bytes of the buffer have been transferred */
        bytes_to_check = (g_opts.g_buffer_size / g_opts.xfer_size) * g_opts.xfer_size;
        if(out.bytes_moved < bytes_to_check)
            bytes_to_check = out.bytes_moved;

        /* check integrity of fill pattern.  Note that this isn't as strong as
         * checking every RDMA transfer separately since we are looping around
         * and overwriting in a ring-buffer style.  We could corrupt early and
         * but then overwrite it with correct results on a later pass.
         */
        for(x=0; x<(bytes_to_check/sizeof(x)); x++)
        {
            assert(((hg_size_t*)g_buffer)[x] == x);
        }

        /* fill pattern for return trip, increment each value by 1 */
        for(x=0; x<(g_opts.g_buffer_size/sizeof(x)); x++)
            ((hg_size_t*)g_buffer)[x] = x+1;
    }

    margo_free_input(handle, &in);
    margo_destroy(handle);

    free(tid_array);

    ABT_mutex_free(&cur_off_mutex);
    
    if(in.shutdown)
        ABT_eventual_set(g_bw_done_eventual, NULL, 0);

    free(arg_array);

    return;
}
DEFINE_MARGO_RPC_HANDLER(bw_ult)

static int run_benchmark(hg_id_t id, ssg_member_id_t target,
    ssg_group_id_t gid, margo_instance_id mid, int shutdown, int duration, int print_flag)
{
    hg_handle_t handle;
    hg_addr_t target_addr;
    int ret;
    bw_rpc_in_t in;
    bw_rpc_out_t out;
    void* buffer = g_buffer;
    hg_size_t i;
    hg_size_t bytes_to_check;
    double start_ts, end_ts;

    /* fill pattern in origin buffer */
    for(i=0; i<(g_opts.g_buffer_size/sizeof(i)); i++)
        ((hg_size_t*)buffer)[i] = i;

    target_addr = ssg_get_group_member_addr(gid, target);
    assert(target_addr != HG_ADDR_NULL);

    ret = margo_create(mid, target_addr, id, &handle);
    assert(ret == 0);

    ret = margo_bulk_create(mid, 1, &buffer, &g_opts.g_buffer_size, HG_BULK_READWRITE, &in.bulk_handle);
    assert(ret == 0);
    in.op = HG_BULK_PULL;
    in.shutdown = 0;
    in.duration = duration;

    start_ts = ABT_get_wtime();
    ret = margo_forward(handle, &in);
    end_ts = ABT_get_wtime();
    assert(ret == 0);

    ret = margo_get_output(handle, &out);
    assert(ret == HG_SUCCESS);

    if(print_flag)
    {
        printf("<op>\t<warmup_seconds>\t<concurrency>\t<threads>\t<xfer_size>\t<total_bytes>\t<seconds>\t<MiB/s>\t<xfer_memory>\t<align_buffer>\n");

        printf("PULL\t%d\t%d\t%d\t%d\t%lu\t%f\t%f\t%lu\t%d\n",
            g_opts.warmup_seconds,
            g_opts.concurrency,
            g_opts.threads,
            g_opts.xfer_size,
            out.bytes_moved,
            (end_ts-start_ts),
            ((double)out.bytes_moved/(end_ts-start_ts))/(1024.0*1024.0),
            g_opts.g_buffer_size,
            g_opts.align_buffer);
    }

    margo_free_output(handle, &out);

    /* pause a moment */
    margo_thread_sleep(mid, 100);

    in.op = HG_BULK_PUSH;
    in.shutdown = shutdown;
    in.duration = duration;

    start_ts = ABT_get_wtime();
    ret = margo_forward(handle, &in);
    end_ts = ABT_get_wtime();
    assert(ret == 0);

    ret = margo_get_output(handle, &out);
    assert(ret == HG_SUCCESS);

    if(print_flag)
    {
        printf("PUSH\t%d\t%d\t%d\t%d\t%lu\t%f\t%f\t%lu\t%d\n",
            g_opts.warmup_seconds,
            g_opts.concurrency,
            g_opts.threads,
            g_opts.xfer_size,
            out.bytes_moved,
            (end_ts-start_ts),
            ((double)out.bytes_moved/(end_ts-start_ts))/(1024.0*1024.0),
            g_opts.g_buffer_size,
            g_opts.align_buffer);
    }

    /* calculate how many bytes of the buffer have been transferred */
    bytes_to_check = (g_opts.g_buffer_size / g_opts.xfer_size) * g_opts.xfer_size;
    if(out.bytes_moved < bytes_to_check)
        bytes_to_check = out.bytes_moved;
    /* check fill pattern we got back; should be what we set plus one */
    for(i=0; i<(bytes_to_check/sizeof(i)); i++)
    {
        assert(((hg_size_t*)g_buffer)[i] == i+1);
    }

    margo_free_output(handle, &out);
    margo_bulk_free(in.bulk_handle);
    margo_destroy(handle);

    return(0);
}

/* function that assists in transferring data until end condition is met */
static void bw_worker(void *_arg)
{
    struct bw_worker_arg *arg = _arg;
    double now;
    size_t my_off;
    int ret;

    // printf("# DBG: worker started.\n");

    now = ABT_get_wtime();

    while((now - arg->start_tm) < arg->duration)
    {
        /* find the offset for this transfer and then increment for next
         * one
         */
        ABT_mutex_spinlock(*arg->cur_off_mutex);
        my_off = *arg->cur_off;
        (*arg->cur_off) += g_opts.xfer_size;
        if(((*arg->cur_off)+g_opts.xfer_size) > g_opts.g_buffer_size)
            *arg->cur_off = 0;
        ABT_mutex_unlock(*arg->cur_off_mutex);

        ret = margo_bulk_transfer(arg->mid, arg->op,
                *arg->target_addr, *arg->client_bulk_handle, my_off, g_bulk_handle, my_off, g_opts.xfer_size);
        assert(ret == 0);

        arg->bytes_moved += g_opts.xfer_size;
        now = ABT_get_wtime();
        //printf("now: %f\n", now);
    }

    // printf("# DBG: worker stopped.\n");
    return;
}

static void* custom_mmap_alloc(const char* filename, size_t size, int rank)
{
    int fd;
    int ret;
    void *addr;
    char local_filename[256] = {0};

    /* make filename unique per rank in case two procs are on the same node */
    sprintf(local_filename, "%s.%d", filename, rank);

    fd = open(local_filename, O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR);
    if(fd<0)
    {
        perror("creat");
        return(NULL);
    }

    ret = posix_fallocate(fd, 0, size);
    if(ret != 0)
    {
        errno = ret;
        perror("posix_fallocate");
        return(NULL);
    }

    addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
    if(addr == MAP_FAILED)
    {
        perror("mmap");
        return(NULL);
    }

    close(fd);

    return addr;
}

static void  custom_mmap_free(const char* filename, void* addr, size_t size, int rank)
{
    char local_filename[256] = {0};

    /* make filename unique per rank in case two procs are on the same node */
    sprintf(local_filename, "%s.%d", filename, rank);

    munmap(addr, size);
    remove(local_filename);
}
