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
#include <stdatomic.h>
#ifdef HAVE_X86INTRIN_H
    #include <x86intrin.h>
#endif
#ifdef HAVE_ABT_H
    #include <abt.h>
#endif
#include <sys/mman.h>

#include <mpi.h>

#include "node-microbench-util.h"

struct options {
    long unsigned million_iterations;
    long unsigned thousand_iterations;
};

struct test_case {
    char* name;
    void (*fn)(long unsigned);
};

static int  parse_args(int argc, char** argv, struct options* opts);
static void usage(void);

static void test_fn_call_normal(long unsigned iters);
static void test_fn_call_inline(long unsigned iters);
static void test_fn_call_x_obj(long unsigned iters);

static void test_mpi_wtime(long unsigned iters);
static void test_gettimeofday(long unsigned iters);
static void test_clock_gettime_realtime(long unsigned iters);
static void test_clock_gettime_realtime_coarse(long unsigned iters);
static void test_clock_gettime_monotonic(long unsigned iters);
static void test_clock_gettime_monotonic_coarse(long unsigned iters);
static void test_pthread_mutex_lock(long unsigned iters);
static void test_pthread_recursive_mutex_lock(long unsigned iters);
static void test_pthread_spin_lock(long unsigned iters);
static void test_stdatomic_lock(long unsigned iters);
#ifdef HAVE_RDTSCP_INTRINSIC
static void test_rdtscp(long unsigned iters);
#endif
#ifdef HAVE_ABT_H
static void test_abt_eventual_dynamic_allocation(long unsigned iters);
    #ifdef ABT_EVENTUAL_INITIALIZER
static void test_abt_eventual_static_allocation(long unsigned iters);
    #endif
#endif
static void test_mmap_access_firstpage_8M(long unsigned iters);
static void test_mmap_access_eachMB_8M(long unsigned iters);
static void test_malloc_access_eachMB_8M(long unsigned iters);
static void test_posix_memalign_access_eachMB_8M(long unsigned iters);
#if 0
static void test_mmap_accesspages_8M(long unsigned iters);
static void test_mmap_populate_8M(long unsigned iters);
#endif

static struct options   g_opts;
static struct test_case g_test_cases[] = {
    {"fn_call_normal", test_fn_call_normal},
    {"fn_call_inline", test_fn_call_inline},
    {"fn_call_cross_object", test_fn_call_x_obj},
    {"mpi_wtime", test_mpi_wtime},
    {"gettimeofday", test_gettimeofday},
    {"clock_gettime(REALTIME)", test_clock_gettime_realtime},
    {"clock_gettime(REALTIME_COARSE)", test_clock_gettime_realtime_coarse},
    {"clock_gettime(MONOTONIC)", test_clock_gettime_monotonic},
    {"clock_gettime(MONOTONIC_COARSE)", test_clock_gettime_monotonic_coarse},
    {"pthread_mutex_lock/unlock", test_pthread_mutex_lock},
    {"pthread_mutex_recursive_lock/unlock", test_pthread_recursive_mutex_lock},
    {"pthread_spin_lock/unlock", test_pthread_spin_lock},
    {"stdatomic lock/unlock", test_stdatomic_lock},
#ifdef HAVE_RDTSCP_INTRINSIC
    {"rdtscp", test_rdtscp},
#endif
#ifdef HAVE_ABT_H
    {"ABT_eventual dynamic per fn", test_abt_eventual_dynamic_allocation},
    #ifdef ABT_EVENTUAL_INITIALIZER
    {"ABT_eventual static per fn", test_abt_eventual_static_allocation},
    #endif
#endif
    {"mmap(8M)/munmap() with access to first page",
     test_mmap_access_firstpage_8M},
    {"mmap(8M)/munmap() with access each MB offset",
     test_mmap_access_eachMB_8M},
    {"malloc(8M)/free() with access each MB offset",
     test_malloc_access_eachMB_8M},
    {"posix_memalign(8M)/free() with access each MB offset",
     test_posix_memalign_access_eachMB_8M},
/* the following are too slow to run comfortably for 1M iterations on
 * some systems
 */
#if 0
    {"mmap(8M)/munmap() with access to all pages", test_mmap_accesspages_8M},
    {"mmap(8M,MAP_POPULATE)/munmap()", test_mmap_populate_8M},
#endif
    {NULL, NULL}};

