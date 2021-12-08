#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
    int *p = malloc(10 * sizeof(int));
    memset(p, 0xab, 10 * sizeof(int));
    int *b = malloc(sizeof(int));
    *b = 20;
    printf("*p = %x\n", *p);
    printf("*b = %d\n", *b);
    return 0;
}
