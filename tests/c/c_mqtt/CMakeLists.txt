##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(test_c_mqtt C)
get_filename_component(current_directory_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)

#-----------------------------------------------------#
#   Resolve YUNETAS_BASE
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

message(WARNING "Using YUNETAS_BASE: ${YUNETAS_BASE}")

set(_yunetas_project_cmake "${YUNETAS_BASE}/tools/cmake/project.cmake")
if(NOT EXISTS "${_yunetas_project_cmake}")
  message(FATAL_ERROR "Missing: ${_yunetas_project_cmake}")
endif()

include("${_yunetas_project_cmake}")

##############################################
#   MQTT broker sources (embedded in test)
##############################################
set(MQTT_BROKER_SRC_DIR "${YUNETAS_BASE}/yunos/c/mqtt_broker/src")
set(MQTT_MQTT_SRC_DIR "${YUNETAS_BASE}/modules/c/c_prot/src")

set(MQTT_BROKER_SRCS
    ${MQTT_BROKER_SRC_DIR}/c_mqtt_broker.c
    ${MQTT_MQTT_SRC_DIR}/c_prot_mqtt2.c
    ${MQTT_MQTT_SRC_DIR}/tr2q_mqtt.c
    ${MQTT_MQTT_SRC_DIR}/mqtt_util.c
)

##############################################
#   Tests
##############################################
set(binary "test_mqtt_test1")
add_yuno_executable(${binary}
    main_test1.c
    c_test1.c
    ${MQTT_BROKER_SRCS}
)

target_include_directories(${binary} PRIVATE
    ${MQTT_BROKER_SRC_DIR}
)

target_link_libraries(${binary}
    ${YUNETAS_KERNEL_LIBS}
    ${YUNETAS_EXTERNAL_LIBS}
    ${YUNETAS_PCRE_LIBS}
    ${OPENSSL_LIBS}
    ${MBEDTLS_LIBS}
    ${DEBUG_LIBS}
)

add_test("${current_directory_name}/test1" ${binary})
set_tests_properties("${current_directory_name}/test1" PROPERTIES TIMEOUT 30)
