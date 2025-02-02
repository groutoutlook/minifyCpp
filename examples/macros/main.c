#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#define a 324
#define f(a) printf( \
    "hi %s\n", #a)
#define g (a)
void print_int(int num)
{
    printf("Got int %d\n", num);
}
int main()
{
    int b = 3; // this is a comment
#ifdef O_APPEND
    int f = O_APPEND;
#else
    int gf = 31;
#endif

    print_int g;
#define d
    printf("a+b = %d\n", a + b);
    for (int i = 0; i < 10; ++i)
    {
        int tmp = i * i;
        int last = tmp * tmp;
        printf("hello %d\n", last);
    }
}