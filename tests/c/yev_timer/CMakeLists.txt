##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(yev_timer C)

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
    yev_timer.c
)
SET (YUNO_HDRS
)

##############################################
#   yuno
##############################################
add_executable(${PROJECT_NAME} ${YUNO_SRCS} ${YUNO_HDRS})

target_link_libraries(${PROJECT_NAME}
    libyunetas-core-linux.a
    libyunetas-gobj.a

    libjansson.a
    liburing.a
    libpcre2-8.a
    m
    uuid
    bfd     # to stacktrace
)

target_link_options(${PROJECT_NAME} PUBLIC LINKER:-Map=${PROJECT_NAME}.map)

##############################################
#   Installation
##############################################
install(
    TARGETS ${PROJECT_NAME}
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${BIN_DEST_DIR}
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})

# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# or compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
