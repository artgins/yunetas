##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(perf_yev_ping_pong C)

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

include_directories(/yuneta/development/projects/^mulesol/mulesol-sistemas/projects/frigo/esp/esp_frigo/main)


##############################################
#   Source
##############################################
SET (YUNO_SRCS
    src/perf_yev_ping_pong.c
)
SET (YUNO_HDRS
)

##############################################
#   yuno
##############################################
add_executable(${PROJECT_NAME} ${YUNO_SRCS} ${YUNO_HDRS})

target_link_libraries(${PROJECT_NAME}
    libyunetas-core-linux.a
    libyunetas-gobj.a

    libjansson.a
    liburing.a
    m
    #z rt m
    uuid
    #util
    bfd     # to stacktrace
)

#if(ESP32_MODE)
#    target_link_libraries(${PROJECT_NAME}
#        /yuneta/development/outputs/lib/libyunetas-core-linux.a
## NO resuelto       /yuneta/development/outputs/lib/libyunetas-esp32.a   # To test partially esp32 you can include this
#    )
#else()
#    target_link_libraries(${PROJECT_NAME}
#        /yuneta/development/outputs/lib/libyunetas-core-linux.a
#    )
#endif()

target_link_options(${PROJECT_NAME} PUBLIC LINKER:-Map=${PROJECT_NAME}.map)

# Add a custom command to generate assembler .lst file
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND objdump -SlF ${PROJECT_NAME} > ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.lst
    COMMENT "Generating assembler"
)

##############################################
#   Installation
##############################################
install(
    TARGETS ${PROJECT_NAME}
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION ${BIN_DEST_DIR}
)

# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# or compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
