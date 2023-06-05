#------------------------------------------------------#
#           Source
#------------------------------------------------------#
file(GLOB srcs src/*.c)

set(requires
    esp_event
    esp_jansson
)

#------------------------------------------------------#
#           Register
#------------------------------------------------------#
idf_component_register(
    SRCS ${srcs}
    REQUIRES ${requires}
    INCLUDE_DIRS
        src/
)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_CONFIG_H=1")
#target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format-truncation")
