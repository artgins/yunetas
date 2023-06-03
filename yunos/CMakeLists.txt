##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.0)
project(yunos C)


##############################################
#   Source
##############################################
#add_subdirectory(watchfs)
#add_subdirectory(emailsender)
#add_subdirectory(logcenter)
#add_subdirectory(emu_device)
#add_subdirectory(controlcenter)
#add_subdirectory(sgateway)
#add_subdirectory(dba_postgres)

# compile in Release mode optimized but adding debug symbols, useful for profiling :
#
#     cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
#
# or compile with NO optimization and adding debug symbols :
#
#     cmake -DCMAKE_BUILD_TYPE=Debug ..
#
# Compilando para ARM poner cmake -DNOT_INCLUDE_LIBUNWIND
#