int main(int argc, char** argv)
{
    int           nranks;
    int           namelen;
    char          processor_name[MPI_MAX_PROCESSOR_NAME];
    int           ret;
    int           test_idx = 0;
    double        tm1, tm2;
    long unsigned true_iters;

#ifdef HAVE_ABT_H
    ABT_init(0, NULL);
#endif

    MPI_Init(&argc, &argv);

    /* This is an MPI program (so that we can measure the cost of relevant
     * MPI functions and so that we can easily launch on compute nodes) but
     * it only uses one process.
     */
    MPI_Comm_size(MPI_COMM_WORLD, &nranks);
    if (nranks != 1) {
        usage();
        exit(EXIT_FAILURE);
    }
    MPI_Get_processor_name(processor_name, &namelen);

    ret = parse_args(argc, argv, &g_opts);
    if (ret < 0) {
        usage();
        exit(EXIT_FAILURE);
    }

    if (g_opts.million_iterations)
        true_iters = g_opts.million_iterations * 1000000UL;
    else
        true_iters = g_opts.thousand_iterations * 1000UL;

    /* loop through however many test cases are defined */
    printf("#<test case>\t<ops>\t<total s>\t<m_ops/s>\t<ns/op>\n");
    while (g_test_cases[test_idx].fn) {
        sleep(1);
        printf("%s\t", g_test_cases[test_idx].name);

        tm1 = MPI_Wtime();
        g_test_cases[test_idx].fn(true_iters);
        tm2 = MPI_Wtime();

        printf("%lu\t%f\t%f\t%f\n", true_iters, tm2 - tm1,
               (double)true_iters / 1e6 / (tm2 - tm1),
               ((tm2 - tm1) * 1000.0) / ((double)true_iters / 1e6));

        test_idx++;
    }

    MPI_Finalize();

#ifdef HAVE_ABT_H
    ABT_finalize();
#endif

    return 0;
}

static int parse_args(int argc, char** argv, struct options* opts)
{
    int opt;
    int ret;

    memset(opts, 0, sizeof(*opts));

    while ((opt = getopt(argc, argv, "m:t:")) != -1) {
        switch (opt) {
        case 'm':
            ret = sscanf(optarg, "%lu", &opts->million_iterations);
            if (ret != 1) return (-1);
            break;
        case 't':
            ret = sscanf(optarg, "%lu", &opts->thousand_iterations);
            if (ret != 1) return (-1);
            break;
        default:
            return (-1);
        }
    }

    if (opts->million_iterations < 1 && opts->thousand_iterations < 1)
        return (-1);
    if (opts->million_iterations > 0 && opts->thousand_iterations > 0)
        return (-1);

    return (0);
}

static void usage(void)
{
    fprintf(stderr,
            "Usage: "
            "node-microbench -m <iterations (millions)>\n"
            "\t\t(must be run with exactly 1 process)\n");
    return;
}

static inline int fn_call_inline(int i)
{
    int j = i + i;

    return (j);
}

/* how long does it take to issue an inline function call */
static void test_fn_call_inline(long unsigned iters)
{
    long unsigned i;
    int           tmp = 1;

    for (i = 0; i < iters; i++) { tmp = fn_call_inline(tmp); }

    return;
}
static int fn_call_normal(int i)
{
    int j = i + i;

    return (j);
}

/* how long does it take to issue a "normal" function call */
static void test_fn_call_normal(long unsigned iters)
{
    long unsigned i;
    int           tmp = 1;

    for (i = 0; i < iters; i++) { tmp = fn_call_normal(tmp); }

    return;
}

/* how long does it take to issue a function call to another object */
static void test_fn_call_x_obj(long unsigned iters)
{
    long unsigned i;
    int           tmp = 1;

    for (i = 0; i < iters; i++) { tmp = fn_call_x_obj(tmp); }

    return;
}

/* how expensive is MPI_Wtime()? */
static void test_mpi_wtime(long unsigned iters)
{
    long unsigned i;
    double        tm __attribute__((unused));

    for (i = 0; i < iters; i++) { tm = MPI_Wtime(); }

    return;
}

