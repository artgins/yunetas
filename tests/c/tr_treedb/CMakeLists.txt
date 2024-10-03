##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(test_tr_treedb C)
include(CheckIncludeFiles)
include(CheckSymbolExists)

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

include_directories(/yuneta/development/output/include)

##############################################
#   Source
##############################################

SET (YUNO_SRCS
    test_tr_treedb.c
    test_departments.c
    test_users.c
    test_final_foto.c
    test_compound.c
    test_final_foto2.c
)
SET (YUNO_HDRS
)

##############################################
#   yuno
##############################################
ADD_EXECUTABLE(test_tr_treedb ${YUNO_SRCS} ${YUNO_HDRS})

TARGET_LINK_LIBRARIES(test_tr_treedb
    /yuneta/development/output/lib/libghelpers.a
    /yuneta/development/output/lib/libjansson.a
    /yuneta/development/output/lib/libuv.a
    /yuneta/development/output/lib/libpcre2-8.a
    /yuneta/development/output/lib/libunwind.a
    rt pthread dl lzma m uuid
)

##############################################
#   Installation
##############################################
install(
    TARGETS test_tr_treedb
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${BIN_DEST_DIR}
)
