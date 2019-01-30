/*
 * Copyright (c) 2019 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

/* Effective bandwidth test to a single local pmdk pool using pmemobj */

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include <abt.h>
#include <libpmemobj.h>


struct options
{
    unsigned long xfer_size;
    unsigned long total_mem_size;
    int concurrency;
    char* pmdk_pool;
    int xstreams;
};

struct bench_worker_arg
{
    ABT_mutex *cur_off_mutex;
    unsigned long *cur_off;
    PMEMobjpool *pmem_pool;
};

/* defealt to 512 MiB total xfer unless specified otherwise */
#define DEF_BW_TOTAL_MEM_SIZE 524288000UL
/* defealt to 1 MiB xfer sizes unless specified otherwise */
#define DEF_BW_XFER_SIZE 1048576UL

static int parse_args(int argc, char **argv, struct options *opts);
static void usage(void);

static struct options g_opts;
static char *g_buffer = NULL;
static ABT_pool g_transfer_pool;

static int run_benchmark(struct options *opts, PMEMobjpool *pmem_pool);
static void bench_worker(void *_arg);

int main(int argc, char **argv) 
{
    int ret;
    ABT_xstream *bw_worker_xstreams = NULL;
    ABT_sched *bw_worker_scheds = NULL;
    ABT_sched self_sched;
    ABT_xstream self_xstream;
    int i;
    PMEMobjpool *pmem_pool;

    ret = parse_args(argc, argv, &g_opts);
    if(ret < 0)
    {
        usage();
        exit(EXIT_FAILURE);
    }

    ret = ABT_init(argc, argv);
    assert(ret == 0);

    /* set primary ES to idle without polling */
    ret = ABT_sched_create_basic(ABT_SCHED_BASIC_WAIT, 0, NULL,
        ABT_SCHED_CONFIG_NULL, &self_sched);
    assert(ret == 0);
    ret = ABT_xstream_self(&self_xstream);
    assert(ret == 0);
    ret = ABT_xstream_set_main_sched(self_xstream, self_sched);
    assert(ret == 0);

    pmem_pool = pmemobj_open(g_opts.pmdk_pool, NULL);
    if(!pmem_pool)
    {
        fprintf(stderr, "pmemobj_open: %s\n", pmemobj_errormsg());
        return(-1);
    }

    /* allocate one big buffer for writes */
    g_buffer = calloc(g_opts.xfer_size, 1);
    if(!g_buffer)
    {
        fprintf(stderr, "Error: unable to allocate %lu byte buffer.\n", g_opts.xfer_size);
        return(-1);
    }

    /* set up abt pool */
    if(g_opts.xstreams <= 0)
    {
        ABT_pool pool;
        ABT_xstream xstream;
        
        /* run bulk transfers from primary pool */
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
                g_opts.xstreams * sizeof(*bw_worker_xstreams));
        bw_worker_scheds = malloc(
                g_opts.xstreams * sizeof(*bw_worker_scheds));
        assert(bw_worker_xstreams && bw_worker_scheds);

        ret = ABT_pool_create_basic(ABT_POOL_FIFO_WAIT, ABT_POOL_ACCESS_MPMC,
                ABT_TRUE, &g_transfer_pool);
        assert(ret == ABT_SUCCESS);
        for(i = 0; i < g_opts.xstreams; i++)
        {
            ret = ABT_sched_create_basic(ABT_SCHED_BASIC_WAIT, 1, &g_transfer_pool,
                    ABT_SCHED_CONFIG_NULL, &bw_worker_scheds[i]);
            assert(ret == ABT_SUCCESS);
            ret = ABT_xstream_create(bw_worker_scheds[i], &bw_worker_xstreams[i]);
            assert(ret == ABT_SUCCESS);
        }
    }

    run_benchmark(&g_opts, pmem_pool);

    for(i=0; i<g_opts.xstreams; i++)
    {
        ABT_xstream_join(bw_worker_xstreams[i]);
        ABT_xstream_free(&bw_worker_xstreams[i]);
    }

    pmemobj_close(pmem_pool);

    free(g_buffer);

    ABT_finalize();

    return 0;
}

