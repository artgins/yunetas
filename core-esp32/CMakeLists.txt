##############################################
#   CMake
##############################################
project(yunetas-esp32 C)
cmake_minimum_required(VERSION 3.0)
include(CheckIncludeFiles)
include(CheckSymbolExists)

set(CMAKE_INSTALL_PREFIX /yuneta/development/outputs)

set(INC_DEST_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(LIB_DEST_DIR ${CMAKE_INSTALL_PREFIX}/lib)

if(CMAKE_BUILD_TYPE MATCHES Debug)
  add_definitions(-DDEBUG)
endif(CMAKE_BUILD_TYPE MATCHES Debug)

add_definitions(-D_GNU_SOURCE)
add_definitions(-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64)

# find_package(PostgreSQL REQUIRED)

include_directories(/yuneta/development/outputs/include)

##############################################
#   Source
##############################################

set (SRCS
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_ethernet.c
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_transport.c
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_uart.c
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_wifi.c
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_yuno.c
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_timer.c

    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_c_prot/src/c_prot_http_cli.c
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_c_prot/src/c_prot_modbus_master.c
)


set (HDRS
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_ethernet.h
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_transport.h
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_uart.h
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_wifi.h
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_esp_yuno.h
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_yuneta/src/c_timer.h

    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_c_prot/src/c_prot_http_cli.h
    /yuneta/development/yuneta/yunetas/core-esp32/components/esp_c_prot/src/c_prot_modbus_master.h
)


##############################################
#   Lib
##############################################
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

target_compile_options(${PROJECT_NAME} PUBLIC -Wall -std=c99 -g)
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