/* how expensive is gettimeofday()? */
static void test_gettimeofday(long unsigned iters)
{
    long unsigned  i;
    struct timeval tv __attribute__((unused));

    for (i = 0; i < iters; i++) { gettimeofday(&tv, NULL); }

    return;
}

/* how expensive is clock_gettime()? */
static void test_clock_gettime_realtime(long unsigned iters)
{
    long unsigned   i;
    struct timespec tp __attribute__((unused));

    for (i = 0; i < iters; i++) { clock_gettime(CLOCK_REALTIME, &tp); }

    return;
}

/* how expensive is clock_gettime()? */
static void test_clock_gettime_realtime_coarse(long unsigned iters)
{
    long unsigned   i;
    struct timespec tp __attribute__((unused));

    for (i = 0; i < iters; i++) { clock_gettime(CLOCK_REALTIME_COARSE, &tp); }

    return;
}

/* how expensive is clock_gettime()? */
static void test_clock_gettime_monotonic(long unsigned iters)
{
    long unsigned   i;
    struct timespec tp __attribute__((unused));

    for (i = 0; i < iters; i++) { clock_gettime(CLOCK_MONOTONIC, &tp); }

    return;
}

/* how expensive is clock_gettime()? */
static void test_clock_gettime_monotonic_coarse(long unsigned iters)
{
    long unsigned   i;
    struct timespec tp __attribute__((unused));

    for (i = 0; i < iters; i++) { clock_gettime(CLOCK_MONOTONIC_COARSE, &tp); }

    return;
}

#if 0
/* how expensive is mmap if we populate region */
static void test_mmap_populate_8M(long unsigned iters)
{
    long unsigned   i;
    void* buf;

    for (i = 0; i < iters; i++) {
        buf = mmap(NULL, 8388608, (PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS|MAP_POPULATE), -1, 0);
        assert(buf);
        munmap(buf, 8388608);
    }

    return;
}

/* how expensive is mmap if we access all pages? */
static void test_mmap_accesspages_8M(long unsigned iters)
{
    long unsigned   i;
    long unsigned   j;
    char* buf;
    char data[] = "Hello world.";

    for (i = 0; i < iters; i++) {
        buf = mmap(NULL, 8388608, (PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS), -1, 0);
        assert(buf);
        for(j=0; j<8388608; j += 4096) {
            memcpy(&buf[j], data, strlen(data)+1);
        }
        munmap(buf, 8388608);
    }

    return;
}
#endif

/* how expensive is posix_memalign if we access the first page of each MiB? */
static void test_posix_memalign_access_eachMB_8M(long unsigned iters)
{
    long unsigned i;
    long unsigned j;
    char*         buf;
    char          data[] = "Hello world.";
    int  ret;

    for (i = 0; i < iters; i++) {
        ret = posix_memalign((void**)&buf, 4096, 8388608);
        assert(ret == 0);
        for (j = 0; j < 8388608; j += 1048576) {
            memcpy(&buf[j], data, strlen(data) + 1);
        }
        free(buf);
    }

    return;
}

/* how expensive is malloc if we access the first page of each MiB? */
static void test_malloc_access_eachMB_8M(long unsigned iters)
{
    long unsigned i;
    long unsigned j;
    char*         buf;
    char          data[] = "Hello world.";

    for (i = 0; i < iters; i++) {
        buf = malloc(8388608);
        assert(buf);
        for (j = 0; j < 8388608; j += 1048576) {
            memcpy(&buf[j], data, strlen(data) + 1);
        }
        free(buf);
    }

    return;
}

/* how expensive is mmap if we access the first page of each MiB? */
static void test_mmap_access_eachMB_8M(long unsigned iters)
{
    long unsigned i;
    long unsigned j;
    char*         buf;
    char          data[] = "Hello world.";

    for (i = 0; i < iters; i++) {
        buf = mmap(NULL, 8388608, (PROT_READ | PROT_WRITE),
                   (MAP_PRIVATE | MAP_ANONYMOUS), -1, 0);
        assert(buf);
        for (j = 0; j < 8388608; j += 1048576) {
            memcpy(&buf[j], data, strlen(data) + 1);
        }
        munmap(buf, 8388608);
    }

    return;
}

