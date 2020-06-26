/*
 * Copyright (c) 2020 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <mercury.h>

#include "lib-nm.h"

#define NM_ID 1

void* nm_run_client(void* _arg)
{
    struct nm_client_args *nm_args = _arg;
    hg_context_t *context;

    /* create separate context for this component */
    context = HG_Context_create_id(nm_args->class, NM_ID);
    assert(context);

    sleep(1);

    HG_Context_destroy(context);

    return(NULL);
}

void* nm_run_server(void* _arg)
{
    struct nm_server_args *nm_args = _arg;
    hg_context_t *context;

    /* create separate context for this component */
    context = HG_Context_create_id(nm_args->class, NM_ID);
    assert(context);

    sleep(1);

    HG_Context_destroy(context);

    return(NULL);
}
