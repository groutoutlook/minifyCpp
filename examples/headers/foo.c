#include "foo.h"

int sum(struct foostruct *f)
{
    return f->val + f->b.otherVal + (f->b.f ? sum(f->b.f) : 0);
}
int a(int b, int c)
{
    return b + c;
}