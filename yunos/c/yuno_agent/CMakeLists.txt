##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(yuneta_agent C)

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
SET (YUNO_SRCS
    src/main.c
    src/c_agent.c
    src/agent_treedb.c
    src/audit_record.c
    src/dir_listing.c
    src/file_content.c
    src/find_new_yunos.c
    src/restart_plan.c
    src/watch_request.c
    src/yuno_config_file.c
)
SET (YUNO_HDRS
    src/c_agent.h
    src/agent_treedb.h
    src/audit_record.h
    src/dir_listing.h
    src/file_content.h
    src/find_new_yunos.h
    src/restart_plan.h
    src/watch_request.h
    src/yuno_config_file.h
)

##############################################
#   yuno
##############################################
add_yuno_executable(${PROJECT_NAME} ${YUNO_SRCS} ${YUNO_HDRS})

if(CONFIG_FULLY_STATIC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_SEARCH_START_STATIC TRUE
        LINK_SEARCH_END_STATIC TRUE
    )
endif()

target_link_libraries(${PROJECT_NAME}
    ${YUNETAS_KERNEL_LIBS}
    ${YUNETAS_EXTERNAL_LIBS}
    ${YUNETAS_PCRE_LIBS}
    ${JWT_LIBS}
    ${OPENSSL_LIBS}
    ${MBEDTLS_LIBS}
    ${DEBUG_LIBS}
)

##############################################
#   Installation
##############################################
#   This lands service/yuneta_agent on /yuneta/agent, where the postinst of
#   the .deb/.rpm reads it to seed /etc/init.d/yuneta_agent -- so a build on a
#   source node OVERWRITES what the package put there. The packagers are the
#   authority for that script (the heredocs in packages/{deb,rpm}/
#   make-yuneta-agent-*.sh); the copy here must stay identical to the .deb one,
#   which is the only flavour a source node can be
#   (packages/tests/test-yuneta-agent-cert.sh checks it). When they drifted,
#   the copy here still started /yuneta/bin/nginx/sbin/nginx by hand: a second
#   web server, of the wrong kind on a node serving with openresty, racing the
#   yuneta-webserver unit for ports 80 and 443; up to 7.30.15 it drove no
#   systemd unit and ran neither yuneta-node-uuid nor yuneta-agent-cert.
install(DIRECTORY service
    USE_SOURCE_PERMISSIONS
    DESTINATION "/yuneta/agent"
)

install(DIRECTORY certs
    USE_SOURCE_PERMISSIONS
    DESTINATION "/yuneta/agent"
)

#   The agent's own pair (yuneta_agent.{crt,key}) is not in certs/: it is
#   made on the node, by the user that builds (yuneta-agent-cert drops to
#   yuneta when run as root), when there is none or when it is the pair
#   published with Yuneta up to 7.30.5; any other pair is kept. A package is
#   built from these files and must not carry the build machine's pair: its
#   builders copy only controlcenter.crt.
install(CODE "
    execute_process(
        COMMAND \"${YUNETAS_BASE}/packages/templates/yuneta-agent-cert\" ensure
        RESULT_VARIABLE _agent_cert_rc
    )
    if(NOT _agent_cert_rc EQUAL 0)
        message(WARNING \"yuneta-agent-cert ensure failed (\${_agent_cert_rc}): the agent keeps the pair it had\")
    endif()
")

#   The agents' units run the scripts that packages/install-yuneta-node-uuid.sh
#   and packages/install-yuneta-agent-cert.sh put in /usr/libexec/yuneta, and
#   a `git pull` does not update them: say it when they are not this tree's
#   (nothing when the node never took them).
install(CODE "set(_yuneta_templates \"${YUNETAS_BASE}/packages/templates\")")
install(CODE [[
    set(_stale "")
    foreach(_s yuneta-node-uuid yuneta-agent-cert)
        set(_installed "/usr/libexec/yuneta/${_s}")
        if(EXISTS "${_installed}")
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E compare_files "${_installed}" "${_yuneta_templates}/${_s}"
                RESULT_VARIABLE _same_rc
            )
            if(NOT _same_rc EQUAL 0)
                list(APPEND _stale "${_installed} is not this tree's")
            endif()
        elseif(EXISTS "/usr/libexec/yuneta/yuneta-agent-cert")
            list(APPEND _stale "${_installed} is missing (the pair is bound to the node uuid)")
        endif()
    endforeach()
    foreach(_unit yuneta_agent yuneta_agent22)
        set(_old_dropin "/etc/systemd/system/${_unit}.service.d/yuneta-agent-cert.conf")
        if(EXISTS "${_old_dropin}")
            list(APPEND _stale "${_old_dropin} is of 7.30.14 (it runs before the node uuid)")
        endif()
    endforeach()
    if(_stale)
        string(REPLACE ";" "; " _stale_text "${_stale}")
        message(WARNING "${_stale_text}. From this checkout, as root, one after the other: packages/install-yuneta-node-uuid.sh, then packages/install-yuneta-agent-cert.sh")
    endif()
]])

install(
    TARGETS ${PROJECT_NAME}
    PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_WRITE GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
    DESTINATION "/yuneta/agent"
)

# compile in Release mode :
#
#     cmake -DCMAKE_BUILD_TYPE=Release ..
#
# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
#
