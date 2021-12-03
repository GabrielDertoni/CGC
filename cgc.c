#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <string.h>

#include "cgc.h"

#define MIN_CAP 8

#define sysmalloc  malloc
#define syscalloc  calloc
#define sysrealloc realloc

struct block {
    volatile void *start;
    uint64_t size : 62;
    uint64_t marked : 1;
    uint64_t free : 1;
};

volatile static void *stack_base = NULL;
static bool setup = false;
static struct block *blocks = NULL;
static size_t blocks_cap = 0;
static size_t blocks_len = 0;
static size_t allocs_since_gc = 0;

void __init(void *sp) {
    stack_base = sp;
    setup = true;
}

static void gc_mark(volatile void **start, volatile void **limit) {
    for (volatile void **ptr = start; ptr < limit; ptr++) {
        // Check if is a possible pointer to a node.
        // TODO: Use a binary search with a balanced tree.
        for (size_t i = 0; i < blocks_len; i++) {
            if (!blocks[i].free
             && !blocks[i].marked
             &&  blocks[i].start <= *ptr
             &&  blocks[i].start + blocks[i].size > *ptr)
            {
                // sp is a pointer into that block, mark it as visited.
                blocks[i].marked = 1;
                gc_mark((volatile void **)blocks[i].start,
                        (volatile void **)(blocks[i].start + blocks[i].size));

                // Blocks are disjoints so a pointer can only point to inside
                // one of the blocks.
                break;
            }
        }
    }
}

void gc() {
    void *sp;
    // Mark used nodes.
    gc_mark((volatile void **)&sp, (volatile void **)stack_base);

    // Free umarked nodes
    for (size_t i = 0; i < blocks_len; i++) {
        if (!blocks[i].marked && !blocks[i].free) {
            free((void*)blocks[i].start);
            blocks[i].free = 1;
        }
        blocks[i].marked = 0;
    }

    allocs_since_gc = 0;
}

volatile void *gcmalloc(size_t size) {
    if (!setup) {
        fprintf(stderr, "Garbage collector not setup.\n");
        exit(1);
    }

    // Run the garbage collector.
    if (++allocs_since_gc > GC_INTERVAL) gc();

    // Perform allocation.

    // TODO: use dlsym.
    // malloc_fn *sysmalloc = dlsym(RTLD_NEXT, "malloc");
    void *ptr = sysmalloc(size);
    if (!ptr) return NULL;

    struct block alloc;
    alloc.start = ptr;
    alloc.size = size;
    alloc.marked = 0;
    alloc.free = 0;

    if (blocks_cap == blocks_len) {
        if ((blocks_cap *= 2) == 0) blocks_cap = MIN_CAP;
        blocks = sysrealloc(blocks, blocks_cap * sizeof(struct block));
    }
    blocks[blocks_len++] = alloc;
    return ptr;
}

volatile void *gccalloc(size_t nmemb, size_t size) {
    volatile void *ptr = gcmalloc(nmemb * size);
    memset((void*)ptr, 0, nmemb * size);
    return ptr;
}

volatile void *gcrealloc(void *ptr, size_t size) {
    if (++allocs_since_gc > GC_INTERVAL) gc();

    void *new_ptr = sysrealloc(ptr, size);
    if (!new_ptr) return NULL;

    for (size_t i = 0; i < blocks_len; i++) {
        if (blocks[i].start == ptr && blocks[i].free == 0) {
            blocks[i].start = new_ptr;
            blocks[i].size = size;
            break;
        }
    }

    return new_ptr;
}

__attribute__((destructor))
static void cleanup_gc() {
    gc(NULL, NULL);
    free(blocks);
}
