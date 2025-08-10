##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.10)
project(stress_listen C)

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
SET(SRCS
    listen
)

##############################################
#   Tests
##############################################
foreach(test ${SRCS})
    set(binary "stress_${test}")
    add_yuno_executable(${binary} "main_${test}.c" "c_${test}.c")

    target_link_libraries(${binary}
        ${YUNETAS_KERNEL_LIBS}
        ${YUNETAS_EXTERNAL_LIBS}
        ${YUNETAS_PCRE_LIBS}
        ${OPENSSL_LIBS}
        ${MBEDTLS_LIBS}
        ${DEBUG_LIBS}
    )

endforeach()

##############################################
#   System install
##############################################
foreach(test ${SRCS})
    set(binary "stress_${test}")

    install(
        TARGETS ${binary}
        PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_WRITE GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
        DESTINATION ${YUNOS_DEST_DIR}
    )

endforeach()
