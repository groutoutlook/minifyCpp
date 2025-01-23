#include <stdio.h>
#include "foo.h"
void printYes(struct foostruct *unused)
{
    printf("Yes!\n");
}
typedef int very;
int cool;
enum myEnum
{
    YES,
    NO,
};
// int YES = 3; // bad here

int main()
{
    struct foostruct a = {
        .val = 3,
        .b = {
            .otherVal = 4}};
    struct foostruct b = {
        .val = 1,
        .b = {
            .otherVal = 2,
            .f = &a}};

    typedef struct coolstruct
    {
        void (*fn)(struct foostruct *);
    } myT;
    myT c;
    c.fn = printYes;
    c.fn(&a);
    enum myEnum val1 = YES;
    enum myEnum val2 = YES;
    static const int YES = 3;
    enum myEnum val3 = YES; // but it works here

    enum mySecondEnum
    {
        VVVVAL1,
        VVVVAL2,
    };

    printf("Sum is %d\n", sum(&b));
    return 0;
}