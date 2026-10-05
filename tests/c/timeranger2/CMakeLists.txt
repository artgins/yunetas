##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(test_timeranger2 C)
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
    test_tranger_startup
    test_topic_pkey_integer
    test_topic_pkey_integer_iterator
    test_topic_pkey_integer_iterator2
    test_topic_pkey_integer_iterator3
    test_topic_pkey_integer_iterator4
    test_topic_pkey_integer_iterator5
    test_topic_pkey_integer_iterator6
    test_delete_instance
    test_delete_key_propagation
    test_rt_disk_multi_feed
    test_rt_disk_overflow
    test_fs_watcher_overflow
    test_rt_disk_watcher_gone
    test_rt_disk_unwatched_key
    test_out_of_order_append
    test_pkey_path_traversal
    test_topic_path_traversal
    test_read_never_exits
    test_iterator_index
    test_late_record
    test_stop_reopen
    test_append_md_contract
    test_str2system_flag
    test_tm_order
    test_lost_lock
    test_topic_var_replace
    test_key_reborn_pages
    test_open_list_history
    test_unreadable_at_open
    test_uncommitted_append
    test_torn_md2_tail
    test_md2_read_error
    test_md2_short_write
    test_nul_escape_record
    test_torn_tail_check_fails
    test_testing
    test_cmp_file_ids
    test_dir_identity
    test_unlistable_dirs
    test_unlisted_relist_once
    test_tr2check
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
    if(test STREQUAL "test_md2_read_error")
        # The test fails the read of one md2 file in its own __wrap_pread()
        target_link_options(${binary} PRIVATE "-Wl,--wrap=pread")
    endif()
    if(test STREQUAL "test_md2_short_write")
        # The test writes a md2 row short and fails its cut back, in its own
        # __wrap_write() and __wrap_ftruncate()
        target_link_options(${binary} PRIVATE "-Wl,--wrap=write,--wrap=ftruncate")
    endif()
    if(test STREQUAL "test_cmp_file_ids")
        # A unit test: it #includes timeranger2.c to reach a PRIVATE function
        target_include_directories(${binary} PRIVATE
            ${YUNETAS_BASE}/kernel/c/timeranger2/src
        )
    endif()
    if(test STREQUAL "test_dir_identity")
        # A unit test: it #includes timeranger2.c to reach a PRIVATE
        # function, and fails statx() in its own __wrap_statx()
        target_include_directories(${binary} PRIVATE
            ${YUNETAS_BASE}/kernel/c/timeranger2/src
        )
        target_link_options(${binary} PRIVATE "-Wl,--wrap=statx")
    endif()
    if(test STREQUAL "test_unlisted_relist_once")
        # The test fails the readdir() of one key directory in its own
        # __wrap_opendir() and __wrap_readdir()
        target_link_options(${binary} PRIVATE "-Wl,--wrap=opendir,--wrap=readdir,--wrap=closedir")
    endif()
    if(test STREQUAL "test_delete_key_propagation")
        # The test fails a stat(), an opendir() and a readdir() of one path in
        # its own __wrap_stat(), __wrap_opendir() and __wrap_readdir(), and
        # holds the master's delete signals in __wrap_rmdir()
        target_link_options(${binary} PRIVATE "-Wl,--wrap=stat,--wrap=opendir,--wrap=readdir,--wrap=rmdir")
    endif()
    if(test STREQUAL "test_rt_disk_unwatched_key")
        # The test fails the watch of one key directory (ENOSPC) in its own
        # __wrap_inotify_add_watch()
        target_link_options(${binary} PRIVATE "-Wl,--wrap=inotify_add_watch")
    endif()
    if(test STREQUAL "test_fs_watcher_overflow")
        # The test fails FIONREAD once in its own __wrap_ioctl(), and the
        # watch of one directory (ENOSPC) in __wrap_inotify_add_watch()
        target_link_options(${binary} PRIVATE "-Wl,--wrap=ioctl,--wrap=inotify_add_watch")
    endif()
    if(test STREQUAL "test_torn_tail_check_fails")
        # The test fails the open of a content file in its own __wrap_open()
        target_link_options(${binary} PRIVATE "-Wl,--wrap=open")
    endif()

    if(test STREQUAL "test_tr2check")
        # The test runs the tr2check tool of THIS checkout: the one built in
        # this tree when there is one (ENABLE_SDK), else the one of its module
        # build dir, which `yunetas build` and the first step of `yunetas test`
        # build. Not /yuneta/bin/tr2check: that is machine-wide, the last one
        # installed, and a worktree run (an A/B against the last tag, the
        # release suite of a node) would test somebody else's
        if(TARGET tr2check)
            add_dependencies(${binary} tr2check)
            target_compile_definitions(${binary} PRIVATE TR2CHECK_BIN="$<TARGET_FILE:tr2check>")
        else()
            target_compile_definitions(${binary} PRIVATE
                TR2CHECK_BIN="${YUNETAS_BASE}/utils/c/tr2check/build/tr2check")
        endif()
    endif()

    add_test("${current_directory_name}/${test}" ${binary})

endforeach()

##############################################
#   What the tests share, for ctest -j
##############################################
#   test_topic_pkey_integer fills tr_topic_pkey_integer, the iterators read
#   it (each opens it as master: one at a time) and _iterator6 appends to
#   it, so it goes last. RUN_SERIAL: their times are the trend the release
#   checklist reads, which other tests running at once would skew.
set_tests_properties("${current_directory_name}/test_topic_pkey_integer" PROPERTIES
    FIXTURES_SETUP tr_topic_pkey_integer
    RESOURCE_LOCK tr_topic_pkey_integer
    RUN_SERIAL TRUE
)
set(PKEY_INTEGER_READERS
    test_topic_pkey_integer_iterator
    test_topic_pkey_integer_iterator2
    test_topic_pkey_integer_iterator3
    test_topic_pkey_integer_iterator4
    test_topic_pkey_integer_iterator5
)
set(PKEY_INTEGER_READER_TESTS "")
foreach(test ${PKEY_INTEGER_READERS})
    set_tests_properties("${current_directory_name}/${test}" PROPERTIES
        FIXTURES_REQUIRED tr_topic_pkey_integer
        RESOURCE_LOCK tr_topic_pkey_integer
        RUN_SERIAL TRUE
    )
    list(APPEND PKEY_INTEGER_READER_TESTS "${current_directory_name}/${test}")
endforeach()
set_tests_properties("${current_directory_name}/test_topic_pkey_integer_iterator6" PROPERTIES
    FIXTURES_REQUIRED tr_topic_pkey_integer
    RESOURCE_LOCK tr_topic_pkey_integer
    DEPENDS "${PKEY_INTEGER_READER_TESTS}"
    RUN_SERIAL TRUE
)

#   tr_delete_instance is tr_treedb_delete_instance's database too
set_tests_properties("${current_directory_name}/test_delete_instance" PROPERTIES
    RESOURCE_LOCK tr_delete_instance
)

#   Both flood the per-user inotify queue
set_tests_properties(
    "${current_directory_name}/test_fs_watcher_overflow"
    "${current_directory_name}/test_rt_disk_overflow"
    PROPERTIES RESOURCE_LOCK inotify_flood
)

#   Alone under ctest -j: it asserts the watcher's cost per directory on a
#   big tree against a small one (at most 6 times), and other tests loading
#   the machine at the same time inflate the big pass only. On wattyzer at
#   -j8 it measured 48 us against 713 (31 s alone, 86 s in the suite)
set_tests_properties("${current_directory_name}/test_fs_watcher_overflow"
    PROPERTIES RUN_SERIAL TRUE)
