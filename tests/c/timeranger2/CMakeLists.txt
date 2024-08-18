##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
include(/yuneta/development/yunetas/tools/cmake/project.cmake)
get_filename_component(current_directory_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)

##############################################
#   Source
##############################################
set(SRCS
    test_timeranger2.c
)

##############################################
#   Tests
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
    target_link_options(${binary} PUBLIC LINKER:-Map=${PROJECT_NAME}.map)

    add_test("${current_directory_name}/${test}" ${binary})

endforeach()
