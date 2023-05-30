#------------------------------------------------------#
#           Source
#------------------------------------------------------#
file(GLOB SRC src/*.c)

set(requires
    esp_jansson
)

#------------------------------------------------------#
#           Register
#------------------------------------------------------#
idf_component_register(
    SRCS ${SRC}
    REQUIRES ${requires}
    INCLUDE_DIRS
        src/
)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_CONFIG_H=1")
#target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format-truncation")
