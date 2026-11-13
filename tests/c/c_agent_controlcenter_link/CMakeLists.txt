##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(test_c_agent_controlcenter_link NONE)

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
#   The real agent (its build dir: this test builds nothing), run in
#   namespaces of its own, linked to a control center written in python.
#   77: what it needs is not on this machine (skipped, said why)
##############################################
add_test(
    NAME ${PROJECT_NAME}
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run_test.sh
        ${YUNETAS_BASE}/yunos/c/yuno_agent/build/yuneta_agent
)
# Alone under ctest -j: its slow link (tc, ~30 KB/s) must keep a pace the
# console's deadline accepts, and the load of the tests running at once
# slows it below that (11 KB/s seen under -j8)
set_tests_properties(${PROJECT_NAME} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 300 RUN_SERIAL TRUE)
