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

struct drainer
{
    int started;
    int waited;
    int done;
    int draining;
};

struct drainer *g_drainer;
/* TODO: this is crude.  Technically will be more efficient if each drainer
 * has its own mutex and cond, but then you need an atomic way to swap
 * drainers too (e.g. another mutex) and the code gets more perilous because
 * of lock ordering and race condition protection.  This is simpler but will
 * cause superfluous wake-ups and lock contention.
 */
ABT_mutex g_drainer_mutex = ABT_MUTEX_NULL;
ABT_mutex g_drainer_cond = ABT_COND_NULL;

struct options
{
    unsigned long xfer_size;
    unsigned long total_mem_size;
    int concurrency;
    char* pmdk_pool;
    int xstreams;
    int highwater; /* highwater mark if drain coalescing */
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
static ABT_pool g_transfer_pool;

static int run_benchmark(struct options *opts, PMEMobjpool **pmem_pools, int pmem_pools_count);
static void bench_worker(void *_arg);

int main(int argc, char **argv) 
{
    int ret;
    ABT_xstream *bw_worker_xstreams = NULL;
    ABT_sched *bw_worker_scheds = NULL;
    ABT_sched self_sched;
    ABT_xstream self_xstream;
    int i;
    PMEMobjpool **pmem_pools;
    int pmem_pools_count = 0;
    char *tmp_pool_name;

    /* set up argobots tunables that would normally be handled by margo */
    if(!getenv("ABT_THREAD_STACKSIZE"))
        putenv("ABT_THREAD_STACKSIZE=2097152");
    if(!getenv("ABT_MEM_MAX_NUM_STACKS"))
        putenv("ABT_MEM_MAX_NUM_STACKS=8");

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

    if(g_opts.highwater)
    {
        /* initialize tracking data structure for drain coalescing */
        g_drainer = calloc(1, sizeof(*g_drainer));
        assert(g_drainer);
        ret = ABT_mutex_create(&g_drainer_mutex);
        assert(ret == 0);
        ret = ABT_cond_create(&g_drainer_cond);
        assert(ret == 0);
    }

    /* count number of commas in pmdk_pool argument to see how many pools we
     * have
     */
    pmem_pools_count = 1;
    for(i=0; i<strlen(g_opts.pmdk_pool); i++)
        if(g_opts.pmdk_pool[i] == ',')
            pmem_pools_count++;

    pmem_pools = malloc(pmem_pools_count * sizeof(*pmem_pools));
    assert(pmem_pools);

    tmp_pool_name = strtok(g_opts.pmdk_pool, ",");
    pmem_pools[0] = pmemobj_open(tmp_pool_name, NULL);
    if(!pmem_pools[0])
    {
        fprintf(stderr, "pmemobj_open: %s\n", pmemobj_errormsg());
        return(-1);
    }

    i=1;
    while((tmp_pool_name = strtok(NULL, ",")))
    {
        pmem_pools[i] = pmemobj_open(tmp_pool_name, NULL);
        if(!pmem_pools[i])
        {
            fprintf(stderr, "pmemobj_open: %s\n", pmemobj_errormsg());
            return(-1);
        }
        i++;
    }
    assert(i==pmem_pools_count);

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

    run_benchmark(&g_opts, pmem_pools, pmem_pools_count);

    for(i=0; i<g_opts.xstreams; i++)
    {
        ABT_xstream_join(bw_worker_xstreams[i]);
        ABT_xstream_free(&bw_worker_xstreams[i]);
    }

    if(g_opts.highwater)
    {
        fprintf(stderr, "FOO: freeing\n");
        ABT_mutex_free(&g_drainer_mutex);
        ABT_cond_free(&g_drainer_cond);
    }

    for(i=0; i<pmem_pools_count; i++)
        pmemobj_close(pmem_pools[i]);

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

    while((opt = getopt(argc, argv, "x:c:p:m:T:h:")) != -1)
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
            case 'h':
                ret = sscanf(optarg, "%d", &opts->highwater);
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
        "\t-x <xfer_size> - size of each write operation in bytes\n"
        "\t-m <total_mem_size> - total amount of data to write to each pool\n"
        "\t-p <pmdk pool> - existing pool created with pmempool create obj\n"
        "\t\tnote: can be comma-separated list to use multiple pools;\n"
        "\t\t      pools will be round-robin distributed across ULTs.\n"
        "\t[-c concurrency] - number of concurrent operations to issue with ULTs\n"
        "\t[-T <os threads] - number of dedicated operating system threads to run ULTs on\n"
        "\t[-h <highwater>] - enable drain coalescing with this highwater mark\n"
        "\t\texample: ./pmdk-bw -x 4096 -p /dev/shm/test.dat\n");
    
    return;
}

struct pool_offset
{
    ABT_mutex mutex;
    unsigned long off;
};

