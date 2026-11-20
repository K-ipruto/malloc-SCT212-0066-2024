#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include "myalloc.h"

#define NUM_OPS 50000
#define MAX_SLOTS 1024
#define FIXED_SEED 42U

static double elapsed_ms(struct timespec start, struct timespec end) {
    double sec = (double)(end.tv_sec - start.tv_sec);
    double nsec = (double)(end.tv_nsec - start.tv_nsec) / 1e9;
    return (sec + nsec) * 1000.0;
}

static void run_myalloc_bench(const char *label, fit_policy_t policy) {
    set_policy(policy);
    srand(FIXED_SEED);

    void *slots[MAX_SLOTS];
    memset(slots, 0, sizeof(slots));

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < NUM_OPS; i++) {
        int idx = rand() % MAX_SLOTS;
        if (slots[idx] == NULL) {
            size_t sz = (size_t)(rand() % 4096) + 1;
            slots[idx] = my_malloc(sz);
            if (slots[idx] != NULL) {
                memset(slots[idx], 0xAB, 1);
            }
        } else {
            my_free(slots[idx]);
            slots[idx] = NULL;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    heap_stats_t st;
    heap_stats(&st);

    double ext_frag = (st.free_bytes > 0)
        ? (1.0 - ((double)st.largest_free / (double)st.free_bytes))
        : 0.0;

    printf("%-10s | time=%8.3f ms | heap_bytes=%zu | free_bytes=%zu | free_blocks=%zu | largest_free=%zu | ext_frag=%.4f\n",
           label, elapsed_ms(t0, t1),
           st.heap_bytes, st.free_bytes, st.free_blocks, st.largest_free, ext_frag);

    for (int i = 0; i < MAX_SLOTS; i++) {
        if (slots[i] != NULL) {
            my_free(slots[i]);
        }
    }
}

static void run_glibc_bench(void) {
    srand(FIXED_SEED);

    void *slots[MAX_SLOTS];
    memset(slots, 0, sizeof(slots));

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < NUM_OPS; i++) {
        int idx = rand() % MAX_SLOTS;
        if (slots[idx] == NULL) {
            size_t sz = (size_t)(rand() % 4096) + 1;
            slots[idx] = malloc(sz);
            if (slots[idx] != NULL) {
                memset(slots[idx], 0xAB, 1);
            }
        } else {
            free(slots[idx]);
            slots[idx] = NULL;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    printf("%-10s | time=%8.3f ms | heap_bytes=N/A (glibc) | free_bytes=N/A | free_blocks=N/A | largest_free=N/A | ext_frag=N/A\n",
           "GLIBC", elapsed_ms(t0, t1));

    for (int i = 0; i < MAX_SLOTS; i++) {
        if (slots[i] != NULL) {
            free(slots[i]);
        }
    }
}

int main(void) {
    for (int mode = 0; mode < 3; mode++) {
        pid_t pid = fork();
        if (pid == 0) {
            if (mode == 0) {
                run_myalloc_bench("FIT_FIRST", FIT_FIRST);
            } else if (mode == 1) {
                run_myalloc_bench("FIT_BEST", FIT_BEST);
            } else {
                run_glibc_bench();
            }
            return 0;
        }
        waitpid(pid, NULL, 0);
    }
    return 0;
}
