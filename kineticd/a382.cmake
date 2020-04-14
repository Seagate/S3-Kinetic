# Specify the cross compiler
SET(CMAKE_C_COMPILER   arm-marvell-linux-gnueabi-gcc)
SET(CMAKE_CXX_COMPILER arm-marvell-linux-gnueabi-g++)
set(CMAKE_RANLIB arm-marvell-linux-gnueabi-ranlib)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

