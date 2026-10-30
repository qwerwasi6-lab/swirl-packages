/* hello.c - run it with:  tcc -run /home/user/tcc/hello.c */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

int main(void) {
    printf("Hello from TinyCC on SwirlOS!\n");
    for (int i = 0; i < 10; i++) printf("fib(%d) = %d\n", i, fib(i));
    char *p = malloc(32);
    strcpy(p, "heap works too");
    printf("%s (%d chars)\n", p, (int)strlen(p));
    free(p);
    return 0;
}
