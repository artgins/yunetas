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

#------------------------------------------------------#
#           Source
#------------------------------------------------------#
set(srcs
    src/c_esp_transport.c
    src/c_esp_wifi.c
    src/c_esp_yuno.c
    src/c_timer.c
    src/esp_persistent.c
)

#if(CONFIG_INCLUDE_YUNETA_UART) ## source needed for define of events
    list(APPEND srcs "src/c_esp_uart.c")
#endif()

#if(CONFIG_INCLUDE_YUNETA_ETHERNET)  ## source needed for define of events
    list(APPEND srcs "src/c_esp_ethernet.c")
#endif()

set(include
    src
)

set(requires
    log
    driver
    vfs
    esp_timer
    esp_netif
    esp_event
    esp_wifi
    esp_eth
    tcp_transport
    nvs_flash
    wpa_supplicant
    esp_gobj
)

#------------------------------------------------------#
#           Register
#------------------------------------------------------#
idf_component_register(
    SRCS ${srcs}
    REQUIRES ${requires}
    INCLUDE_DIRS ${include} ${INC_DEST_DIR}
)

#if(CONFIG_YUNETA_ENABLED)
    #idf_component_optional_requires(PUBLIC esp_netif esp_nvs nvs_flash)
    #idf_component_optional_requires(PRIVATE esp_pm)
#endif()
