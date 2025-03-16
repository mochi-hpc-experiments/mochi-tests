#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <sys/time.h>
#include <unistd.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_collective.h>

/* 
 * If FI_DOUBLE is not defined in your provider headers, define it here.
 * Replace the value below with the proper enumeration value if needed.
 */
#ifndef FI_DOUBLE
#define FI_DOUBLE 1
#endif

/* Get high-resolution time in seconds */
double get_time_in_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

/* Benchmark the barrier collective.
 * Performs a warm-up phase and then times a fixed number of barrier calls.
 */
void benchmark_barrier(struct fid_ep *ep, fi_addr_t coll_addr, int iterations) {
    int i, ret;
    double start, end;

    printf("Benchmark Barrier: Using collective barrier function pointer: %p\n", ep->collective->barrier);
    
    /* Warm-up phase: 10 iterations */
    for (i = 0; i < 10; i++) {
        printf("Debug: Calling fi_barrier (warm-up iteration %d)\n", i);
        ret = fi_barrier(ep, coll_addr, NULL);
        if (ret < 0) {
            fprintf(stderr, "fi_barrier warm-up failed at iteration %d: %s\n", 
                    i, fi_strerror(-ret));
            return;
        }
    }

    /* Timed phase */
    start = get_time_in_seconds();
    for (i = 0; i < iterations; i++) {
        ret = fi_barrier(ep, coll_addr, NULL);
        if (ret < 0) {
            fprintf(stderr, "fi_barrier failed at iteration %d: %s\n", 
                    i, fi_strerror(-ret));
            return;
        }
    }
    end = get_time_in_seconds();

    double avg_latency_us = ((end - start) / iterations) * 1e6;
    printf("Barrier: %d iterations, average latency = %.2f microseconds\n", 
           iterations, avg_latency_us);
}

/* Benchmark the broadcast collective.
 * Allocates a buffer of doubles, performs a warm-up, and then times the broadcast calls.
 * The 'count' parameter indicates the number of doubles to send.
 */
void benchmark_broadcast(struct fid_ep *ep, fi_addr_t coll_addr, fi_addr_t root_addr,
                         int iterations, int count) {
    int i, ret;
    double start, end;
    double *buffer = (double *) malloc(count * sizeof(double));
    if (!buffer) {
        fprintf(stderr, "Failed to allocate broadcast buffer\n");
        return;
    }

    /* Initialize the buffer with test data */
    for (i = 0; i < count; i++) {
        buffer[i] = (double) i;
    }

    printf("Benchmark Broadcast: Using collective broadcast function pointer: %p\n", ep->collective->broadcast);
    
    /* Warm-up phase: 10 iterations */
    for (i = 0; i < 10; i++) {
        printf("Debug: Calling fi_broadcast (warm-up iteration %d)\n", i);
        ret = fi_broadcast(ep, buffer, count, NULL, coll_addr, root_addr, FI_DOUBLE, 0, NULL);
        if (ret < 0) {
            fprintf(stderr, "fi_broadcast warm-up failed at iteration %d: %s\n", 
                    i, fi_strerror(-ret));
            free(buffer);
            return;
        }
    }

    /* Timed phase */
    start = get_time_in_seconds();
    for (i = 0; i < iterations; i++) {
        ret = fi_broadcast(ep, buffer, count, NULL, coll_addr, root_addr, FI_DOUBLE, 0, NULL);
        if (ret < 0) {
            fprintf(stderr, "fi_broadcast failed at iteration %d: %s\n", 
                    i, fi_strerror(-ret));
            free(buffer);
            return;
        }
    }
    end = get_time_in_seconds();

    double total_time = end - start;
    printf("Broadcast: count = %d doubles, iterations = %d, total time = %.6f s, average latency = %.2f us\n",
           count, iterations, total_time, (total_time/iterations)*1e6);
    free(buffer);
}

