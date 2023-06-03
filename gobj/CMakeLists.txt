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

set(CMAKE_C_STANDARD 99)

if(LINUX OR UNIX OR CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()

IF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
ELSE()
  add_definitions(-DNOT_INCLUDE_LIBUNWIND=1)
ENDIF()

if(WIN32)
  add_definitions(-DNOT_INCLUDE_LIBUNWIND)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_definitions(-DDEBUG)
  option(SHOWNOTES "Show preprocessor notes" OFF)

  if(CMAKE_COMPILER_IS_GNUCC)
    # GCC specific debug options
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g3 -ggdb3 -gdwarf-2")
    set(AVOID_VERSION -avoid-version)
  endif(CMAKE_COMPILER_IS_GNUCC)
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
)


set (HDRS
    src/00_http_parser.h
    src/ansi_escape_codes.h
    src/environment.h
    src/ghttp_parser.h
    src/gobj.h
    src/istream.h
    src/parse_url.h
)

##############################################
#   Lib
##############################################
add_library(yunetas-gobj ${SRCS} ${HDRS})
set_target_properties(yunetas-gobj
    PROPERTIES COMPILE_DEFINITIONS "__SECURITY_AMICABLE_LEVEL__=1"
)


##############################################
#   System install
##############################################
install(FILES ${HDRS} DESTINATION ${INC_DEST_DIR})
install(
    TARGETS yunetas-gobj
    PERMISSIONS
    OWNER_READ OWNER_WRITE
    GROUP_READ GROUP_WRITE
    WORLD_READ
    DESTINATION ${LIB_DEST_DIR}
)
