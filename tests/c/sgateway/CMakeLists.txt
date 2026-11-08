##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(test_sgateway NONE)

#-----------------------------------------------------#
#   Resolve YUNETAS_BASE
#-----------------------------------------------------#
if(DEFINED ENV{YUNETAS_BASE} AND IS_DIRECTORY "$ENV{YUNETAS_BASE}")
    set(YUNETAS_BASE "$ENV{YUNETAS_BASE}")
elseif(IS_DIRECTORY "/yuneta/development/yunetas")
    set(YUNETAS_BASE "/yuneta/development/yunetas")
elseif(IS_DIRECTORY "/yuneta/development")
    set(YUNETAS_BASE "/yuneta/development")
else()
    message(FATAL_ERROR
        "YUNETAS_BASE not found.\n"
        "Set the environment variable YUNETAS_BASE to a valid directory, "
        "or ensure /yuneta/development[/yunetas] exists.")
endif()

##############################################
#   Test
#   The real sgateway (outputs/yunos: this test builds nothing), driven
#   by a peer written in python: a listener as its output target and a
#   client of its input port.
#   77: the binary is not there (skipped, said why)
##############################################
add_test(
    NAME ${PROJECT_NAME}
    COMMAND python3 -I ${CMAKE_CURRENT_SOURCE_DIR}/src/test_sgateway.py
        ${YUNETAS_BASE}/outputs/yunos/sgateway
)
set_tests_properties(${PROJECT_NAME} PROPERTIES SKIP_RETURN_CODE 77)
