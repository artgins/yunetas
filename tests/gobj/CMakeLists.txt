##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.0)
include(/yuneta/development/yuneta/yunetas/tools/cmake/project.cmake)

##############################################
#   Source
##############################################
set(SRCS
    gobj.c
    gobj2.c
)

##############################################
#   Test Executables
##############################################
foreach(test ${SRCS})
    set(binary "${test}.bin")
    add_executable(${binary} ${test})

    target_link_libraries(${binary}
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
    get_filename_component(current_directory_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    add_test("${current_directory_name}/${test}" ${binary})

endforeach()
