#ifndef _CGC_H_
#define _CGC_H_

#include <stdlib.h>

#ifndef GC_INTERVAL
#define GC_INTERVAL 8
#endif

// FIXME: Is this how you do it?
#pragma pack(8)

void gc();
volatile void *gcmalloc(size_t size);
volatile void *gccalloc(size_t nmemb, size_t size);
volatile void *gcrealloc(void *ptr, size_t size);

#endif
