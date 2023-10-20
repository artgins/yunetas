##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.0)
include(/yuneta/development/yuneta/yunetas/tools/cmake/project.cmake)
project(test_gobj C)

##############################################
#   Source
##############################################
set (SRCS
    test_gobj.c
)

##############################################
#   Executable
##############################################
add_executable(${PROJECT_NAME} ${SRCS})

target_link_libraries(${PROJECT_NAME}
    ${CRITERION_LIBRARIES}
    /yuneta/development/outputs/lib/libyunetas-c_prot.a
    /yuneta/development/outputs/lib/libyunetas-core-linux.a
    /yuneta/development/outputs/lib/libyunetas-gobj.a

    /yuneta/development/outputs/lib/libjansson.a
    /yuneta/development/outputs/lib/liburing.a
    m
    #z rt m
    uuid
    #util
    bfd     # to stacktrace
)

add_test (test_gobj test_gobj)
