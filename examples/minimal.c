#include <stdio.h>

#include "../cgc.h"

int main() {
    int **ptr;
    ptr = (int **)gcmalloc(sizeof(int*));
    *ptr = (int *)gcmalloc(sizeof(int));
    **ptr = 10;

    printf("ptr = %p\n", ptr);
    printf("*ptr = %p\n", *ptr);
    printf("**ptr = %d\n", **ptr);

    return 0;
}

