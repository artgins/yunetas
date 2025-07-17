##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(yuneta_cli C)

#-----------------------------------------------------#
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#-----------------------------------------------------#
if(DEFINED ENV{YUNETAS_BASE})
    set(YUNETAS_BASE $ENV{YUNETAS_BASE})
else()
    set(YUNETAS_BASE "/yuneta/development/yunetas")
endif()
include("${YUNETAS_BASE}/tools/cmake/project.cmake")

# check_include_files(ncurses.h HAVE_NCURSES_H)
# if(HAVE_NCURSES_H)
#     add_definitions(-DHAVE_NCURSES_H)
# else(HAVE_NCURSES_H)
#     message("***  WARNING: GNU C ncurses library not found.")
# endif(HAVE_NCURSES_H)

##############################################
#   Source
##############################################

SET (YUNO_SRCS
    src/main.c
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
