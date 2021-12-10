#ifndef _CGC_H_
#define _CGC_H_

#include <stdlib.h>

#ifndef GC_INTERVAL
#define GC_INTERVAL 8
#endif

#define GC_INIT() gcinit(__builtin_frame_address(0));

void gcinit(void* frame_address);
void gc();
volatile void *gcmalloc(size_t size);
volatile void *gccalloc(size_t nmemb, size_t size);
volatile void *gcrealloc(void *ptr, size_t size);

#ifdef GC_EXTERN_ALLOC

extern void*(*__gc_sysmalloc)(size_t);
extern void*(*__gc_syscalloc)(size_t, size_t);
extern void*(*__gc_sysrealloc)(void*, size_t);
extern void (*__gc_sysfree)(void*);

#else

#define __gc_sysmalloc  malloc
#define __gc_syscalloc  calloc
#define __gc_sysrealloc realloc
#define __gc_sysfree    free

#endif

#endif
