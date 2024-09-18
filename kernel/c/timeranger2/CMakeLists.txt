##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)

##############################################
#   Source
##############################################
add_subdirectory(tr2list)
add_subdirectory(tr2keys)

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
