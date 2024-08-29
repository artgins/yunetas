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
    test_tranger_startup.c
    test_topic_integer.c
)

##############################################
#   Tests
##############################################
foreach(test ${SRCS})
    set(binary "${test}.bin")
    add_executable(${binary} ${test})

    target_link_libraries(${binary}
        libyunetas-c_prot.a
        libyunetas-core-linux.a
        libyunetas-gobj.a

        libjansson.a
        liburing.a
        m
        #z rt m
        uuid
        #util
        bfd     # to stacktrace
    )
    target_link_options(${binary} PUBLIC LINKER:-Map=${PROJECT_NAME}.map)

    add_test("${current_directory_name}/${test}" ${binary})

endforeach()
