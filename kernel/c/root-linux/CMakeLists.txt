##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(yunetas-core-linux C)

#-----------------------------------------------------#
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#-----------------------------------------------------#
if(DEFINED ENV{YUNETAS_BASE})
    set(YUNETAS_BASE_DIR $ENV{YUNETAS_BASE})
else()
    set(YUNETAS_BASE_DIR "/yuneta/development/yunetas")
endif()
include("${YUNETAS_BASE_DIR}/tools/cmake/project.cmake")

##############################################
#   Source
##############################################
set (SRCS
    src/c_linux_yuno.c
    src/c_timer.c
    src/c_linux_transport.c
    src/c_linux_uart.c
    src/c_authz.c
    src/c_task_authenticate.c
    src/c_task.c
    src/yunetas_environment.c
    src/yunetas_ev_loop.c
    src/fs_watcher.c
    src/dbsimple.c
    src/entry_point.c
    src/yunetas_register.c
    src/ydaemon.c
    src/rotatory.c
)

set (HDRS
    src/c_linux_yuno.h
    src/c_timer.h
    src/c_linux_transport.h
    src/c_linux_uart.h
    src/c_authz.h
    src/c_task_authenticate.h
    src/c_task.h
    src/yunetas_environment.h
    src/yunetas_ev_loop.h
    src/fs_watcher.h
    src/dbsimple.h
    src/entry_point.h
    src/yunetas_register.h
    src/ydaemon.h
    src/rotatory.h

    src/yunetas.h
)


##############################################
#   Lib
##############################################
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

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


# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# or compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