static int run_benchmark(struct options *opts, PMEMobjpool **pmem_pools, int pmem_pools_count)
{
    int ret;
    int i;
    ABT_thread *tid_array;
    struct bench_worker_arg *arg_array;
    double start_tm, end_tm;
    int j;
    struct pool_offset* cur_off_array;

    tid_array = malloc(g_opts.concurrency * sizeof(*tid_array));
    assert(tid_array);
    arg_array = malloc(g_opts.concurrency * sizeof(*arg_array));
    assert(arg_array);
    cur_off_array = malloc(pmem_pools_count * sizeof(*cur_off_array));
    assert(cur_off_array);

    /* we keep a current offset *per pool* so that any threads sharing that
     * pool will likewise share an offset
     */
    for(i=0; i<pmem_pools_count; i++)
    {
        ABT_mutex_create(&cur_off_array[i].mutex);
        cur_off_array[i].off = 0;
    }

    j=0;
    start_tm = ABT_get_wtime();
    for(i=0; i<g_opts.concurrency; i++)
    {
        arg_array[i].cur_off_mutex = &cur_off_array[j].mutex;
        arg_array[i].cur_off = &cur_off_array[j].off;
        arg_array[i].pmem_pool = pmem_pools[j];
        j++;
        if(j>=pmem_pools_count)
            j=0;
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
        g_opts.total_mem_size * pmem_pools_count,
        (end_tm-start_tm),
        ((double)g_opts.total_mem_size * pmem_pools_count/(end_tm-start_tm))/(1024.0*1024.0));

    free(tid_array);
    for(i=0; i<pmem_pools_count; i++)
    {
        ABT_mutex_free(&cur_off_array[i].mutex);
    }
    free(cur_off_array);

    return(0);
}

static void bench_worker(void *_arg)
{
    struct bench_worker_arg* arg = _arg;
    PMEMoid oid;
    uint64_t *buffer;
    uint64_t val;
    int ret;
    struct drainer *my_drainer;
    int turn_off_the_lights = 0;

    ABT_mutex_spinlock(*arg->cur_off_mutex);
    while(*arg->cur_off < g_opts.total_mem_size)
    {
        (*arg->cur_off) += g_opts.xfer_size;
        ABT_mutex_unlock(*arg->cur_off_mutex);

        if(g_opts.highwater)
        {
            /* get drain coalescing data structure */
            ABT_mutex_lock(g_drainer_mutex);
                my_drainer = g_drainer;
                my_drainer->started++;
                if(my_drainer->started == g_opts.highwater)
                {
                    /* this drainer is full; create a new one for the next
                     * writer to coalesce on
                     */
                    g_drainer = calloc(1, sizeof(*g_drainer));
                    assert(g_drainer);
                }
            ABT_mutex_unlock(g_drainer_mutex);
        }

        /* create an object */
        /* NOTE: for now we don't try to keep up with oid */
        ret = pmemobj_alloc(arg->pmem_pool, &oid, g_opts.xfer_size, 0, NULL, NULL);
        if(ret != 0)
        {
            fprintf(stderr, "pmemobj_create: %s\n", pmemobj_errormsg());
            assert(0);
        }

        /* TODO: the offset tracking stuff is superfluous if we are just
         * setting values.  Need to think about what we want to measure
         * here.  A memcpy from a buffer local to this ULT might be a better
         * measure?
         */
        /* fill in values */
        buffer = pmemobj_direct(oid);
        for(val = 0; val < g_opts.xfer_size/sizeof(val); val++)
            buffer[val] = val;

        if(g_opts.highwater)
        {
            /* first half of persist; the drain will be done by coalescer */
            pmemobj_flush(arg->pmem_pool, buffer, g_opts.xfer_size);

            /* do drain coalescing check */
            turn_off_the_lights = 0;
            ABT_mutex_lock(g_drainer_mutex);
            my_drainer->waited++;

            if(my_drainer->waited == my_drainer->started)
            {
                /* trigger a drain; everyone who started on this drainer is
                 * now waiting on drain
                 */
                fprintf(stderr, "FOO: draining %d.\n", my_drainer->waited);
                my_drainer->draining = 1;
                ABT_cond_broadcast(g_drainer_cond);
                if(my_drainer == g_drainer)
                {
                    /* put a new drainer struct in place for the next batch while we
                     * hold lock.  People using old drainer should have a ref to it
                     * in a local var. */
                    g_drainer = calloc(1, sizeof(*g_drainer));
                    assert(g_drainer);
                }

                /* drop lock while draining */
                ABT_mutex_unlock(g_drainer_mutex);
                pmemobj_drain(arg->pmem_pool);
                ABT_mutex_lock(g_drainer_mutex);
            }
            else
            {
                fprintf(stderr, "FOO: waiting.\n");
                while(!my_drainer->draining)
                    ABT_cond_wait(g_drainer_cond, g_drainer_mutex);
            }

            /* everyone: check to see if you are the last one done; if so you
             * must clean up.  Mutex is held here.
             */
            my_drainer->done++;
            if(my_drainer->done == my_drainer->waited)
            {
                turn_off_the_lights = 1;
            }
            ABT_mutex_unlock(g_drainer_mutex);
            if(turn_off_the_lights) free(my_drainer);
        }
        else
        {
            pmemobj_persist(arg->pmem_pool, buffer, g_opts.xfer_size);
        }

        ABT_mutex_spinlock(*arg->cur_off_mutex);
    }

    ABT_mutex_unlock(*arg->cur_off_mutex);

    return;
}
