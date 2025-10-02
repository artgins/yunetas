##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(yunos C)

##############################################
#   Source
##############################################
add_subdirectory(controlcenter)
add_subdirectory(dba_postgres)
add_subdirectory(emailsender)
add_subdirectory(emu_device)
add_subdirectory(logcenter)
add_subdirectory(sgateway)
add_subdirectory(watchfs)
add_subdirectory(yuno_cli)
add_subdirectory(yuno_agent)
add_subdirectory(yuno_agent22)
add_subdirectory(mqtt_broker)
add_subdirectory(mqtt_client)

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
# compile to build static binary :
#
#     cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/yuneta/development/yunetas/tools/cmake/musl-toolchain.cmake ..
#
#
