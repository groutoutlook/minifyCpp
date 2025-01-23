#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

/**
 * @brief An example implementation of the Linux xxd tool
 *
 * @param argc
 * @param argv
 * @return int
 */
int main(int argc, char *argv[])
{
    // open file
    int file = STDIN_FILENO;
    if (argc == 2)
        file = open(argv[1], O_RDONLY);

    // initialize vars
    int good = 1;
    char buf[16];
    char *cur = buf;
    int remaining = 16;
    int r;
    int execution_num = 0;

    // main loop
    while (good)
    {
        r = read(file, cur, remaining);
        if (r == -1)
            exit(2);
        cur += r;
        remaining -= r;
        if (remaining == 0 || (r == 0 && remaining != 16))
        {
            printf("%08x    ", execution_num);
            execution_num += 16;
            for (int j = 0; j < 16; ++j)
            {
                if (j < 16 - remaining)
                    printf("%02hhx", buf[j]);
                else
                    printf("  ");
                if (j % 2 != 0)
                    printf(" ");
            }
            printf("    ");
            for (int j = 0; j < 16; ++j)
            {
                if (j < 16 - remaining)
                {
                    if (isprint(buf[j]))
                        printf("%c", buf[j]);
                    else
                        printf(".");
                }
                else
                    printf(" ");
            }
            cur = buf;
            remaining = 16;
            printf("\n");
        }
        good = r != 0;
    }
}
