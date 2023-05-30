#------------------------------------------------------#
#           Source
#------------------------------------------------------#
set(srcs
    src/c_prot_http_cli.c
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
    SRCS ${SRC}
    REQUIRES ${requires}
    INCLUDE_DIRS ${include}
)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_CONFIG_H=1")
#target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format-truncation")
