##############################################
#   musl-toolchain.cmake
##############################################
set(AS_STATIC ON)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER /usr/bin/musl-gcc)
set(CMAKE_EXE_LINKER_FLAGS "-static -no-pie -Wl,-Bstatic")
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-static")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(BUILD_SHARED_LIBS OFF)
