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

// All main general purpouse x86 registers
struct regs64 {
    union {
        struct {
            uint64_t rax; // Accumulator
            uint64_t rbx; // Base
            uint64_t rcx; // Counter
            uint64_t rdx; // Data
            uint64_t rsi; // Stream source
            uint64_t rdi; // Stream destination
        } named;
        uint64_t vec[6];
    };
};

volatile static void *stack_base = NULL;
static bool setup = false;
static struct block *blocks = NULL;
static size_t blocks_cap = 0;
static size_t blocks_len = 0;
static size_t allocs_since_gc = 0;

#ifdef VALGRIND
#include "valgrind.h"

static uintptr_t valgrind_check_block(uint64_t _tid, uintptr_t blockp, uintptr_t p) {
    struct block* block = (struct block*)blockp;
    return !block->free
        && !block->marked
        &&  block->start <= (volatile void *)p
        &&  block->start + block->size > (volatile void *)p;
}

static struct block* valgrind_running_check_ptr(volatile void *ptr) {
    // TODO: Use a binary search with a balanced tree.
    for (size_t i = 0; i < blocks_len; i++) {
        bool points_to_block = (bool)VALGRIND_NON_SIMD_CALL2(
            valgrind_check_block,
            (uintptr_t)&blocks[i],
            (uintptr_t)ptr
        );

        if (points_to_block) {
            // Blocks are disjoint so a pointer can only point to inside one of
            // the blocks.
            return &blocks[i];
        }
    }
    return NULL;
}

static struct block* valgrind_check_ptr(volatile void *ptr) {
    for (size_t i = 0; i < blocks_len; i++) {
        bool points_to_block = valgrind_check_block(
            0,
            (uintptr_t)&blocks[i],
            (uintptr_t)ptr
        );

        if (points_to_block) {
            // Blocks are disjoint so a pointer can only point to inside one of
            // the blocks.
            return &blocks[i];
        }
    }
    return NULL;
}

static struct block* (*check_ptr)(volatile void *ptr) = valgrind_check_ptr;

#else
static inline struct block* check_ptr(volatile void *ptr) {
    // TODO: Use a binary search with a balanced tree.
    for (size_t i = 0; i < blocks_len; i++) {
        if (!blocks[i].free
         && !blocks[i].marked
         &&  blocks[i].start <= ptr
         &&  blocks[i].start + blocks[i].size > ptr)
        {
            // Blocks are disjoint so a pointer can only point to inside one of
            // the blocks.
            return &blocks[i];
        }
    }
    return NULL;
}
#endif

void __gc_init(void *sp) {
#ifdef VALGRIND
    if (RUNNING_ON_VALGRIND) {
        check_ptr = valgrind_running_check_ptr;
    }
#endif
    stack_base = sp;
    setup = true;
}

static inline void get_regs(struct regs64* ptr) {
    asm(
        "mov %%rax, %0\n"
        "mov %%rbx, %1\n"
        "mov %%rcx, %2\n"
        "mov %%rdx, %3\n"
        "mov %%rsi, %4\n"
        "mov %%rdi, %5\n"
        :"=m" (ptr->named.rax),
         "=m" (ptr->named.rbx),
         "=m" (ptr->named.rcx),
         "=m" (ptr->named.rdx),
         "=m" (ptr->named.rsi),
         "=m" (ptr->named.rdi)
    );
}

static inline void mark_from_registers() {
    struct regs64 regs;
    get_regs(&regs);
    for (uint8_t i = 0; i < 6; i++) {
        check_ptr((volatile void*)regs.vec[i]);
    }
}

static void mark_memory(volatile void **start, volatile void **limit) {
    // FIXME: Should `ptr` actually be volatile??
    for (volatile void **ptr = start; ptr < limit; ptr++) {
        // Check if is a possible pointer to a block.
        struct block *match;
        if ((match = check_ptr(*ptr))) {
            // `ptr` is a pointer into a block, mark it as visited.
            match->marked = 1;
            mark_memory((volatile void **)match->start,
                        (volatile void **)(match->start + match->size));
        }
    }
}

static void gc_mark() {
    void *sp = NULL;
    mark_from_registers();
    mark_memory((volatile void **)&sp, (volatile void **)stack_base);
}

void gc() {
    // Mark used nodes.
    gc_mark();

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
    // TODOO: Run the garbage collector every few allocated bytes instead of
    // every few API calls.
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
static void gc_cleanup() {
    for (size_t i = 0; i < blocks_len; i++) {
        if (!blocks[i].free) {
            free((void*)blocks[i].start);
            blocks[i].free = 1;
        }
    }
    free(blocks);
}
