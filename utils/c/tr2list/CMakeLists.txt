##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(tr2list C)

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

##############################################
#   Source
##############################################
SET (YUNO_SRCS
    tr2list.c
)

##############################################
#   yuno
##############################################
add_yuno_executable(${PROJECT_NAME} ${YUNO_SRCS})

target_link_libraries(${PROJECT_NAME}
    libargp-standalone.a
    libtimeranger2.a
    libyev_loop.a
    libyunetas-gobj.a
    liburing.a
    libjansson.a
    libpcre2-8.a
    libbacktrace.a
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
    #    DESTINATION ${BIN_DEST_DIR}
    DESTINATION "/yuneta/bin"
)


# compile in Release mode :
#
#     cmake -DCMAKE_BUILD_TYPE=Release ..
#
# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
# compile to build static binary :
#
#     cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/yuneta/development/yunetas/tools/cmake/musl-toolchain.cmake ..
#
