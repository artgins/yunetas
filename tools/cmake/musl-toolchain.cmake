##############################################
#   musl-toolchain.cmake
##############################################

# Avoid
# -march=native in cross-compilation
# -ffast-math if you need IEEE-compliant math
# -fomit-frame-pointer if you need accurate stack traces

set(AS_STATIC ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER /usr/bin/musl-gcc)

set(CMAKE_C_FLAGS_RELEASE "-O3 \
    -fno-plt -fomit-frame-pointer -ffast-math -fstrict-aliasing \
    -fno-stack-protector -DNDEBUG")

set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-static -no-pie -Wl,--strip-all")

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-static")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(BUILD_SHARED_LIBS OFF)
