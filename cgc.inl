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

#define malloc    gcmalloc
#define calloc    gccalloc
#define realloc   gcrealloc
#define free(...)

#endif
