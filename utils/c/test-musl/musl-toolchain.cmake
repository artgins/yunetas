# Change to your actual musl-gcc path
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_C_COMPILER /usr/bin/musl-gcc)
set(CMAKE_C_FLAGS "-static -no-pie")
set(CMAKE_EXE_LINKER_FLAGS "-static -no-pie -Wl,-Bstatic")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
