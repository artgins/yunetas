#set(CMAKE_SYSTEM_NAME Linux)
#set(CMAKE_SYSTEM_PROCESSOR x86_64)
#
## Specify the cross compiler
#set(CMAKE_C_COMPILER musl-gcc)
#set(CMAKE_CXX_COMPILER musl-g++)
#
## Search for programs in the build host directories
#set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
## Search for libraries and headers in the target directories
#set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
#set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
#
## Static linking
#set(CMAKE_EXE_LINKER_FLAGS "-static")
#set(CMAKE_SHARED_LINKER_FLAGS "-static")

# musl-toolchain.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER musl-gcc)
set(CMAKE_CXX_COMPILER musl-g++)
set(CMAKE_EXE_LINKER_FLAGS "-static")
