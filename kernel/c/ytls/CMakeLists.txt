##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(ytls C)

#-----------------------------------------------------#
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#-----------------------------------------------------#
if(DEFINED ENV{YUNETAS_BASE})
  set(YUNETAS_BASE $ENV{YUNETAS_BASE})
else()
  set(YUNETAS_BASE "/yuneta/development/yunetas")
endif()
include("${YUNETAS_BASE}/tools/cmake/project.cmake")

##############################################
#   Source
##############################################

set (SRCS
    src/ytls.c
    src/tls/openssl.c
    src/tls/mbedtls.c
)


set (HDRS
    src/ytls.h
)


##############################################
#   Lib
##############################################
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

target_link_options(${PROJECT_NAME} PUBLIC LINKER:-Map=${PROJECT_NAME}.map)

##############################################
#   System install
##############################################
install(FILES ${HDRS} DESTINATION ${INC_DEST_DIR})

install(
    TARGETS ${PROJECT_NAME}
    PERMISSIONS
    OWNER_READ OWNER_WRITE
    GROUP_READ GROUP_WRITE
    WORLD_READ
    DESTINATION ${LIB_DEST_DIR}
)
