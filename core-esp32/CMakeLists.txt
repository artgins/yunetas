##############################################
#   CMake
##############################################
#
# This file is to compile esp32 firmware in linux environment
#
cmake_minimum_required(VERSION 3.16)
include(/yuneta/development/yuneta/yunetas/tools/cmake/project.cmake)
project(yunetas-esp32 C)

##############################################
#   Source
##############################################
set (SRCS
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_ethernet.c
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_transport.c
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_uart.c
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_wifi.c
#    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_yuno.c
#    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_timer.c

#    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_c_prot/src/c_prot_http_cli.c
#    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_c_prot/src/c_prot_modbus_master.c
)


set (HDRS
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_ethernet.h
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_transport.h
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_uart.h
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_wifi.h
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_yuno.h
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
