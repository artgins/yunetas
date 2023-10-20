##############################################
#   CMake
##############################################
#cmake_minimum_required(VERSION 3.0)
include(/yuneta/development/yuneta/yunetas/tools/cmake/project.cmake)
#project(test_gobj C)

##############################################
#   Source
##############################################
set (SRCS
    test_gobj.c
)


add_executable(test_gobj test_gobj.c)

target_link_libraries (test_gobj foo ${CRITERION_LIBRARIES})

# use C99
set_property (TARGET test_gobj PROPERTY C_STANDARD 99)

# Enable a few warnings
if (CMAKE_COMPILER_IS_GNUCC)
  target_compile_options (test_gobj PRIVATE -Wall -Wextra)
endif (CMAKE_COMPILER_IS_GNUCC)
if (MSVC)
  target_compile_options (test_gobj PRIVATE /W4)
endif (MSVC)

add_test (test_gobj test_gobj)
