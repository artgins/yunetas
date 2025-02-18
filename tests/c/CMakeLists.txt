##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)

##############################################
#   Source
##############################################
add_subdirectory(yev_loop/yev_events)
add_subdirectory(yev_loop/yev_events_tls)
add_subdirectory(timeranger2)
add_subdirectory(tr_msg)
add_subdirectory(tr_treedb)
add_subdirectory(c_timer0)
add_subdirectory(c_timer)
add_subdirectory(c_tcp)
add_subdirectory(c_tcps)
add_subdirectory(tr_queue)
