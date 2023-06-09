cmake_minimum_required(VERSION 3.16)
set (CMAKE_C_COMPILER /usr/bin/clang)
include(CheckIncludeFiles)
include(CheckSymbolExists)

set (CMAKE_ENABLE_EXPORTS TRUE)

set(CMAKE_INSTALL_PREFIX /yuneta/development/outputs)

set(INC_DEST_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(LIB_DEST_DIR ${CMAKE_INSTALL_PREFIX}/lib)
set(BIN_DEST_DIR ${CMAKE_INSTALL_PREFIX}/bin)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG)
endif(CMAKE_BUILD_TYPE MATCHES Debug)

add_definitions(-D_GNU_SOURCE)
add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)

include_directories(/yuneta/development/outputs/include)
set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /yuneta/development/outputs/lib)

#gcc -fno-pie -no-pie -g3 -o $@ stacktrace_demo.c -lbfd -ldl

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_options(-std=c99 -Wall -g3)
else()
    add_compile_options(-std=c99 -Wall -O)
endif()

add_link_options(-no-pie)   # to stacktrace with bfd
