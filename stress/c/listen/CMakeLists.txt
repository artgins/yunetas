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
    set(YUNETAS_BASE_DIR $ENV{YUNETAS_BASE})
else()
    set(YUNETAS_BASE_DIR "/yuneta/development/yunetas")
endif()
include("${YUNETAS_BASE_DIR}/tools/cmake/project.cmake")

##############################################
#   Source
##############################################
SET(SRCS
    listen
)

SET(COMMON
)

##############################################
#   Tests
##############################################
foreach(test ${SRCS})
    set(binary "stress_${test}")
    add_yuno_executable(${binary} "main_${test}.c" "c_${test}.c" ${COMMON})

    target_link_libraries(${binary}
        ${YUNETAS_KERNEL_LIBS}
        ${YUNETAS_EXTERNAL_LIBS}
        ${YUNETAS_PCRE_LIBS}
    )

    if(CONFIG_YTLS_USE_MBEDTLS)
        target_link_libraries(${binary}
            ${MBEDTLS_LIBS}
        )
    endif()

#    if(CONFIG_YTLS_USE_OPENSSL) TODO openssl needs yet
        target_link_libraries(${binary}
            ${OPENSSL_LIBS}
        )
#    endif()

    if(CONFIG_DEBUG_WITH_BACKTRACE)
        target_link_libraries(${binary}
            ${DEBUG_LIBS}
        )
    endif()

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
