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
    yev_timer_periodic1.c
    yev_timer_periodic2.c
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
        libtimeranger2.a
        libytls.a
        libyunetas-gobj.a

        libjansson.a
        liburing.a
        libpcre2-8.a
        m
        #z rt m
        uuid
        #util
        bfd     # to stacktrace
    )
    add_test("${current_directory_name}/${test}" ${binary})

endforeach()