/* how expensive is mmap if we access first page? */
static void test_mmap_access_firstpage_8M(long unsigned iters)
{
    long unsigned i;
    void*         buf;
    char          data[] = "Hello world.";

    for (i = 0; i < iters; i++) {
        buf = mmap(NULL, 8388608, (PROT_READ | PROT_WRITE),
                   (MAP_PRIVATE | MAP_ANONYMOUS), -1, 0);
        assert(buf);
        memcpy(buf, data, strlen(data) + 1);
        munmap(buf, 8388608);
    }

    return;
}

/* how expensive is pthread mutex lock/unlock? */
static void test_pthread_mutex_lock(long unsigned iters)
{
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    long unsigned   i;

    for (i = 0; i < iters; i++) {
        pthread_mutex_lock(&mtx);
        pthread_mutex_unlock(&mtx);
    }

    return;
}

static void test_pthread_recursive_mutex_lock(long unsigned iters)
{
    pthread_mutex_t mtx = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
    long unsigned   i;

    for (i = 0; i < iters; i++) {
        pthread_mutex_lock(&mtx);
        pthread_mutex_unlock(&mtx);
    }

    return;
}

static void test_pthread_spin_lock(long unsigned iters)
{
    pthread_spinlock_t sl;
    long unsigned      i;

    pthread_spin_init(&sl, PTHREAD_PROCESS_PRIVATE);

    for (i = 0; i < iters; i++) {
        pthread_spin_lock(&sl);
        pthread_spin_unlock(&sl);
    }

    pthread_spin_destroy(&sl);
    return;
}

static void test_stdatomic_lock(long unsigned iters)
{
    atomic_flag   m = ATOMIC_FLAG_INIT;
    long unsigned i;

    for (i = 0; i < iters; i++) {
        while (atomic_flag_test_and_set(&m))
            ;
        atomic_flag_clear(&m);
    }

    return;
}

#ifdef HAVE_RDTSCP_INTRINSIC

    /* NOTE: in practice the BASE_FREQ is CPU-specific.  We just choose a
     * hard-coded example value so the required math conversion
     * is included in cost measurement.
     */
    #define __EXAMPLE_BASE_FREQ 1300000000.0
static void test_rdtscp(long unsigned iters)
{
    unsigned long long rval;
    long unsigned      i;
    unsigned           flag;
    double             ts __attribute__((unused));

    for (i = 0; i < iters; i++) {
        rval = __rdtscp(&flag);
        ts   = (double)rval / __EXAMPLE_BASE_FREQ;
    }
    return;
}
#endif /* HAVE_RDTSCP_INTRINSIC */

#ifdef HAVE_ABT_H

static void test_abt_eventual_dynamic_allocation_fn(void)
{
    int          ret;
    ABT_eventual eventual;

    ret = ABT_eventual_create(0, &eventual);
    assert(ret == 0);

    /* use it for something trivial */
    ABT_eventual_set(eventual, NULL, 0);
    ABT_eventual_wait(eventual, NULL);

    ABT_eventual_free(&eventual);

    return;
}

static void test_abt_eventual_dynamic_allocation(long unsigned iters)
{
    long unsigned i;

    for (i = 0; i < iters; i++) test_abt_eventual_dynamic_allocation_fn();

    return;
}

    /* introduced in Argobots > 1.1 */
    #ifdef ABT_EVENTUAL_INITIALIZER

static void test_abt_eventual_static_allocation_fn(void)
{
    ABT_eventual_memory eventual_mem = ABT_EVENTUAL_INITIALIZER;
    ABT_eventual eventual = ABT_EVENTUAL_MEMORY_GET_HANDLE(&eventual_mem);

    /* use it for something trivial */
    ABT_eventual_set(eventual, NULL, 0);
    ABT_eventual_wait(eventual, NULL);

    return;
}

static void test_abt_eventual_static_allocation(long unsigned iters)
{
    long unsigned i;

    for (i = 0; i < iters; i++) test_abt_eventual_static_allocation_fn();

    return;
}

    #endif /* ABT_EVENTUAL_INITIALIZER */

#endif /* HAVE_ABT_H */
