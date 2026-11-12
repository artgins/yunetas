##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(test_c_ycommand_request_answers NONE)

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
#   ycommand from its build dir (this test builds nothing), against a
#   peer written in python, in a network namespace of its own.
#   77: what it needs is not on this machine (skipped, said why)
##############################################
add_test(
    NAME ${PROJECT_NAME}
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
        ${YUNETAS_BASE}/utils/c/ycommand/build/ycommand
)
set_tests_properties(${PROJECT_NAME} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
