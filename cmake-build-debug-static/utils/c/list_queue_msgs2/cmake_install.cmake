# Install script for directory: /yuneta/development/yunetas/utils/c/list_queue_msgs2

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

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/yuneta/development/outputs_static/bin/list_queue_msgs2" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/yuneta/development/outputs_static/bin/list_queue_msgs2")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/yuneta/development/outputs_static/bin/list_queue_msgs2"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/yuneta/development/outputs_static/bin/list_queue_msgs2")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/yuneta/development/outputs_static/bin" TYPE EXECUTABLE PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE FILES "/yuneta/development/yunetas/cmake-build-debug-static/utils/c/list_queue_msgs2/list_queue_msgs2")
  if(EXISTS "$ENV{DESTDIR}/yuneta/development/outputs_static/bin/list_queue_msgs2" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/yuneta/development/outputs_static/bin/list_queue_msgs2")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/yuneta/development/outputs_static/bin/list_queue_msgs2"
         OLD_RPATH "/yuneta/development/outputs_ext_static/lib:/yuneta/development/outputs_static/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/yuneta/development/outputs_static/bin/list_queue_msgs2")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  include("/yuneta/development/yunetas/cmake-build-debug-static/utils/c/list_queue_msgs2/CMakeFiles/list_queue_msgs2.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}/yuneta/bin/list_queue_msgs2" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/yuneta/bin/list_queue_msgs2")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/yuneta/bin/list_queue_msgs2"
         RPATH "")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/yuneta/bin/list_queue_msgs2")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "/yuneta/bin" TYPE EXECUTABLE PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE FILES "/yuneta/development/yunetas/cmake-build-debug-static/utils/c/list_queue_msgs2/list_queue_msgs2")
  if(EXISTS "$ENV{DESTDIR}/yuneta/bin/list_queue_msgs2" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/yuneta/bin/list_queue_msgs2")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/yuneta/bin/list_queue_msgs2"
         OLD_RPATH "/yuneta/development/outputs_ext_static/lib:/yuneta/development/outputs_static/lib:"
         NEW_RPATH "")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/yuneta/bin/list_queue_msgs2")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  include("/yuneta/development/yunetas/cmake-build-debug-static/utils/c/list_queue_msgs2/CMakeFiles/list_queue_msgs2.dir/install-cxx-module-bmi-Debug.cmake" OPTIONAL)
endif()

