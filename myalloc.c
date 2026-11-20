#define _DEFAULT_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "myalloc.h"

#define ALIGNMENT 16
#define MIN_SPLIT_PAYLOAD 16
#define GROW_MIN_BYTES (64 * 1024)
#define BLOCK_MAGIC 0x5A110C01

typedef struct block {
    size_t size;          /* payload size in bytes */
    struct block *next;   /* next block, in increasing address order */
    int free;             /* 1 = free, 0 = in use */
    int magic;            /* constant to detect bad pointers */
} block_t;

static inline size_t align_up(size_t n) {
    return (n + (ALIGNMENT - 1)) & ~(size_t)(ALIGNMENT - 1);
}

#define HDR (align_up(sizeof(block_t)))

static inline void *payload(block_t *b) {
    return (void *)((char *)b + HDR);
}

static inline block_t *header(void *ptr) {
    return (block_t *)((char *)ptr - HDR);
}

static block_t *head = NULL;
static block_t *tail = NULL;
static size_t total_heap_bytes = 0;
static fit_policy_t current_policy = FIT_FIRST;

static block_t *grow(size_t need) {
    void *cur = sbrk(0);
    if (cur == (void *)-1) {
        return NULL;
    }

    uintptr_t cur_addr = (uintptr_t)cur;
    uintptr_t payload_unaligned = cur_addr + HDR;
    size_t pad = (ALIGNMENT - (payload_unaligned % ALIGNMENT)) % ALIGNMENT;

    size_t block_total = HDR + need;
    if (block_total < GROW_MIN_BYTES) {
        block_total = GROW_MIN_BYTES;
    }
    block_total = align_up(block_total);

    size_t total_req = pad + block_total;
    void *raw = sbrk((intptr_t)total_req);
    if (raw == (void *)-1) {
        return NULL;
    }

    total_heap_bytes += total_req;

    block_t *b = (block_t *)((char *)raw + pad);
    b->size = block_total - HDR;
    b->next = NULL;
    b->free = 1;
    b->magic = BLOCK_MAGIC;

    if (head == NULL) {
        head = b;
        tail = b;
    } else {
        tail->next = b;
        tail = b;
    }

    return b;
}

static block_t *find_fit(size_t need) {
    block_t *cur = head;
    while (cur != NULL) {
        if (cur->free && cur->size >= need) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

static void split_block(block_t *b, size_t need) {
    if (b->size < need + HDR + MIN_SPLIT_PAYLOAD) {
        return;
    }

    block_t *rem = (block_t *)((char *)payload(b) + need);
    rem->size = b->size - need - HDR;
    rem->next = b->next;
    rem->free = 1;
    rem->magic = BLOCK_MAGIC;

    b->size = need;
    b->next = rem;

    if (tail == b) {
        tail = rem;
    }
}

void *my_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size_t need = align_up(size);
    block_t *b = find_fit(need);
    if (b == NULL) {
        b = grow(need);
        if (b == NULL) {
            return NULL;
        }
    }

    split_block(b, need);
    b->free = 0;
    return payload(b);
}
