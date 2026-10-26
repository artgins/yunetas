##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(test_yuno_skeletons NONE)

##############################################
#   Test
#   The templates of yuno-skeleton, instantiated and built (and the
#   standalone yuno run a few seconds): see test_yuno_skeletons.sh
##############################################
add_test(
    NAME yuno_skeleton/templates
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_yuno_skeletons.sh
)
set_tests_properties(yuno_skeleton/templates PROPERTIES TIMEOUT 600)

##############################################
#   Test
#   The cloners (yclone-gclass, yclone-project, ymake-skeleton) on a
#   yuno made by yuno-skeleton: see test_cloners.sh
##############################################
add_test(
    NAME yuno_skeleton/cloners
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_cloners.sh
)
set_tests_properties(yuno_skeleton/cloners PROPERTIES TIMEOUT 600)
