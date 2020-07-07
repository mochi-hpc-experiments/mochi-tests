/*
 * Copyright (c) 2020 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

/* Example non-margo code that can run in tandem with margo code.  The
 * explicit differences from normal Mercury bootstrapping are:
 *
 * - assume that a class has already been set up by another party (margo in
 *   this case)
 * - create a separate context (with it's own id) in that class to prevent
 *   interference
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include <mercury.h>
#include <mercury_macros.h>

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
            ret = HG_Progress(pargs->context, 10);

        if(ret != HG_SUCCESS && ret != HG_TIMEOUT)
        {
            fprintf(stderr, "Error: unexpected HG_Progress() error code %d\n", ret);
            assert(0);
        }
    }

    return(NULL);
}

static hg_return_t nm_noop_rpc_cb(hg_handle_t handle)
{
    hg_return_t ret = HG_SUCCESS;
    pthread_t tid;

    tid = pthread_self();
    printf("# (non-margo) nm_noop_rpc_cb tid: %lu\n", tid);

    /* Send response back */
    ret = HG_Respond(handle, NULL, NULL, NULL);
    assert(ret == HG_SUCCESS);

    ret = HG_Destroy(handle);
    assert(ret == HG_SUCCESS);

    return ret;
}

static int client_done_count = 0;
pthread_cond_t client_done_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t client_done_mutex = PTHREAD_MUTEX_INITIALIZER;

static hg_return_t
nm_noop_forward_cb(const struct hg_cb_info *callback_info)
{
    int ret;

    assert(callback_info->ret == HG_SUCCESS);

    pthread_mutex_lock(&client_done_mutex);
    client_done_count++;
    if(client_done_count == 10)
    {
        pthread_cond_signal(&client_done_cond);
        pthread_mutex_unlock(&client_done_mutex);
        return(HG_SUCCESS);
    }
    pthread_mutex_unlock(&client_done_mutex);

    /* keep forwarding until we hit the desired count */
    ret = HG_Forward(callback_info->info.forward.handle, nm_noop_forward_cb, NULL, NULL);
    assert(ret == 0);

    return(HG_SUCCESS);
}

void* nm_run_client(void* _arg)
{
    struct nm_client_args *nm_args = _arg;
    struct progress_fn_args pargs;
    pthread_t tid;
    int ret;
    hg_id_t nm_noop_id;
    hg_handle_t handle;

    /* create separate context for this component */
    pargs.context = HG_Context_create_id(nm_args->class, NM_ID);
    assert(pargs.context);

    nm_noop_id = MERCURY_REGISTER(nm_args->class, "nm_noop",
            void, void, NULL);

    /* create thread to drive progress */
    ret = pthread_create(&tid, NULL, progress_fn, &pargs);
    assert(ret == 0);

    printf("# (non-margo) client progress_fn tid: %lu\n", tid);

    /* By creating handle in this context, we ensure that the callback from
     * the *forward* call happens in the local progress thread.  It does not
     * dictate what context will be used on the server, though.  We use
     * HG_Set_target_id() to specify that.
     */
    ret = HG_Create(pargs.context, nm_args->target_addr,
            nm_noop_id, &handle);
    assert(ret == HG_SUCCESS);
    HG_Set_target_id(handle, NM_ID);

    ret = HG_Forward(handle, nm_noop_forward_cb, NULL, NULL);
    assert(ret == 0);

    pthread_mutex_lock(&client_done_mutex);
    while(client_done_count < 10)
        pthread_cond_wait(&client_done_cond, &client_done_mutex);
    pthread_mutex_unlock(&client_done_mutex);

    HG_Destroy(handle);

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

    MERCURY_REGISTER(nm_args->class, "nm_noop", void, void, nm_noop_rpc_cb);

    /* create thread to drive progress */
    ret = pthread_create(&tid, NULL, progress_fn, &pargs);
    assert(ret == 0);

    printf("# (non-margo) server progress_fn tid: %lu\n", tid);

    sleep(1);

    shutdown_flag = 1;
    pthread_join(tid, NULL);

    HG_Context_destroy(pargs.context);

    return(NULL);
}
