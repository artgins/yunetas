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
add_yuno_executable(${PROJECT_NAME} ${YUNO_SRCS} ${YUNO_HDRS})

target_link_libraries(${PROJECT_NAME}
    ${YUNETAS_KERNEL_LIBS}
    ${YUNETAS_EXTERNAL_LIBS}
    ${YUNETAS_PCRE_LIBS}
    ${OPENSSL_LIBS}
    ${MBEDTLS_LIBS}
    ${DEBUG_LIBS}
    panel.a
    ncurses.a
)

##############################################
#   Installation
##############################################
install(
    TARGETS ${PROJECT_NAME}
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${BIN_DEST_DIR}
)

# compile in Release mode :
#
#     cmake -DCMAKE_BUILD_TYPE=Release ..
#
# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
# compile to build static binary :
#
#     cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/yuneta/development/yunetas/tools/cmake/musl-toolchain.cmake ..
#
#
