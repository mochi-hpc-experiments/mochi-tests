/*
 * Copyright (c) 2020 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

/* Example program demonstrating margo and non-margo users of Mercury in the
 * same executable.  The margo code is in this file and runs in the main
 * thread.  The non-margo code is in lib-nm.c and runs in a separate thread.
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#include <mpi.h>

#include <margo.h>
#include <mercury.h>
#include <abt.h>
#include <ssg.h>
#include <ssg-mpi.h>
#include "lib-nm.h"

struct options {
    char* na_transport;
};

static int  parse_args(int argc, char** argv, struct options* opts);
static void usage(void);
DECLARE_MARGO_RPC_HANDLER(noop_ult);

static hg_id_t        noop_id;
static struct options g_opts;
static int            rpcs_serviced = 0;
static ABT_eventual   rpcs_serviced_eventual;

int main(int argc, char** argv)
{
    margo_instance_id      mid;
    int                    nranks;
    int                    my_mpi_rank;
    ssg_group_id_t         gid;
    char*                  gid_buffer;
    size_t                 gid_buffer_size;
    int                    gid_buffer_size_int;
    int                    namelen;
    char                   processor_name[MPI_MAX_PROCESSOR_NAME];
    int                    group_size;
    int                    ret;
    pthread_t              tid;
    struct margo_init_info mii;

    MPI_Init(&argc, &argv);

    /* 2 process test */
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

    tid = pthread_self();
    printf("# (margo) main tid: %lu\n", tid);

    /* actually start margo */
    memset(&mii, 0, sizeof(mii));
    /* note: enabling progress thread to make sure margo rpcs remain
     *       responsive even if we block in pthread calls (for example)
     */
    /* note: we will use one context for margo and one context for
     *       non-margo code
     */
    mii.json_config
        = "{ "
          "\"use_progress_thread\" : true, "
          "\"mercury\" : { "
          "\"max_contexts\": 2 "
          "} "
          "} ";
    mid = margo_init_ext(g_opts.na_transport, MARGO_SERVER_MODE, &mii);
    assert(mid);

    noop_id = MARGO_REGISTER(mid, "noop_rpc", void, void, noop_ult);

    ret = ssg_init();
    assert(ret == SSG_SUCCESS);

    if (my_mpi_rank == 0) {
        /* set up server "group" on rank 0 */
        ret = ssg_group_create_mpi(mid, "margo-p2p-latency", MPI_COMM_SELF,
                                   NULL, NULL, NULL, &gid);
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

    if (my_mpi_rank == 1) {
        int count = 1;
        ssg_group_id_deserialize(gid_buffer, gid_buffer_size_int, &count, &gid);
        assert(gid != SSG_GROUP_ID_INVALID);

        ret = ssg_group_refresh(mid, gid);
        assert(ret == SSG_SUCCESS);
    }

    /* sanity check group size on server/client */
    ret = ssg_get_group_size(gid, &group_size);
    assert(ret == SSG_SUCCESS);
    assert(group_size == 1);

    if (my_mpi_rank == 1) {
        /* rank 1 runs client code */
        hg_handle_t           handle;
        hg_addr_t             target_addr;
        int                   i;
        int                   ret;
        pthread_t             tid;
        struct nm_client_args nm_args;
        ssg_member_id_t       target_id;

        ret = ssg_get_group_member_id_from_rank(gid, 0, &target_id);
        assert(ret == SSG_SUCCESS);
        ret = ssg_get_group_member_addr(gid, target_id, &target_addr);
        assert(ret == SSG_SUCCESS);

        /* create pthread to run some non-margo code */
        nm_args.class       = margo_get_class(mid);
        nm_args.target_addr = target_addr;
        ret = pthread_create(&tid, NULL, nm_run_client, &nm_args);
        assert(ret == 0);

        printf("# (non-margo) nm_run_client tid: %lu\n", tid);

        /* do margo stuff */
        ret = margo_create(mid, target_addr, noop_id, &handle);
        assert(ret == 0);

        for (i = 0; i < 10; i++) {
            ret = margo_forward(handle, NULL);
            assert(ret == 0);
        }

        margo_destroy(handle);

        /* wait for non-margo pthread to exit */
        pthread_join(tid, NULL);
    } else {
        /* rank 0 acts as server */
        pthread_t             tid;
        struct nm_server_args nm_args;

        ret = ABT_eventual_create(0, &rpcs_serviced_eventual);
        assert(ret == 0);

        /* create pthread to run some non-margo code */
        nm_args.class = margo_get_class(mid);
        ret           = pthread_create(&tid, NULL, nm_run_server, &nm_args);
        assert(ret == 0);

        printf("# (non-margo) nm_run_server tid: %lu\n", tid);

        /* wait for for margo service stuff to finish */
        ABT_eventual_wait(rpcs_serviced_eventual, NULL);
        assert(rpcs_serviced == (10));

        /* wait for non-margo pthread to exit */
        pthread_join(tid, NULL);

        /* wait a little for client to finish */
        margo_thread_sleep(mid, 2000);
    }

    ret = ssg_group_destroy(gid);
    assert(ret == SSG_SUCCESS);
    ssg_finalize();

    free(gid_buffer);

    margo_finalize(mid);
    MPI_Finalize();

    return 0;
}

static int parse_args(int argc, char** argv, struct options* opts)
{
    int opt;

    memset(opts, 0, sizeof(*opts));

    while ((opt = getopt(argc, argv, "n:")) != -1) {
        switch (opt) {
        case 'n':
            opts->na_transport = strdup(optarg);
            if (!opts->na_transport) {
                perror("strdup");
                return (-1);
            }
            break;
        default:
            return (-1);
        }
    }

    if (!opts->na_transport) return (-1);

    return (0);
}

static void usage(void)
{
    fprintf(stderr,
            "Usage: margo-plus-non-margo -n <na>\n"
            "\t\texample: mpiexec -n 2 ./margo-plus-non-margo -n verbs://\n");
    return;
}

/* service a remote RPC for a no-op */
static void noop_ult(hg_handle_t handle)
{
    margo_respond(handle, NULL);
    margo_destroy(handle);
    pthread_t tid;

    tid = pthread_self();
    printf("# (margo) noop_ult tid: %lu\n", tid);

    rpcs_serviced++;
    if (rpcs_serviced == 10) ABT_eventual_set(rpcs_serviced_eventual, NULL, 0);

    return;
}
DEFINE_MARGO_RPC_HANDLER(noop_ult)
