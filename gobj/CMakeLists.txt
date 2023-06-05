##############################################
#   CMake
##############################################
project(yunetas-gobj C)
cmake_minimum_required(VERSION 3.0)
include(CheckIncludeFiles)
include(CheckSymbolExists)

set(CMAKE_INSTALL_PREFIX /yuneta/development/outputs)

set(INC_DEST_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(LIB_DEST_DIR ${CMAKE_INSTALL_PREFIX}/lib)

if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_definitions(-DDEBUG)
endif(CMAKE_BUILD_TYPE MATCHES Debug)

add_definitions(-D_GNU_SOURCE)
add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)

include_directories(/yuneta/development/outputs/include)

##############################################
#   Source
##############################################

set (SRCS
    src/00_http_parser.c
    src/ghttp_parser.c
    src/gobj.c
    src/istream.c
    src/parse_url.c
    src/environment.c
    src/log_udp_handler.c
)


set (HDRS
    src/ansi_escape_codes.h
    src/00_http_parser.h
    src/ghttp_parser.h
    src/gobj.h
    src/istream.h
    src/parse_url.h
    src/environment.h
    src/log_udp_handler.h
)

##############################################
#   Lib
##############################################
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

target_compile_options(${PROJECT_NAME} PUBLIC -Wall -std=c99 -g)
target_link_options(${PROJECT_NAME} PUBLIC LINKER:-Map=${PROJECT_NAME}.map)

##############################################
#   System install
##############################################
install(FILES ${HDRS} DESTINATION ${INC_DEST_DIR})
install(
    TARGETS ${PROJECT_NAME}
    PERMISSIONS
    OWNER_READ OWNER_WRITE
    GROUP_READ GROUP_WRITE
    WORLD_READ
    DESTINATION ${LIB_DEST_DIR}
)
