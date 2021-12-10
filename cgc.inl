#ifndef _AUTO_CGC_H_
#define _AUTO_CGC_H_

#include <stdlib.h>

#ifndef GC_INTERVAL
#define GC_INTERVAL 8
#endif

#define __gc_sysmalloc  malloc
#define __gc_syscalloc  calloc
#define __gc_sysrealloc realloc
#define __gc_sysfree    free

#define _CGC_H_
#define VALGRIND
#include "cgc.c"

#define malloc(...)  (void*)gcmalloc(__VA_ARGS__)
#define calloc(...)  (void*)gccalloc(__VA_ARGS__)
#define realloc(...) (void*)gcrealloc(__VA_ARGS__)
#define free(...)

#endif
