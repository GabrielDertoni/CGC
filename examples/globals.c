#include <stdio.h>
#include <string.h>

#define GC_INTERVAL 0
#include "../cgc.h"

// This variable will most likely not be in the stack.
int *ptr;

int main() {
    ptr = (int*)gcmalloc(sizeof(int) * 9000);
    memset(ptr, 0, 9000 * sizeof(int));
    gc();
    printf("&ptr = %p\n", &ptr);
    printf("*ptr = %d\n", *ptr);
    return 0;
}

