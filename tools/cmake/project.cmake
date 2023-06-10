cmake_minimum_required(VERSION 3.16)
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

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_options(-std=c99 -Wall -g3 -fno-pie)
    add_link_options(-no-pie)
else()
    add_compile_options(-std=c99 -Wall -O)
#    add_link_options()
endif()

# to stacktrace with bfd
if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
#    MESSAGE("=================> Clang")
#    MESSAGE(${CMAKE_CURRENT_SOURCE_DIR})
else()
#    MESSAGE("=================> GCC")
#    MESSAGE(${CMAKE_CURRENT_SOURCE_DIR})
endif()
