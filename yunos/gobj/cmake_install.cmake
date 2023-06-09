# Install script for directory: /yuneta/development/yuneta/yunetas/gobj

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

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/yuneta/development/outputs/include/ansi_escape_codes.h;/yuneta/development/outputs/include/00_http_parser.h;/yuneta/development/outputs/include/ghttp_parser.h;/yuneta/development/outputs/include/gobj.h;/yuneta/development/outputs/include/istream.h;/yuneta/development/outputs/include/parse_url.h;/yuneta/development/outputs/include/gobj_environment.h;/yuneta/development/outputs/include/log_udp_handler.h;/yuneta/development/outputs/include/helpers.h")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/yuneta/development/outputs/include" TYPE FILE FILES
    "/yuneta/development/yuneta/yunetas/gobj/src/ansi_escape_codes.h"
    "/yuneta/development/yuneta/yunetas/gobj/src/00_http_parser.h"
    "/yuneta/development/yuneta/yunetas/gobj/src/ghttp_parser.h"
    "/yuneta/development/yuneta/yunetas/gobj/src/gobj.h"
    "/yuneta/development/yuneta/yunetas/gobj/src/istream.h"
    "/yuneta/development/yuneta/yunetas/gobj/src/parse_url.h"
    "/yuneta/development/yuneta/yunetas/gobj/src/gobj_environment.h"
    "/yuneta/development/yuneta/yunetas/gobj/src/log_udp_handler.h"
    "/yuneta/development/yuneta/yunetas/gobj/src/helpers.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/yuneta/development/outputs/lib/libyunetas-gobj.a")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/yuneta/development/outputs/lib" TYPE STATIC_LIBRARY PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ GROUP_WRITE WORLD_READ FILES "/yuneta/development/yuneta/yunetas/yunos/gobj/libyunetas-gobj.a")
endif()

