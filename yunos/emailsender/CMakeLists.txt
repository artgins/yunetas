##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(emailsender C)
include(CheckIncludeFiles)
include(CheckSymbolExists)

set(CMAKE_INSTALL_PREFIX /yuneta/development/output)

set(INC_DEST_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(LIB_DEST_DIR ${CMAKE_INSTALL_PREFIX}/lib)
set(BIN_DEST_DIR ${CMAKE_INSTALL_PREFIX}/yunos)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99")

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

#
## Set this search path for cmake, to find our custom search modules
#
# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
# find_package(LIBIDN REQUIRED)

include_directories(/yuneta/development/output/include)
set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /yuneta/development/output/lib)

##############################################
#   Source
##############################################

SET (YUNO_SRCS
    src/main.c
    src/yuno_emailsender.c
    src/c_emailsender.c
    src/c_curl.c
)
SET (YUNO_HDRS
    src/yuno_emailsender.h
    src/c_emailsender.h
    src/c_curl.h
)

##############################################
#   yuno
##############################################
ADD_EXECUTABLE(emailsender ${YUNO_SRCS} ${YUNO_HDRS})

TARGET_LINK_LIBRARIES(emailsender
    /yuneta/development/output/lib/libyuneta-core.a
    /yuneta/development/output/lib/libginsfsm.a
    /yuneta/development/output/lib/libghelpers.a
    /yuneta/development/output/lib/libuv.a
    /yuneta/development/output/lib/libjansson.a
    /yuneta/development/output/lib/libunwind.a
    /yuneta/development/output/lib/libpcre2-8.a
    /yuneta/development/output/lib/libcurl.a
    /yuneta/development/output/lib/libssl.a
    /yuneta/development/output/lib/libcrypto.a

    pthread dl  # used by libuv
    lzma        # used by libunwind

    dl z
    uuid
    m
    util
)

# if(LIBIDN_FOUND)
#     target_link_libraries(${PROJECT_NAME} ${LIBIDN_LIBRARIES})
# endif()

##############################################
#   Installation
##############################################
install(
    TARGETS emailsender
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${BIN_DEST_DIR}
)
