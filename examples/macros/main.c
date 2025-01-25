#include <stdio.h>
#define a 324
#define f(a) printf( \
    "hi %s\n", #a)
int main()
{
    int b = 3; // this is a comment
#define d
    printf("a+b = %d\n", a + b);
    for (int i = 0; i < 10; ++i)
    {
        int tmp = i * i;
        int last = tmp * tmp;
        printf("hello %d\n", last);
    }
}