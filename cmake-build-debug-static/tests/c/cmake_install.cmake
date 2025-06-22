# Install script for directory: /yuneta/development/yunetas/tests/c

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/yuneta/development/outputs_static")
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
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/yev_loop/yev_events/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/yev_loop/yev_events_tls/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/timeranger2/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/tr_msg/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/tr_treedb/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/c_timer0/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/c_timer/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/c_tcp/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/c_tcps/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/c_tcp2/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/c_tcps2/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/tr_queue/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/c_subscriptions/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-static/tests/c/kw/cmake_install.cmake")

endif()

