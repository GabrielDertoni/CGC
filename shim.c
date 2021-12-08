/*
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

// #define GC_INTERVAL 0
// #define VALGRIND
// #include "cgc.h"

void*(*sysmalloc)(size_t)  = NULL;
void*(*syscalloc)(size_t, size_t)  = NULL;
void*(*sysrealloc)(void*, size_t) = NULL;

__attribute__((constructor))
static void init() {
    printf("HELLO\n");
    fflush(stdout);
    sysmalloc  = (void*(*)(size_t))dlsym(RTLD_NEXT, "malloc");
    syscalloc  = (void*(*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
    sysrealloc = (void*(*)(void*, size_t))dlsym(RTLD_NEXT, "realloc");
}

void *malloc(size_t size) {
    printf("HELLO\n");
    fflush(stdout);
    // return (void*)sysmalloc(size);
    return NULL;
}

void *calloc(size_t nmemb, size_t size) {
    return (void*)syscalloc(nmemb, size);
}

void *realloc(void *ptr, size_t size) {
    return (void*)sysrealloc(ptr, size);
}
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>

#define VALGRIND
#define NGLOBALS
#include "cgc.h"

#define INIT_AND_CALL(fn, ...) \
    init();                          \
    return fn##_fn(__VA_ARGS__);

char tmpbuff[1024];
unsigned long tmppos = 0;
unsigned long tmpallocs = 0;

void *memset(void*,int,size_t);
void *memmove(void *to, const void *from, size_t size);

void*(*__gc_sysmalloc)(size_t) = NULL;
void*(*__gc_syscalloc)(size_t, size_t) = NULL;
void*(*__gc_sysrealloc)(void*, size_t) = NULL;
void (*__gc_sysfree)(void*) = NULL;

static void*(*gcmalloc_fn)(size_t);
static void*(*gccalloc_fn)(size_t, size_t);
static void*(*gcrealloc_fn)(void*, size_t);
static void (*gcfree_fn)(void*);

__attribute__((constructor))
static void init() {
    __gc_sysmalloc  = (void*(*)(size_t))         dlsym(RTLD_NEXT, "malloc");
    __gc_syscalloc  = (void*(*)(size_t, size_t)) dlsym(RTLD_NEXT, "calloc");
    __gc_sysrealloc = (void*(*)(void*, size_t))  dlsym(RTLD_NEXT, "realloc");
    __gc_sysfree    = (void (*)(void*))          dlsym(RTLD_NEXT, "free");

    if (!__gc_sysmalloc || !__gc_sysfree || !__gc_syscalloc || !__gc_sysrealloc) {
        // If we used something like `fprintf` it could allocate memory which
        // would endup calling `malloc` which is not initialized.
        static const char msg[] = "Error in `dlsym`: ";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        const char *err = dlerror();
        write(STDERR_FILENO, err, strlen(err));
        write(STDERR_FILENO, "\n", 1);
        exit(EXIT_FAILURE);
    }

    gcmalloc_fn = (void*(*)(size_t))gcmalloc;
    gccalloc_fn = (void*(*)(size_t, size_t))gccalloc;
    gcrealloc_fn = (void*(*)(void*, size_t))gcrealloc;

    // Even though `gcfree_fn` takes one argument, but `gc` takes none, this
    // type cast can still be made. `gc` will just ignore the argument passed.
    gcfree_fn = gc;
}

static void *init_and_malloc(size_t size)               { INIT_AND_CALL(gcmalloc, size)        }
static void *init_and_calloc(size_t nmemb, size_t size) { INIT_AND_CALL(gccalloc, nmemb, size) }
static void *init_and_realloc(void *ptr, size_t size)   { INIT_AND_CALL(gcrealloc, ptr, size)  }
static void init_and_free(void *ptr)                    { init(); gcfree_fn(ptr);              }

static void*(*gcmalloc_fn)(size_t)          = init_and_malloc;
static void*(*gccalloc_fn)(size_t, size_t)  = init_and_calloc;
static void*(*gcrealloc_fn)(void *, size_t) = init_and_realloc;
static void (*gcfree_fn)(void*)             = init_and_free;

void *malloc(size_t size) {
    return gcmalloc_fn(size);
}

void *calloc(size_t nmemb, size_t size) {
    return gccalloc_fn(nmemb, size);
}

void *realloc(void *ptr, size_t size) {
    return gcrealloc_fn(ptr, size);
}

void free(void *ptr) {
    gcfree_fn(ptr);
}
