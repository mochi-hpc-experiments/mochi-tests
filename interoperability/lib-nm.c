/*
 * Copyright (c) 2020 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include <mercury.h>

#include "lib-nm.h"

#define NM_ID 1

static int shutdown_flag = 0;

struct progress_fn_args
{
    hg_context_t *context;
};

void* progress_fn(void* _arg)
{
    struct progress_fn_args *pargs = _arg;
    int ret;
    unsigned int actual_count;

    while(!shutdown_flag)
    {
        do{
            ret = HG_Trigger(pargs->context, 0, 1, &actual_count);
        }while((ret == HG_SUCCESS) && actual_count);

        if(!shutdown_flag)
            ret = HG_Progress(pargs->context, 100);

        if(ret != HG_SUCCESS && ret != HG_TIMEOUT)
        {
            fprintf(stderr, "Error: unexpected HG_Progress() error code %d\n", ret);
            assert(0);
        }
    }

    return(NULL);
}

void* nm_run_client(void* _arg)
{
    struct nm_client_args *nm_args = _arg;
    struct progress_fn_args pargs;
    pthread_t tid;
    int ret;

    /* create separate context for this component */
    pargs.context = HG_Context_create_id(nm_args->class, NM_ID);
    assert(pargs.context);

    /* create thread to drive progress */
    ret = pthread_create(&tid, NULL, progress_fn, &pargs);
    assert(ret == 0);

    sleep(1);

    shutdown_flag = 1;
    pthread_join(tid, NULL);

    HG_Context_destroy(pargs.context);

    return(NULL);
}

void* nm_run_server(void* _arg)
{
    struct nm_server_args *nm_args = _arg;
    struct progress_fn_args pargs;
    pthread_t tid;
    int ret;

    /* create separate context for this component */
    pargs.context = HG_Context_create_id(nm_args->class, NM_ID);
    assert(pargs.context);

    /* create thread to drive progress */
    ret = pthread_create(&tid, NULL, progress_fn, &pargs);
    assert(ret == 0);

    sleep(1);

    shutdown_flag = 1;
    pthread_join(tid, NULL);

    HG_Context_destroy(pargs.context);

    return(NULL);
}
