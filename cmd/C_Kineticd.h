#ifndef C_KINETICD_H_ 
#define C_KINETICD_H_ 

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void* allocArgv(int argc);
void initKinetic(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif

#endif



