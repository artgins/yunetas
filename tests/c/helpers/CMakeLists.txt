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

#----------------------------------------#
#   Static binaries
#   To compile as static,
#   also using gcc, set next:
#----------------------------------------#
if(CONFIG_FULLY_STATIC)
    set(CMAKE_EXE_LINKER_FLAGS "-static -Wl,-Bstatic")
    set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-static")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(BUILD_SHARED_LIBS OFF)
endif()


##############################################
#   Source
##############################################
set(SRCS
    test_helpers
    test_rotatory
    test_audit_record
    test_dir_array_nomem
    test_dir_listing
)

##############################################
#   Tests
##############################################
foreach(test ${SRCS})
    set(binary "${test}")
    add_yuno_executable(${binary} "${test}.c")

    if(CONFIG_FULLY_STATIC)
        set_target_properties(${binary} PROPERTIES
            LINK_SEARCH_START_STATIC TRUE
            LINK_SEARCH_END_STATIC TRUE
        )
    endif()

    target_link_libraries(${binary}
        ${YUNETAS_KERNEL_LIBS}
        ${YUNETAS_EXTERNAL_LIBS}
        ${YUNETAS_PCRE_LIBS}
        ${JWT_LIBS}
        ${OPENSSL_LIBS}
        ${MBEDTLS_LIBS}
        ${DEBUG_LIBS}
    )
    target_link_options(${binary} PUBLIC LINKER:-Map=${PROJECT_NAME}.map)

    add_test("${current_directory_name}/${test}" ${binary})

endforeach()

# test_helpers makes an entry disappear during a walk (see its test_rmrdir_entry_vanishes)
target_link_options(test_helpers PUBLIC LINKER:--wrap=lstat)

# test_rotatory fakes the free space of one directory, the clock and a failing rename (see its header)
target_link_options(test_rotatory PUBLIC LINKER:--wrap=statvfs,--wrap=fstatvfs,--wrap=time,--wrap=rename)

# test_audit_record tests the audit record builder of yuno_agent (its own source)
target_sources(test_audit_record PRIVATE
    ${YUNETAS_BASE}/yunos/c/yuno_agent/src/audit_record.c
)
target_include_directories(test_audit_record PRIVATE
    ${YUNETAS_BASE}/yunos/c/yuno_agent/src
)

# test_dir_listing tests the answer of the agent's dir-* commands (its own source)
target_sources(test_dir_listing PRIVATE
    ${YUNETAS_BASE}/yunos/c/yuno_agent/src/dir_listing.c
)
target_include_directories(test_dir_listing PRIVATE
    ${YUNETAS_BASE}/yunos/c/yuno_agent/src
)
