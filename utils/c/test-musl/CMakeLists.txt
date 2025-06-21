##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.10)
project(static_musl_test C)

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

add_executable(${PROJECT_NAME} hello.c)

# Enforce full static link flags globally
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_SEARCH_START_STATIC TRUE
    LINK_SEARCH_END_STATIC TRUE
)

# These linker flags ensure no shared libs are pulled in
target_link_options(${PROJECT_NAME} PRIVATE -static -no-pie -Wl,-Bstatic)

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
