# Install script for directory: /yuneta/development/yunetas/utils/c

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

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/fs_watcher/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/inotify/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/tr2list/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/tr2keys/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/tr2migrate/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/list_queue_msgs2/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/test-musl/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/pkey_to_jwks/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/ybatch/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/yclone-gclass/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/yclone-project/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/ycommand/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/ylist/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/ymake-skeleton/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/yscapec/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/yshutdown/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/ystats/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/ytestconfig/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/ytests/cmake_install.cmake")
  include("/yuneta/development/yunetas/cmake-build-debug-musl/utils/c/yuno-skeleton/cmake_install.cmake")

endif()

