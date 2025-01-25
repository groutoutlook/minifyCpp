#include <stdio.h>
#include <unistd.h>

struct myStruct
{
    double d1, d2;
    int val1;
    int val2;
    int *ptr;
};
struct myOtherStruct
{
    int val1;
};
struct myThirdStruct
{
    int val2;
};
enum myFirstEnum
{
    YES,
    NO
};
enum mySecondEnum
{
    WOOH,
    NAAAH
};
enum lastEnum
{
    NOOOOO,
    WOOOOOOH
};

void printMyStruct(struct myStruct *pointer)
{
    printf("{myStruct, val1: %d, val2: %d}", pointer->val1, pointer->val2);
}
void sum_two(int a, int b)
{
    int c = a + b;
    printf("The sum is %d\n", c);
}

int main()
{
    int a = 1;
    int b = 2;
    struct myStruct m1;
    struct myStruct m2 = {
        .val1 = a + b,
        .val2 = 42,
        .d1 = 0.2,
    };
    const struct myStruct m3 = {};
    struct myStruct m4 = {
        .ptr = 0};
    struct myStruct arr[3] = {[1].d1 = 33.3};
    printMyStruct(&m1);
    printMyStruct(&m2);
    if (m2.val1 == 3)
    {
        int cool = 5;
        printf("Cool!! %d\n", cool);
        sum_two(cool, a);
    }
    else
    {
        int other = 2;
        printf("Oh no... %d\n", other);
        sum_two(other, b);
    }
}
