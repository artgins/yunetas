##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
get_filename_component(current_directory_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)

#-----------------------------------------------------#
#   Resolve YUNETAS_BASE
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#   - else default "/yuneta/development"
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

message(DEBUG "Using YUNETAS_BASE: ${YUNETAS_BASE}")

# Ensure the expected cmake file exists
set(_yunetas_project_cmake "${YUNETAS_BASE}/tools/cmake/project.cmake")
if(NOT EXISTS "${_yunetas_project_cmake}")
  message(FATAL_ERROR "Missing: ${_yunetas_project_cmake}")
endif()

include("${_yunetas_project_cmake}")

##############################################
#   Test
#
#   The static resolver only exists under CONFIG_FULLY_STATIC (it replaces
#   glibc getaddrinfo). This is a self-contained unit test: it #includes
#   static_resolv.c directly to reach the static helpers, links only libc
#   (no io_uring / kernel libs), and binds an unprivileged localhost port via
#   YUNETA_DNS_PORT, which the same macro routes dns_query() to.
##############################################
if(CONFIG_FULLY_STATIC)
    add_executable(test_static_resolv_spoof test_static_resolv_spoof.c)

    target_include_directories(test_static_resolv_spoof PRIVATE
        ${YUNETAS_BASE}/outputs/include
        ${YUNETAS_BASE}/kernel/c/yev_loop/src
    )
    target_compile_definitions(test_static_resolv_spoof PRIVATE YUNETA_DNS_PORT=15353)

    add_test("${current_directory_name}/test_static_resolv_spoof" test_static_resolv_spoof)
endif()
