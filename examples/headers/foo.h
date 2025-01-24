struct foostruct
{
    int val;
    struct barstruct
    {
        int otherVal;
        struct foostruct *f;
    } b;
    struct barstruct *c;
};

int sum(struct foostruct *f);

int a(int b, int c);