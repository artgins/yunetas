##############################################
#   CMake
##############################################
#
# This file is to compile esp32 firmware in linux environment
# TODO cuando esté transport y uart en linux ya no hará falta
#
cmake_minimum_required(VERSION 3.5)
project(yunetas-esp32 C)

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
    components/esp_yuneta/src/c_esp_ethernet.c
    components/esp_yuneta/src/c_esp_transport.c
    components/esp_yuneta/src/c_esp_uart.c
    components/esp_yuneta/src/c_esp_wifi.c
#    components/esp_yuneta/src/c_esp_yuno.c
#    components/esp_yuneta/src/c_timer.c

#    components/esp_c_prot/src/c_prot_http_cli.c
#    components/esp_c_prot/src/c_prot_modbus_master.c
)


set (HDRS
    components/esp_yuneta/src/c_esp_ethernet.h
    components/esp_yuneta/src/c_esp_transport.h
    components/esp_yuneta/src/c_esp_uart.h
    components/esp_yuneta/src/c_esp_wifi.h
    components/esp_yuneta/src/c_esp_yuno.h
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
