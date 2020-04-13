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

void CInitMain(char* store_partition) {
    printf(" IN CINITMAIN\n");
    int argcc = 4;
    int i = 0;
    char dev[4];
    for (i = 0; i< 4;  i++) {
	if (store_partition[i] ==  0x00) {
		break;
	}
	if (i == 3) {
	   dev[i] = '\0';
	   break;
	} else {
	   dev[i] = store_partition[i];
	}
    }
    char store_part[50] = "--store_partition=/dev/";
    char store_dev[50] = "--store_device=/dev/";
    char *argvv[] = {(char*)"./minio",
		    strcat(store_part, store_partition),
#ifdef PRODUCT_X86
		    strcat(store_dev, dev),
#else
		    strcat(store_dev, store_partition),
#endif
		    (char*)"--metadata_db_path=./metadata.db",
		    NULL
		   };
    printf(" AFTER ARRV %s %s %s %s\n", argvv[0], argvv[1], argvv[2], argvv[3]);
    InitMain(argcc, argvv);
}

//For stand alone kineticd, uncomment the lines below.
/*
int main(int argc, char* argv[]) {
    CInitMain(argv[1]);
    return 0;
}
*/
