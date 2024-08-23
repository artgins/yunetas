##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(yunetas-c_prot C)

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
set (SRCS
    # Protocols
    src/msg_ievent.c
    src/c_prot_http_cli.c
    src/c_prot_modbus_master.c
    src/c_ievent_cli.c
    src/c_prot_tcp4h.c
    src/c_ota.c
)


set (HDRS
    # Protocols
    src/msg_ievent.h
    src/c_prot_http_cli.h
    src/c_prot_modbus_master.h
    src/c_ievent_cli.h
    src/c_prot_tcp4h.h
    src/c_ota.h
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

##############################################
#   sub-projects
##############################################
