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
    test_yevent_listen1
    test_yevent_listen2
    test_yevent_listen3
    test_yevent_listen4
    test_yevent_connect1
    test_yevent_connect2
    test_yevent_traffic1
    test_yevent_traffic2
    test_yevent_traffic3
    test_yevent_traffic4
    test_yevent_traffic5
    test_yevent_traffic6

    test_yevent_timer_once1
    test_yevent_timer_once2
    test_yevent_timer_periodic1
)

##############################################
#   Tests
##############################################
foreach(test ${SRCS})
    set(binary "${test}")
    add_executable(${binary} "${test}.c")

    target_link_libraries(${binary}
        libyunetas-c_prot.a
        libyunetas-core-linux.a
        libtimeranger2.a
        libyev_loop.a
        libytls.a
        libyunetas-gobj.a

        libjansson.a
        liburing.a
        libpcre2-8.a
        libbacktrace.a
        libcrypto.a

        m
    )
    add_test("${current_directory_name}/${test}" ${binary})

endforeach()
