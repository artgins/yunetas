##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(yuneta_cli C)
include(CheckIncludeFiles)
include(CheckSymbolExists)

set(CMAKE_INSTALL_PREFIX /yuneta/development/output)

set(BIN_DEST_DIR ${CMAKE_INSTALL_PREFIX}/agent)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99")

if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_definitions(-DDEBUG)

  # TEST para probar en kdevelop
#   add_definitions(-DTEST_KDEVELOP)
  # TEST para probar en kdevelop

  option(SHOWNOTES "Show preprocessor notes" OFF)

  if(CMAKE_COMPILER_IS_GNUCC)
    # GCC specific debug options
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g3 -ggdb3 -gdwarf-2")
    set(AVOID_VERSION -avoid-version)
  endif(CMAKE_COMPILER_IS_GNUCC)
endif(CMAKE_BUILD_TYPE MATCHES Debug)

add_definitions(-D_GNU_SOURCE)
add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)

# check_include_files(ncurses.h HAVE_NCURSES_H)
# if(HAVE_NCURSES_H)
#     add_definitions(-DHAVE_NCURSES_H)
# else(HAVE_NCURSES_H)
#     message("***  WARNING: GNU C ncurses library not found.")
# endif(HAVE_NCURSES_H)

include_directories(/yuneta/development/output/include)

##############################################
#   Source
##############################################

SET (YUNO_SRCS
    src/main.c
    src/yuno_yuneta_cli.c
    src/c_cli.c
    src/c_wn_stdscr.c
    src/c_wn_layout.c
    src/c_wn_box.c
    src/c_wn_stsline.c
    src/c_wn_editline.c
    src/c_wn_list.c
    src/c_wn_toolbar.c
    src/c_wn_static.c
)
SET (YUNO_HDRS
    src/yuno_yuneta_cli.h
    src/c_cli.h
    src/c_wn_stdscr.h
    src/c_wn_layout.h
    src/c_wn_box.h
    src/c_wn_stsline.h
    src/c_wn_editline.h
    src/c_wn_list.h
    src/c_wn_toolbar.h
    src/c_wn_static.h
)

##############################################
#   yuno
##############################################
ADD_EXECUTABLE(yuneta ${YUNO_SRCS} ${YUNO_HDRS})
SET_TARGET_PROPERTIES(yuneta PROPERTIES OUTPUT_NAME yuneta)

TARGET_LINK_LIBRARIES(yuneta
    /yuneta/development/output/lib/libyuneta-tls.a
    /yuneta/development/output/lib/libyuneta-core.a
    /yuneta/development/output/lib/libginsfsm.a
    /yuneta/development/output/lib/libytls.a
    /yuneta/development/output/lib/libghelpers.a
    /yuneta/development/output/lib/libuv.a
    /yuneta/development/output/lib/libjwt.a
    /yuneta/development/output/lib/libjansson.a
    /yuneta/development/output/lib/libunwind.a
    /yuneta/development/output/lib/libpcre2-8.a
    /yuneta/development/output/lib/libpanel.a
    /yuneta/development/output/lib/libncurses.a
    /yuneta/development/output/lib/libssl.a
    /yuneta/development/output/lib/libcrypto.a

    pthread dl  # used by libuv
    lzma        # used by libunwind
    z rt m
    uuid
    util
)


##############################################
#   Installation
##############################################
install(
    TARGETS yuneta
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${BIN_DEST_DIR}
)
