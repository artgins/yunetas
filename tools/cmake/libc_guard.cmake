################################################################################
#   libc_guard.cmake
#
#   Refuse to link freshly compiled objects against prebuilt Yuneta archives
#   that were compiled against a different glibc.
#
#   Why this exists
#   ---------------
#   The .deb/.rpm ship prebuilt static archives (outputs/lib, outputs_ext/lib).
#   A node that has a compiler and project sources can compile its own yunos
#   against them. With CONFIG_FULLY_STATIC the link then mixes those archives
#   with the NODE's static glibc.
#
#   That is not portable. The archives reference glibc-internal symbols such as
#   `_dl_x86_cpu_features`, which back the ifunc resolvers that select the
#   CPU-tuned memcpy/strlen variants. The layout behind those internals changes
#   between glibc releases. A dynamic link fails loudly with an undefined
#   reference; a STATIC link resolves silently against the wrong layout, the
#   resolver picks a wrong routine, and the heap is corrupted at run time.
#
#   The symptom is a SIGABRT/SIGSEGV deep inside glibc's allocator
#   (`unlink_chunk`, `_int_free_merge_chunk`, `_int_malloc`) seconds after
#   start, with no Yuneta error logged first, and stack frames that point at
#   whatever innocent code happened to free next. It looks exactly like an
#   application refcount bug and it is not one. Hence this guard: fail at
#   configure time, where the cause is still visible.
#
#   How it works
#   ------------
#   `yuneta_write_libc_stamp()` records the building glibc next to the
#   archives, and ships with them inside the package. Every other build calls
#   `yuneta_check_libc_stamp()`, which compares that record against the glibc
#   of the machine doing the linking.
################################################################################

set(YUNETA_LIBC_STAMP_FILE "${YUNETAS_BASE}/outputs/lib/yuneta_libc.stamp")

#--------------------------------------------------#
#   Detect the glibc of the machine we build on
#   Sets out_var to e.g. "2.43", or "" if unknown
#--------------------------------------------------#
function(yuneta_detect_libc out_var)
    set(_version "")

    find_program(YUNETA_GETCONF getconf)
    if(YUNETA_GETCONF)
        execute_process(
            COMMAND ${YUNETA_GETCONF} GNU_LIBC_VERSION
            OUTPUT_VARIABLE _out
            ERROR_QUIET
            RESULT_VARIABLE _rc
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(_rc EQUAL 0 AND _out MATCHES "glibc[ \t]+([0-9]+\\.[0-9]+)")
            set(_version "${CMAKE_MATCH_1}")
        endif()
    endif()

    set(${out_var} "${_version}" PARENT_SCOPE)
endfunction()

#--------------------------------------------------#
#   Record the glibc that builds the archives
#   Called only by the core SDK library
#--------------------------------------------------#
function(yuneta_write_libc_stamp)
    yuneta_detect_libc(_libc)
    if(_libc STREQUAL "")
        message(WARNING
            "Cannot determine the glibc version: `getconf GNU_LIBC_VERSION` is "
            "unavailable. No stamp written, so consumers of these archives "
            "cannot be checked for a glibc mismatch.")
        return()
    endif()

    file(WRITE "${YUNETA_LIBC_STAMP_FILE}" "${_libc}\n")
    message(STATUS "Yuneta: archives built against glibc ${_libc}")
endfunction()

#--------------------------------------------------#
#   Refuse to link against archives from another glibc
#--------------------------------------------------#
function(yuneta_check_libc_stamp)
    if(YUNETA_ALLOW_LIBC_MISMATCH OR DEFINED ENV{YUNETA_ALLOW_LIBC_MISMATCH})
        message(WARNING
            "Yuneta: glibc check DISABLED by YUNETA_ALLOW_LIBC_MISMATCH. "
            "Linking prebuilt archives against a different glibc corrupts the "
            "heap at run time. Only meaningful to diagnose this guard itself.")
        return()
    endif()

    if(NOT EXISTS "${YUNETA_LIBC_STAMP_FILE}")
        # Archives predating the stamp. Nothing to compare against, so say so
        # rather than implying they were checked and found good.
        message(WARNING
            "Yuneta: no glibc stamp at ${YUNETA_LIBC_STAMP_FILE}. These "
            "archives come from an SDK older than this check, so the build "
            "cannot verify they match this machine's glibc. Rebuild the SDK "
            "from source here to get a stamp.")
        return()
    endif()

    file(READ "${YUNETA_LIBC_STAMP_FILE}" _stamped)
    string(STRIP "${_stamped}" _stamped)

    yuneta_detect_libc(_host)
    if(_host STREQUAL "")
        message(WARNING
            "Yuneta: cannot determine this machine's glibc, so the archives "
            "(built against glibc ${_stamped}) go unchecked.")
        return()
    endif()

    if(NOT _stamped STREQUAL _host)
        message(FATAL_ERROR
            "\n"
            "Yuneta: glibc mismatch. REFUSING to build.\n"
            "\n"
            "  prebuilt archives in ${YUNETAS_BASE}/outputs/lib : glibc ${_stamped}\n"
            "  this machine                                     : glibc ${_host}\n"
            "\n"
            "Linking new objects against archives built for another glibc "
            "produces a binary that CORRUPTS THE HEAP at run time: it dies "
            "inside malloc a few seconds after start, with no Yuneta error "
            "logged and a stack trace that blames unrelated code. The archives "
            "reach into glibc internals (ifunc CPU dispatch) whose layout is "
            "not stable across releases, and a static link resolves that "
            "silently instead of failing.\n"
            "\n"
            "Pick one:\n"
            "\n"
            "  1. Do not compile on this machine. Build the yunos where the "
            "SDK itself was built and ship the binaries "
            "(tools/agent/sync_binaries.py). This is the intended flow for "
            "runtime nodes.\n"
            "\n"
            "  2. Build the whole SDK from source here, so archives and glibc "
            "come from the same machine:\n"
            "       yunetas clean && yunetas build\n"
            "     (needs the SDK sources, not just the packaged outputs/).\n"
            "\n"
            "  3. Install a Yuneta package built for THIS distribution, so its "
            "archives match glibc ${_host}.\n"
            "\n"
            "Overriding with -DYUNETA_ALLOW_LIBC_MISMATCH=ON only silences the "
            "message; the resulting binary is still broken.\n")
    endif()

    # DEBUG, not STATUS: this runs once per module, and a match is the normal
    # case. Only the stamp being written and an actual mismatch are worth noise.
    message(DEBUG "Yuneta: glibc ${_host} matches the prebuilt archives")
endfunction()
