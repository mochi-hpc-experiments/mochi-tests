/*
 * Copyright (c) 2020 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

#ifndef LIB_NM_H
#define LIB_NM_H

struct nm_client_args {
    hg_class_t* class;
    hg_addr_t target_addr;
};
void* nm_run_client(void* _arg);

struct nm_server_args {
    hg_class_t* class;
};
void* nm_run_server(void* _arg);

#endif /* LIB_NM_H */