static int parse_args(int argc, char **argv, struct options *opts)
{
    int opt;
    int ret;

    memset(opts, 0, sizeof(*opts));

    opts->concurrency = 1;
    opts->total_mem_size = DEF_BW_TOTAL_MEM_SIZE;
    opts->xfer_size = DEF_BW_XFER_SIZE;
    opts->xstreams = -1;

    while((opt = getopt(argc, argv, "x:c:p:m:T:")) != -1)
    {
        switch(opt)
        {
            case 'p':
                opts->pmdk_pool = strdup(optarg);
                if(!opts->pmdk_pool)
                {
                    perror("strdup");
                    return(-1);
                }
                break;
            case 'x':
                ret = sscanf(optarg, "%lu", &opts->xfer_size);
                if(ret != 1)
                    return(-1);
                break;
            case 'm':
                ret = sscanf(optarg, "%lu", &opts->total_mem_size);
                if(ret != 1)
                    return(-1);
                break;
            case 'T':
                ret = sscanf(optarg, "%d", &opts->xstreams);
                if(ret != 1)
                    return(-1);
                break;
            case 'c':
                ret = sscanf(optarg, "%d", &opts->concurrency);
                if(ret != 1)
                    return(-1);
                break;
            default:
                return(-1);
        }
    }

    if(opts->concurrency < 1 || !opts->pmdk_pool)
    {
        return(-1);
    }

    return(0);
}

static void usage(void)
{
    fprintf(stderr,
        "Usage: "
        "bake-p2p-bw -x <xfer_size> -m <total_mem_size> -p <pmdk pool>\n"
        "\t-x <xfer_size> - size of each bulk tranfer in bytes\n"
        "\t-m <total_mem_size> - total amount of data to write from each client process\n"
        "\t-p <pmdk pool> - existing pool created with pmempool create obj\n"
        "\t[-c concurrency] - number of concurrent operations to issue with ULTs\n"
        "\t[-T <os threads] - number of dedicated operating system threads to run ULTs on\n"
        "\t\texample: ./pmdk-bw -x 4096 -p /dev/shm/test.dat\n");
    
    return;
}

static int run_benchmark(struct options *opts, PMEMobjpool *pmem_pool)
{
    int ret;
    int i;
    ABT_thread *tid_array;
    struct bench_worker_arg *arg_array;
    ABT_mutex cur_off_mutex;
    unsigned long cur_off = 0;
    double start_tm, end_tm;

    tid_array = malloc(g_opts.concurrency * sizeof(*tid_array));
    assert(tid_array);
    arg_array = malloc(g_opts.concurrency * sizeof(*arg_array));
    assert(arg_array);

    ABT_mutex_create(&cur_off_mutex);

    start_tm = ABT_get_wtime();
    for(i=0; i<g_opts.concurrency; i++)
    {
        arg_array[i].cur_off_mutex = &cur_off_mutex;
        arg_array[i].cur_off = &cur_off;
        arg_array[i].pmem_pool = pmem_pool;
        ret = ABT_thread_create(g_transfer_pool, bench_worker, 
            &arg_array[i], ABT_THREAD_ATTR_NULL, &tid_array[i]);
        assert(ret == 0);
    }

    for(i=0; i<g_opts.concurrency; i++)
    {
        ABT_thread_join(tid_array[i]);
        ABT_thread_free(&tid_array[i]);
    }
    end_tm = ABT_get_wtime();

    printf("<op>\t<concurrency>\t<xfer_size>\t<total_bytes>\t<seconds>\t<MiB/s>\n");
    printf("create_write_persist\t%d\t%lu\t%lu\t%f\t%f\n",
        g_opts.concurrency,
        g_opts.xfer_size,
        g_opts.total_mem_size,
        (end_tm-start_tm),
        ((double)g_opts.total_mem_size/(end_tm-start_tm))/(1024.0*1024.0));

    free(tid_array);
    ABT_mutex_free(&cur_off_mutex);

    return(0);
}

static void bench_worker(void *_arg)
{
    struct bench_worker_arg* arg = _arg;
    PMEMoid oid;
    uint64_t *buffer;
    uint64_t val;
    int ret;

    ABT_mutex_spinlock(*arg->cur_off_mutex);
    while(*arg->cur_off < g_opts.total_mem_size)
    {
        (*arg->cur_off) += g_opts.xfer_size;
        ABT_mutex_unlock(*arg->cur_off_mutex);

        /* create an object */
        /* NOTE: for now we don't try to keep up with oid */
        ret = pmemobj_alloc(arg->pmem_pool, &oid, g_opts.xfer_size, 0, NULL, NULL);
        if(ret != 0)
        {
            fprintf(stderr, "pmemobj_create: %s\n", pmemobj_errormsg());
            assert(0);
        }

        /* fill in values */
        buffer = pmemobj_direct(oid);
        for(val = 0; val < g_opts.xfer_size/sizeof(val); val++)
            buffer[val] = val;

        /* persist */
        // pmemobj_persist(arg->pmem_pool, buffer, g_opts.xfer_size);

        ABT_mutex_spinlock(*arg->cur_off_mutex);
    }

    ABT_mutex_unlock(*arg->cur_off_mutex);

    return;
}
