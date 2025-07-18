##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(c-utils C)

##############################################
#   Source
##############################################
add_subdirectory(fs_watcher)
add_subdirectory(inotify)
add_subdirectory(tr2list)
add_subdirectory(tr2keys)
add_subdirectory(tr2migrate)
add_subdirectory(list_queue_msgs2)
add_subdirectory(test-musl)
add_subdirectory(pkey_to_jwks)

add_subdirectory(ybatch)
add_subdirectory(yclone-gclass)
add_subdirectory(yclone-project)
#add_subdirectory(ycommand)
add_subdirectory(ylist)
add_subdirectory(ymake-skeleton)
add_subdirectory(yscapec)
add_subdirectory(yshutdown)
add_subdirectory(ystats)
add_subdirectory(ytestconfig)
add_subdirectory(ytests)
add_subdirectory(yuno-skeleton)

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
