/*
 * Copyright (c) 2017 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include <mpi.h>

struct options {
    long unsigned million_iterations;
};

struct test_case {
    char* name;
    void (*fn)(long unsigned);
};

static int  parse_args(int argc, char** argv, struct options* opts);
static void usage(void);

static void test_fn_call_normal(long unsigned iters);
static void test_fn_call_inline(long unsigned iters);

static struct options   g_opts;
static struct test_case g_test_cases[]
    = {{"fn_call_normal", test_fn_call_normal},
       {"fn_call_inline", test_fn_call_inline},
       {NULL, NULL}};

int main(int argc, char** argv)
{
    int    nranks;
    int    namelen;
    char   processor_name[MPI_MAX_PROCESSOR_NAME];
    int    ret;
    int    test_idx = 0;
    double tm1, tm2;

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

    /* loop through however many test cases are defined */
    printf("#<test case>\t<m_ops>\t<total s>\t<m_ops/s>\t<ns/op>\n");
    while (g_test_cases[test_idx].fn) {
        sleep(1);
        printf("%s\t", g_test_cases[test_idx].name);

        tm1 = MPI_Wtime();
        g_test_cases[test_idx].fn(g_opts.million_iterations * 1000000);
        tm2 = MPI_Wtime();

        printf("%lu\t%f\t%f\t%f\n", g_opts.million_iterations, tm2 - tm1,
               (double)(g_opts.million_iterations) / (tm2 - tm1),
               (tm2 - tm1) / ((double)(g_opts.million_iterations)));

        test_idx++;
    }

    MPI_Finalize();

    return 0;
}

static int parse_args(int argc, char** argv, struct options* opts)
{
    int opt;
    int ret;

    memset(opts, 0, sizeof(*opts));

    while ((opt = getopt(argc, argv, "m:")) != -1) {
        switch (opt) {
        case 'm':
            ret = sscanf(optarg, "%lu", &opts->million_iterations);
            if (ret != 1) return (-1);
            break;
        default:
            return (-1);
        }
    }

    if (opts->million_iterations < 1) return (-1);

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
