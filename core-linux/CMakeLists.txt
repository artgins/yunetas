##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
include(/yuneta/development/yunetas/tools/cmake/project.cmake)
project(yunetas-core-linux C)

##############################################
#   Source
##############################################
set (SRCS
    src/c_linux_yuno.c
    src/c_timer.c
    src/c_linux_transport.c
    src/c_linux_uart.c
    src/yunetas_environment.c
    src/yunetas_ev_loop.c
)

set (HDRS
    src/c_linux_yuno.h
    src/c_timer.h
    src/c_linux_transport.h
    src/c_linux_uart.h
    src/yunetas_environment.h
    src/yunetas_ev_loop.h
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
