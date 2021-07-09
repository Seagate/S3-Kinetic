#ifndef KINETIC_C_KINETICD_H_
#define KINETIC_C_KINETICD_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void* allocArgv(int argc);
int startKineticd(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif

#endif  // KINETIC_C_KINETICD_H_



