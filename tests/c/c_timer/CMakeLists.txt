##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(test_c_timer C)

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
SET (YUNO_SRCS
    src/main.c
    src/c_test_timer.c
)
SET (YUNO_HDRS
    src/c_test_timer.h
)

##############################################
#   Binary
##############################################
add_executable(${PROJECT_NAME} ${YUNO_SRCS} ${YUNO_HDRS})
target_link_libraries(${PROJECT_NAME}
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

    pthread dl  # used by openssl
    uuid
    bfd     # to stacktrace
    m
)

##############################################
#   Test
##############################################
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

##############################################
#   Installation
##############################################
#install(
#    TARGETS ${PROJECT_NAME}
#    PERMISSIONS
#    OWNER_READ OWNER_WRITE OWNER_EXECUTE
#    GROUP_READ GROUP_WRITE GROUP_EXECUTE
#    WORLD_READ WORLD_EXECUTE
#    DESTINATION ${YUNOS_DEST_DIR}
#)

# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# or compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
# Compilando para ARM poner cmake -DNOT_INCLUDE_LIBUNWIND
#
