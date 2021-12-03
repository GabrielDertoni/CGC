#include <stdio.h>

#include "../cgc.h"

int main(int argc, char *argv[]) {
    SETUP_GC(argv);

    int **ptr;
    ptr = (int **)gcmalloc(sizeof(int*));
    *ptr = (int *)gcmalloc(sizeof(int));
    **ptr = 10;

    printf("ptr = %p\n", ptr);
    printf("*ptr = %p\n", *ptr);
    printf("**ptr = %d\n", **ptr);

    return 0;
}

