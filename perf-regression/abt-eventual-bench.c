/*
 * Copyright (c) 2021 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

#define _GNU_SOURCE

#include "sds-tests-config.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <abt.h>
#include <margo.h>

struct options {
    int test_duration_sec;
    int interval_msec;
    int num_ults;
};

struct bench_thread_arg {
    margo_instance_id mid;
    int               completed_ops;
    ABT_thread        tid;
    ABT_barrier       barrier;
    ABT_eventual      eventual;
    ABT_pool          pool;
};

static int  parse_args(int argc, char** argv, struct options* opts);
static void usage(void);
static void bench_thread(void* _arg);
static void ev_thread(void* _arg);

static struct options g_opts;
static double         g_start_tm;

int main(int argc, char** argv)
{
    int                      ret;
    margo_instance_id        mid;
    struct bench_thread_arg* arg_array;
    int                      i;
    ABT_pool                 pool;
    ABT_xstream              xstream;
    ABT_barrier              barrier;
    int                      total_ops = 0;
    struct rusage            use;
    double                   user_cpu_seconds1, user_cpu_seconds2;

    /* Maybe be more configurable later, but for now just start a simple
     * Margo instances with local sm communication and no extra threads
     */
    mid = margo_init("na+sm://", MARGO_CLIENT_MODE, 0, 0);
    assert(mid);

    ret = parse_args(argc, argv, &g_opts);
    if (ret < 0) {
        usage();
        exit(EXIT_FAILURE);
    }

    ret = ABT_xstream_self(&xstream);
    assert(ret == 0);

    ret = ABT_xstream_get_main_pools(xstream, 1, &pool);
    assert(ret == 0);

    ret = ABT_barrier_create(g_opts.num_ults + 1, &barrier);
    assert(ret == 0);

    arg_array = calloc(g_opts.num_ults, sizeof(*arg_array));
    assert(arg_array);
    for (i = 0; i < g_opts.num_ults; i++) {
        arg_array[i].mid     = mid;
        arg_array[i].barrier = barrier;
        arg_array[i].pool    = pool;
        ret = ABT_thread_create(pool, bench_thread, &arg_array[i],
                                ABT_THREAD_ATTR_NULL, &arg_array[i].tid);
        assert(ret == 0);
    }

    /* start capturing cpu time */
    ret = getrusage(RUSAGE_SELF, &use);
    assert(ret == 0);
    user_cpu_seconds1 = (double)use.ru_utime.tv_sec
                      + (double)use.ru_utime.tv_usec / 1000000.0;

    /* set global start time and release barrier for threads to work */
    g_start_tm = ABT_get_wtime();
    ABT_barrier_wait(barrier);

    for (i = 0; i < g_opts.num_ults; i++) {
        ABT_thread_join(arg_array[i].tid);
        ABT_thread_free(&arg_array[i].tid);
        total_ops += arg_array[i].completed_ops;
    }

    ret = getrusage(RUSAGE_SELF, &use);
    assert(ret == 0);
    user_cpu_seconds2 = (double)use.ru_utime.tv_sec
                      + (double)use.ru_utime.tv_usec / 1000000.0;

    printf(
        "#<num_ults>\t<test_duration_sec>\t<interval_msec>\t<ops/"
        "s>\t<cpu_time_sec>\n");
    printf("%d\t%d\t%d\t%f\t%f\n", g_opts.num_ults, g_opts.test_duration_sec,
           g_opts.interval_msec,
           ((double)total_ops / (double)g_opts.test_duration_sec),
           user_cpu_seconds2 - user_cpu_seconds1);

    ABT_barrier_free(&barrier);
    free(arg_array);
    margo_finalize(mid);

    return 0;
}

static int parse_args(int argc, char** argv, struct options* opts)
{
    int opt;
    int ret;

    memset(opts, 0, sizeof(*opts));

    /* default values */
    opts->test_duration_sec = 5;
    opts->interval_msec     = 1;
    opts->num_ults          = 1;

    while ((opt = getopt(argc, argv, "d:i:n:")) != -1) {
        switch (opt) {
        case 'd':
            ret = sscanf(optarg, "%d", &opts->test_duration_sec);
            if (ret != 1) return (-1);
            break;
        case 'i':
            ret = sscanf(optarg, "%d", &opts->interval_msec);
            if (ret != 1) return (-1);
            break;
        case 'n':
            ret = sscanf(optarg, "%d", &opts->num_ults);
            if (ret != 1) return (-1);
            break;
        default:
            return (-1);
        }
    }

    if (opts->test_duration_sec < 1) return (-1);
    if (opts->interval_msec < 1) return (-1);
    if (opts->num_ults < 1) return (-1);

    return (0);
}

static void usage(void)
{
    fprintf(stderr,
            "Usage: "
            "abt-eventual-bench [-d duration_seconds] [-i "
            "interval_milliseconds] [-n number_of_ults]>\n"
            "\t\t(must be run with exactly 1 process)\n");
    return;
}

static void bench_thread(void* _arg)
{
    struct bench_thread_arg* arg = _arg;
    int                      ret;

    arg->completed_ops = 0;

    ABT_barrier_wait(arg->barrier);

    while ((ABT_get_wtime() - g_start_tm) < g_opts.test_duration_sec) {
        /* every iteration we create a new eventual, create a new (detached)
         * thread, and wait on eventual
         */
        ABT_eventual_create(0, &arg->eventual);

        ret = ABT_thread_create(arg->pool, ev_thread, arg, ABT_THREAD_ATTR_NULL,
                                NULL);
        assert(ret == 0);
        ABT_eventual_wait(arg->eventual, NULL);
        ABT_eventual_free(&arg->eventual);
        arg->completed_ops++;
    }

    return;
}

static void ev_thread(void* _arg)
{
    struct bench_thread_arg* arg = _arg;

    /* TODO: make argument a double so we can do partial ms too */
    margo_thread_sleep(arg->mid, g_opts.interval_msec);
    ABT_eventual_set(arg->eventual, NULL, 0);

    return;
}
