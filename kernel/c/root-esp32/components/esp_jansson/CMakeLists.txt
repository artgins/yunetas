#------------------------------------------------------#
#           Source
#------------------------------------------------------#
# Add the lib sources.
file(GLOB srcs jansson/src/*.c)

#------------------------------------------------------#
#           Register
#------------------------------------------------------#
idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS jansson/src
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_CONFIG_H=1")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format-truncation")
