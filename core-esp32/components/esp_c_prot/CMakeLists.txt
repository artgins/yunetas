#------------------------------------------------------#
#           Source
#------------------------------------------------------#
set(srcs
    src/msg_ievent.c
    src/c_prot_http_cli.c
    src/c_ievent_cli.c
    src/c_prot_tcp4h.c
    src/c_ota.c
)

if(CONFIG_YUNETA_MODBUS_MASTER)
    list(APPEND srcs "src/c_prot_modbus_master.c")
endif()

set(include
    src
)

set(requires
    esp_yuneta
)

#------------------------------------------------------#
#           Register
#------------------------------------------------------#
idf_component_register(
    SRCS ${srcs}
    REQUIRES ${requires}
    INCLUDE_DIRS ${include}
)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_CONFIG_H=1")
#target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format-truncation")
