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

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/yuneta/development/outputs/include/c_linux_yuno.h;/yuneta/development/outputs/include/c_timer.h;/yuneta/development/outputs/include/c_linux_transport.h;/yuneta/development/outputs/include/c_linux_uart.h;/yuneta/development/outputs/include/yunetas_environment.h;/yuneta/development/outputs/include/yunetas_ev_loop.h")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/yuneta/development/outputs/include" TYPE FILE FILES
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_linux_yuno.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_timer.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_linux_transport.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/c_linux_uart.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/yunetas_environment.h"
    "/yuneta/development/yunetas/kernel/c/root-linux/src/yunetas_ev_loop.h"
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
  file(INSTALL DESTINATION "/yuneta/development/outputs/lib" TYPE STATIC_LIBRARY PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ GROUP_WRITE WORLD_READ FILES "/yuneta/development/yunetas/cmake-build-debug-coverage/kernel/c/root-linux/libyunetas-core-linux.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  include("/yuneta/development/yunetas/cmake-build-debug-coverage/kernel/c/root-linux/CMakeFiles/yunetas-core-linux.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
endif()

