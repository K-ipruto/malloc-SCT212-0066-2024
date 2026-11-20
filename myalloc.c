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

static void write_err(const char *msg) {
    size_t len = 0;
    while (msg[len] != '\0') {
        len++;
    }
    ssize_t ignored = write(STDERR_FILENO, msg, len);
    (void)ignored;
}

static int is_valid_block(block_t *target) {
    if (target == NULL || head == NULL) {
        return 0;
    }
    block_t *cur = head;
    while (cur != NULL) {
        if (cur == target) {
            return cur->magic == BLOCK_MAGIC;
        }
        cur = cur->next;
    }
    return 0;
}

static int is_physically_adjacent(block_t *a, block_t *b) {
    return (char *)payload(a) + a->size == (char *)b;
}

static void coalesce(void) {
    block_t *cur = head;
    while (cur != NULL && cur->next != NULL) {
        if (cur->free && cur->next->free && is_physically_adjacent(cur, cur->next)) {
            block_t *nxt = cur->next;
            cur->size += HDR + nxt->size;
            cur->next = nxt->next;
            nxt->magic = 0;
            if (tail == nxt) {
                tail = cur;
            }
        } else {
            cur = cur->next;
        }
    }
}

void my_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    if ((uintptr_t)ptr % ALIGNMENT != 0) {
        write_err("my_free: invalid pointer\n");
        return;
    }

    block_t *b = header(ptr);
    if (!is_valid_block(b)) {
        write_err("my_free: invalid pointer\n");
        return;
    }

    if (b->free) {
        write_err("my_free: double free detected\n");
        return;
    }

    b->free = 1;
    coalesce();
}

void *my_calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    if (size > SIZE_MAX / nmemb) {
        return NULL;
    }

    size_t total = nmemb * size;
    void *ptr = my_malloc(total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *my_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return my_malloc(size);
    }
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }
    if ((uintptr_t)ptr % ALIGNMENT != 0) {
        return NULL;
    }

    block_t *b = header(ptr);
    if (!is_valid_block(b) || b->free) {
        return NULL;
    }

    size_t need = align_up(size);
    if (b->size >= need) {
        split_block(b, need);
        return ptr;
    }

    if (b->next != NULL && b->next->free && is_physically_adjacent(b, b->next) &&
        (b->size + HDR + b->next->size >= need)) {
        block_t *nxt = b->next;
        b->size += HDR + nxt->size;
        b->next = nxt->next;
        nxt->magic = 0;
        if (tail == nxt) {
            tail = b;
        }
        split_block(b, need);
        return ptr;
    }

    void *new_ptr = my_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }

    size_t copy_bytes = b->size < size ? b->size : size;
    memcpy(new_ptr, ptr, copy_bytes);
    my_free(ptr);
    return new_ptr;
}
