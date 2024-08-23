##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
get_filename_component(current_directory_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)

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