int main(int argc, char **argv) {
    int mpi_rank, mpi_size;
    int iterations = 1000;
    int count = 1024; // number of doubles for broadcast
    int ret;

    /* Initialize MPI */
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    /* --------------------------------------------------------------------
     * Initialize libfabric.
     * We assume the provider supports collective operations.
     * -------------------------------------------------------------------- */
    struct fi_info *hints = fi_allocinfo();
    if (!hints) {
        fprintf(stderr, "fi_allocinfo failed\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    hints->caps = FI_MSG | FI_COLLECTIVE;
    hints->mode = FI_CONTEXT;

    struct fi_info *fi;
    ret = fi_getinfo(FI_VERSION(1,5), NULL, NULL, 0, hints, &fi);
    if (ret) {
        fprintf(stderr, "fi_getinfo failed: %s\n", fi_strerror(-ret));
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    printf("[Rank %d] Provider: %s\n", mpi_rank, fi->fabric_attr->prov_name);
    
    struct fid_fabric *fabric;
    ret = fi_fabric(fi->fabric_attr, &fabric, NULL);
    if (ret) {
        fprintf(stderr, "[Rank %d] fi_fabric failed: %s\n", mpi_rank, fi_strerror(-ret));
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    struct fid_domain *domain;
    ret = fi_domain(fabric, fi, &domain, NULL);
    if (ret) {
        fprintf(stderr, "[Rank %d] fi_domain failed: %s\n", mpi_rank, fi_strerror(-ret));
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    struct fid_ep *ep;
    ret = fi_endpoint(domain, fi, &ep, NULL);
    if (ret) {
        fprintf(stderr, "[Rank %d] fi_endpoint failed: %s\n", mpi_rank, fi_strerror(-ret));
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    /* Check if collective operations are supported */
    if (!ep->collective) {
        fprintf(stderr, "[Rank %d] Collective operations not supported by provider\n", mpi_rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    /* Create and bind an address vector (AV) */
    struct fi_av_attr av_attr;
    memset(&av_attr, 0, sizeof(av_attr));
    av_attr.type = FI_AV_MAP;
    av_attr.count = mpi_size;  // Enough room for all MPI ranks

    struct fid_av *av = NULL;
    ret = fi_av_open(domain, &av_attr, &av, NULL);
    if (ret) {
        fprintf(stderr, "[Rank %d] fi_av_open failed: %s\n", mpi_rank, fi_strerror(-ret));
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    ret = fi_ep_bind(ep, &av->fid, 0);
    if (ret) {
        fprintf(stderr, "[Rank %d] fi_ep_bind for AV failed: %s\n", mpi_rank, fi_strerror(-ret));
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    /* Create and bind a completion queue (CQ) */
    struct fi_cq_attr cq_attr;
    memset(&cq_attr, 0, sizeof(cq_attr));
    cq_attr.format = FI_CQ_FORMAT_CONTEXT;
    cq_attr.size = 1024;  // Adjust size as needed

    struct fid_cq *cq;
    ret = fi_cq_open(domain, &cq_attr, &cq, NULL);
    if (ret) {
        fprintf(stderr, "[Rank %d] fi_cq_open failed: %s\n", mpi_rank, fi_strerror(-ret));
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    ret = fi_ep_bind(ep, &cq->fid, FI_RECV | FI_TRANSMIT);
    if (ret) {
        fprintf(stderr, "[Rank %d] fi_ep_bind for CQ failed: %s\n", mpi_rank, fi_strerror(-ret));
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    /* Enable the endpoint now that AV and CQ are bound */
    ret = fi_enable(ep);
    if (ret) {
        fprintf(stderr, "[Rank %d] fi_enable failed: %s\n", mpi_rank, fi_strerror(-ret));
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    /* --- Exchange libfabric addresses among MPI nodes --- */
    size_t my_addrlen = 0;
    ret = fi_getname(&ep->fid, NULL, &my_addrlen);
    if (ret != -FI_ETOOSMALL) {
        fprintf(stderr, "[Rank %d] Unexpected error from fi_getname (first call): %s\n", 
                mpi_rank, fi_strerror(-ret));
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    char *my_addr = malloc(my_addrlen);
    if (!my_addr) {
        fprintf(stderr, "[Rank %d] Memory allocation failed for local address\n", mpi_rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    ret = fi_getname(&ep->fid, my_addr, &my_addrlen);
    if (ret) {
        fprintf(stderr, "[Rank %d] fi_getname failed: %s\n", mpi_rank, fi_strerror(-ret));
        free(my_addr);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    char *all_addrs = malloc(mpi_size * my_addrlen);
    if (!all_addrs) {
        fprintf(stderr, "[Rank %d] Memory allocation failed for address exchange buffer\n", mpi_rank);
        free(my_addr);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    MPI_Allgather(my_addr, my_addrlen, MPI_BYTE,
                  all_addrs, my_addrlen, MPI_BYTE, MPI_COMM_WORLD);

    printf("[Rank %d] Exchanged libfabric addresses:\n", mpi_rank);
    for (int i = 0; i < mpi_size; i++) {
        printf("  Rank %d: ", i);
        for (size_t j = 0; j < my_addrlen; j++) {
            printf("%02x", (unsigned char) all_addrs[i * my_addrlen + j]);
        }
        printf("\n");
    }

    /* For this example, choose the collective (group) address to be rank 0's address.
       We assume fi_addr_t is 8 bytes and copy the first 8 bytes from rank 0's address. */
    fi_addr_t coll_addr = 0;
    if (my_addrlen >= sizeof(uint64_t)) {
        memcpy(&coll_addr, all_addrs, sizeof(uint64_t));
    } else {
        fprintf(stderr, "[Rank %d] Unexpected address length (%zu) too short for fi_addr_t\n", mpi_rank, my_addrlen);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    fi_addr_t root_addr = coll_addr;  /* Use rank 0's address as the broadcast root */

    printf("[Rank %d] Using collective address (from rank 0): 0x%lx\n", mpi_rank, (unsigned long) coll_addr);
    printf("[Rank %d] Endpoint collective pointer: %p\n", mpi_rank, ep->collective);
    printf("[Rank %d] Collective barrier function pointer: %p\n", mpi_rank, ep->collective->barrier);
    printf("[Rank %d] Collective broadcast function pointer: %p\n", mpi_rank, ep->collective->broadcast);

    printf("[Rank %d] Starting libfabric collective benchmark with MPI address exchange...\n", mpi_rank);

    /* Run the benchmarks */
    benchmark_barrier(ep, coll_addr, iterations);
    benchmark_broadcast(ep, coll_addr, root_addr, iterations, count);

    /* Cleanup libfabric resources */
    fi_close(&ep->fid);
    fi_close(&domain->fid);
    fi_close(&fabric->fid);
    if (av)
        fi_close(&av->fid);
    if (cq)
        fi_close(&cq->fid);
    fi_freeinfo(fi);
    fi_freeinfo(hints);

    free(my_addr);
    free(all_addrs);

    /* Finalize MPI */
    MPI_Finalize();

    return EXIT_SUCCESS;
}
