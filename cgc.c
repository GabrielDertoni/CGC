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

static volatile void *stack_base = NULL;
static bool setup = false;
static struct block *blocks = NULL;
static size_t blocks_cap = 0;
static size_t blocks_len = 0;
static size_t allocs_since_gc = 0;

#ifndef NDEBUG
#define assert(cond, msg)                                         \
    if (!(cond)) {                                                \
        fprintf(stderr, "C-GC error: %s\n", msg);                 \
        exit(EXIT_FAILURE);                                       \
    }
#else
#define assert(cond, msg)
#endif

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

static void mark_from_memory(volatile void **start, volatile void **limit) {
    // FIXME: Should `ptr` actually be volatile??
    for (volatile void **ptr = start; ptr < limit; ptr++) {
        // Check if is a possible pointer to a block.
        struct block *match;
        if ((match = check_ptr(*ptr))) {
            // `ptr` is a pointer into a block, mark it as visited.
            match->marked = 1;
            mark_from_memory((volatile void **)match->start,
                             (volatile void **)(match->start + match->size));
        }
    }
}

static inline void mark_from_registers() {
    struct regs64 regs;
    get_regs(&regs);
    for (uint8_t i = 0; i < 6; i++) {
        struct block *match;
        if ((match = check_ptr((volatile void*)regs.vec[i]))) {
            match->marked = 1;
            mark_from_memory((volatile void **)match->start,
                             (volatile void **)(match->start + match->size));
        }
    }
}

#ifndef NGLOBALS
static inline void mark_from_dot_data() {
#define LINE_SIZE 2048

    char actual_path[LINE_SIZE];
    ssize_t ret = readlink("/proc/self/exe", (char*)&actual_path, LINE_SIZE);
    assert(ret >= 0, "failed to to read link '/proc/self/exe'");

    FILE *fp = fopen("/proc/self/maps", "r");
    assert(fp != NULL, "failed to read '/proc/self/maps'");

    volatile void **start, **end;
    char path[LINE_SIZE];
    char buf[LINE_SIZE];
    char wp, rp;
    int n = 0;

    while (fgets(buf, LINE_SIZE, fp) != NULL
        && !(n == 5 && rp == 'r' && wp == 'w' && strcmp(path, actual_path) == 0))
    {
        n = sscanf(buf, "%lx-%lx %c%c%*c%*c %*x %*d:%*d %*d %s\n",
                   (uintptr_t*)&start, (uintptr_t*)&end, &rp, &wp, path);
    }
    fclose(fp);
    assert(n == 5 && rp == 'r' && wp == 'w' && strcmp(path, actual_path) == 0,
           "couldn't find .data segment");

    for (volatile void **ptr = start; ptr < end; ptr++) {
        struct block *match;
        if ((match = check_ptr(*ptr))) {
            match->marked = 1;
            mark_from_memory((volatile void **)match->start,
                             (volatile void **)(match->start + match->size));
        }
    }

#undef LINE_SIZE
}
#endif

// This cannot be inlined because we always want to get the frame address of the
// `gc()` function (`mark`s caller), not gc's caller. If this was inlined,
// `__builtin_frame_address(1)` would remain unchanged and get the next frame
// address: https://www.ibm.com/docs/en/xl-c-aix/12.1.0?topic=functions-builtin-frame-address-builtin-return-address#:~:text=If%20a%20function%20is%20inlined%2C%20the%20frame%20or%20return%20address%20corresponds%20to%20that%20of%20the%20function%20that%20is%20returned%20to.
__attribute__((noinline))
static void mark() {
    mark_from_registers();
#ifndef NGLOBALS
    mark_from_dot_data();
#endif

    volatile void **frame_addr = (volatile void **)__builtin_frame_address(0);
    mark_from_memory(frame_addr, (volatile void **)stack_base);
}

void gc() {
    // Mark used nodes.
    mark();

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
    assert(setup, "garbage collector not setup");

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

__attribute__((constructor))
static void gcinit() {
#ifdef VALGRIND
    if (RUNNING_ON_VALGRIND) {
        check_ptr = valgrind_running_check_ptr;
    }
#endif
    // The stack frame of the caller of `gcinit`.
    stack_base = __builtin_frame_address(0);
    setup = true;
}

__attribute__((destructor))
static void gccleanup() {
    for (size_t i = 0; i < blocks_len; i++) {
        if (!blocks[i].free) {
            free((void*)blocks[i].start);
            blocks[i].free = 1;
        }
    }
    free(blocks);
}
