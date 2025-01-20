#include <stdio.h>
#include <unistd.h>

struct myStruct
{
    int val1;
    int val2;
};

void printMyStruct(struct myStruct *pointer)
{
    printf("{myStruct, val1: %d, val2: %d}", pointer->val1, pointer->val2);
}

int main()
{
    struct myStruct m1;
    struct myStruct m2 = {
        .val1 = 3,
        .val2 = 4};
    printMyStruct(&m1);
    printMyStruct(&m2);
}
