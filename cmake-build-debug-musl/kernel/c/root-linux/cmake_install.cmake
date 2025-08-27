# Install script for directory: /yuneta/development/yunetas/kernel/c/root-linux

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/yuneta/development/outputs")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/yuneta/development/outputs/include/c_yuno.h;/yuneta/development/outputs/include/c_timer0.h;/yuneta/development/outputs/include/c_timer.h;/yuneta/development/outputs/include/c_tcp.h;/yuneta/development/outputs/include/c_tcp_s.h;/yuneta/development/outputs/include/c_uart.h;/yuneta/development/outputs/include/c_channel.h;/yuneta/development/outputs/include/c_iogate.h;/yuneta/development/outputs/include/c_qiogate.h;/yuneta/development/outputs/include/c_mqiogate.h;/yuneta/development/outputs/include/c_ievent_srv.h;/yuneta/development/outputs/include/c_ievent_cli.h;/yuneta/development/outputs/include/c_prot_http_cl.h;/yuneta/development/outputs/include/c_prot_http_sr.h;/yuneta/development/outputs/include/c_prot_mqtt.h;/yuneta/development/outputs/include/c_prot_tcp4h.h;/yuneta/development/outputs/include/c_websocket.h;/yuneta/development/outputs/include/c_authz.h;/yuneta/development/outputs/include/c_task.h;/yuneta/development/outputs/include/c_task_authenticate.h;/yuneta/development/outputs/include/c_tranger.h;/yuneta/development/outputs/include/c_node.h;/yuneta/development/outputs/include/c_treedb.h;/yuneta/development/outputs/include/c_resource2.h;/yuneta/development/outputs/include/c_fs.h;/yuneta/development/outputs/include/c_prot_raw.h;/yuneta/development/outputs/include/c_udp.h;/yuneta/development/outputs/include/c_udp_s.h;/yuneta/development/outputs/include/c_gss_udp_s.h;/yuneta/development/outputs/include/c_counter.h;/yuneta/development/outputs/include/ghttp_parser.h;/yuneta/development/outputs/include/istream.h;/yuneta/development/outputs/include/yunetas_environment.h;/yuneta/development/outputs/include/dbsimple.h;/yuneta/development/outputs/include/entry_point.h;/yuneta/development/outputs/include/msg_ievent.h;/yuneta/development/outputs/include/yunetas_register.h;/yuneta/development/outputs/include/ydaemon.h;/yuneta/development/outputs/include/run_command.h;/yuneta/development/outputs/include/yunetas.h")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/yuneta/development/outputs/include" TYPE FILE FILES
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_yuno.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_timer0.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_timer.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_tcp.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_tcp_s.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_uart.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_channel.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_iogate.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_qiogate.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_mqiogate.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_ievent_srv.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_ievent_cli.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_prot_http_cl.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_prot_http_sr.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_prot_mqtt.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_prot_tcp4h.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_websocket.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_authz.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_task.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_task_authenticate.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_tranger.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_node.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_treedb.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_resource2.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_fs.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_prot_raw.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_udp.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_udp_s.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_gss_udp_s.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_counter.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/ghttp_parser.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/istream.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/yunetas_environment.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/dbsimple.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/entry_point.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/msg_ievent.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/yunetas_register.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/ydaemon.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/run_command.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/yunetas.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/yuneta/development/outputs/lib/libyunetas-core-linux.a")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/yuneta/development/outputs/lib" TYPE STATIC_LIBRARY PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ GROUP_WRITE WORLD_READ FILES "/yuneta/development/yunetas/cmake-build-debug-musl/kernel/c/root-linux/libyunetas-core-linux.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  include("/yuneta/development/yunetas/cmake-build-debug-musl/kernel/c/root-linux/CMakeFiles/yunetas-core-linux.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
endif()

