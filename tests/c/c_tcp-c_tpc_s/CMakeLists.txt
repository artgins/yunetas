##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(test_c_tcp C)
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
    test1.c
    pepon_teston.c
)

SET(COMMON
    common/c_pepon.c
    common/c_teston.c
)

##############################################
#   Tests
##############################################
foreach(test ${SRCS})
    get_filename_component(binary ${test} NAME_WE)
    add_executable(${binary} ${test} ${COMMON})

    target_link_libraries(${binary}
        libyunetas-c_prot.a
        libyunetas-core-linux.a
        libtimeranger2.a
        libytls.a
        libyunetas-gobj.a

        libjansson.a
        liburing.a
        libjwt.a
        libpcre2-8.a
        libssl.a
        libcrypto.a

        uuid
        bfd     # to stacktrace
        m
    )
    add_test("${current_directory_name}/${test}" ${binary})

endforeach()
