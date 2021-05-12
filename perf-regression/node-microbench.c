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

static int  parse_args(int argc, char** argv, struct options* opts);
static void usage(void);

static struct options g_opts;

int main(int argc, char** argv)
{
    int  nranks;
    int  namelen;
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int  ret;

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
