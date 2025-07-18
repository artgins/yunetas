##############################################
#   musl-toolchain.cmake
##############################################
set(AS_STATIC ON)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER /usr/bin/musl-gcc)
set(CMAKE_C_FLAGS "-Wall -Wextra -Wno-type-limits -Wno-sign-compare -Wno-unused-parameter -funsigned-char")
set(CMAKE_EXE_LINKER_FLAGS "-static -Wl,-Bstatic")
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-static")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(BUILD_SHARED_LIBS OFF)
