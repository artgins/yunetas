# musl-toolchain.cmake
message(STATUS "Using MUSL compiler")

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER /usr/bin/musl-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/musl-g++)

# Optional: Set sysroot if using a musl-cross toolchain
#set(CMAKE_SYSROOT /path/to/musl/sysroot)

# Optional: Static linking
set(CMAKE_EXE_LINKER_FLAGS "-static")
