##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(perf_c_tcps C)
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
SET(SRCS
    test4
    test5
)

SET(COMMON
    common/c_pepon.c
    common/c_teston.c
)

##############################################
#   Tests
##############################################
foreach(test ${SRCS})
    set(binary "perf_tcps_${test}")
    add_executable(${binary} "main_${test}.c" "c_${test}.c" ${COMMON})

    target_link_libraries(${binary}
        libyunetas-c_prot.a
        libyunetas-core-linux.a
        libtimeranger2.a
        libyev_loop.a
        libytls.a
        libyunetas-gobj.a

        libjansson.a
        liburing.a
        libjwt.a
        libpcre2-8.a
        libssl.a
        libcrypto.a

        pthread dl  # used by openssl
        uuid
        bfd     # to stacktrace
        m
    )
    add_test("${current_directory_name}/${test}" ${binary})

endforeach()
