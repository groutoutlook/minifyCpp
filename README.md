# minify-C

Ever wanted to minify your C code? Now you can. This repository provides a code-minifier for
programs written in C. Simply build the repository and run the executable `minifier` on
your source C file, and the program will produce a minified C file on stdout.

Alternatively, add the -i flag in order to apply the changes in place.

Note: as of now, you must provide the include directories as extra arguments to the executable.
For instance:

```sh
minifier myFile.c -- -I /usr/lib/clang/17/include
```

## Features

- Removes whitespace in between symbols
- Renames all structs, fields, enums, typedefs, and variables in the main file to the first available symbol.
- Edit in-place or print edits to stdout

## Example

Given a file containing

```c
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
```

The current output is the following:

```c
#define m int
#define l printf(
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<ctype.h>
m main(m a,char*b[]){m c=STDIN_FILENO;if(a==2)c=open(b[1],O_RDONLY);m d=1;char e[16];char*f=e;m g=16;m h;m i=0;while(d){h=read(c,f,g);if(h==-1)exit(2);f+=h;g-=h;if(g==0||(h==0&&g!=16)){l "%08x    ",i);i+=16;for(m j=0;j<16;++j){if(j<16-g)l "%02hhx",e[j]);else l "  ");if(j%2!=0)l " ");}l "    ");for(m k=0;k<16;++k){if(k<16-g){if(isprint(e[k]))l "%c",e[k]);else l ".");}else l " ");}f=e;g=16;l "\n");}d=h!=0;}}
```

## Building Natively

In order to build the executable natively, you'll need to have the following packages
installed on your system:

- libclang-17-dev
- clang-17
- llvm-17-dev

You'll also need a working C++ compiler.

If you have all of these, you can run the below to configure and make the project.

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel -DLLVMVersion=17
cmake --build .
```

After running the above, there should be an executable `minifier` in the `build` directory.

If you only want to run the executable, the runtime dependencies can all be installed by installing
the following package:

- libllvm17

## Building/Running with Docker

You can also build and run the executable with Docker. Simply run the below command to build
the docker image.

```sh
docker build --tag=minifier .
```

Then, you can run the executable with the following command. Output will be on stdout.

```sh
docker run -i minifier < myFile.c
```

## Motivation

Why minify C code? Unlike Javascrpt and HTML where the source code must be sent over the web,
making code size actually important, source code size has no effect on C programs. After all,
everything gets compiled down into the same bytecode. However, some UC professors have been
known to offer EC assignments that ask their students to "golf" their code (use the minimum
number of bytes possible to implement their program). While this repository won't optimize
your code by changing its structure, it will at least minify any existing code by removing
any spaces and replacing repeated patterns with defines (when it's worth it).

## Design

The design is quite simple. First, we make a pass over the input and convert all variables
into minimum-size variables.

Then, we do a pass over this output to look for patterns of repeated tokens. If a pattern satisfies
the equation `N*L < 9+X+L`, where N is the number of appearances, L is the length of the pattern,
in bytes, and X is the length of the next available identifier, then a define is added to the file
and all occurrences of the pattern are replaced with the identifier assigned to the pattern.

Finally, we do a pass over the tokens to remove spaces where applicable.

## Known Limitations

- minify-C is meant for minimizing a single source C file. It will not work with C++.
- While minify-C can properly handle includes, there is currently no support for multi-file minimization.
- Macros that declare variables may end up conflicting with minified variables
- Macros that are used to reference different variables across
  their lifetime will cause the program to crash. Use the `--expand-all` flag for this.
- Large files may take a long time to process due to define macro addition. If minimizing is taking too long, try using the `--no-add-macros` flag.
