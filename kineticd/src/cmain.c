#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#ifndef __cplusplus
#define bool _Bool
#define true 1
#define false 0
#else
#define _Bool bool
#define bool bool
#define false false
#define true true
#endif

extern void* InitMain(int argc, char* argv[]);

void CInitMain(int argc, char* argv[]) {
    printf(" IN CINITMAIN \n");
    int i = 0;
    for (i=0; i < argc; i++) {
        printf(" ARGV[%d] %s\n", i, argv[i]);
    }
    InitMain(argc, argv);
}

//For stand alone kineticd, uncomment the lines below.
/*
int main(int argc, char* argv[]) {
    CInitMain(argcm argv[]);
    return 0;
}
*/
