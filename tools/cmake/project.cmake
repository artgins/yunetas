cmake_minimum_required(VERSION 3.5)

include(CheckIncludeFiles)
include(CheckSymbolExists)

set (CMAKE_ENABLE_EXPORTS TRUE)

set(CMAKE_INSTALL_PREFIX /yuneta/development/outputs)

set(INC_DEST_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(LIB_DEST_DIR ${CMAKE_INSTALL_PREFIX}/lib)
set(BIN_DEST_DIR ${CMAKE_INSTALL_PREFIX}/bin)

add_definitions(-D_GNU_SOURCE)
add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)

include_directories(/yuneta/development/outputs/include)
set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /yuneta/development/outputs/lib)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG)
    add_compile_options(-std=c99 -Wall -Wextra -g3 -fno-pie -fno-stack-protector -Wno-unused-parameter -fPIC)
    add_link_options(-no-pie)
else()
    add_compile_options(-std=c99 -Wall -Wextra -g3 -O -fno-pie -fno-stack-protector -Wno-unused-parameter -fPIC)
    add_link_options(-no-pie)
endif()

#if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
#    MESSAGE("=================> Clang")
#else()
#    MESSAGE("=================> NOT CLang")
#endif()
#
#MESSAGE(STATUS "DIR ${CMAKE_CURRENT_SOURCE_DIR}")
#MESSAGE(STATUS "COMPILER_ID ${CMAKE_C_COMPILER_ID}")
#MESSAGE(STATUS "COMPILER ${CMAKE_C_COMPILER}")
