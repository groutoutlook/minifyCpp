#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#define a 324
#define f(a) printf( \
    "hi %s\n", #a)
#define g (a)
#define myVar b + c
void print_int(int num)
{
    int b = 3;
    int c = 4;
    printf("Got int %d\n", num + myVar);
}
int main()
{
    int b = 3; // this is a comment
#ifdef O_APPEND
    int f = O_APPEND;
#else
    int gf = 31;
#endif

    int c = 3;
    print_int g;
#define d
    printf("a+b = %d\n", a);
    for (int i = 0; i < 10; ++i)
    {
        int tmp = i * i;
        int last = tmp * tmp;
        printf("hello %d\n", last + myVar);
    }
}