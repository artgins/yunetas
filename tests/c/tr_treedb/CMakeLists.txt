##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.5)
project(test_tr_treedb VERSION 1.0)
get_filename_component(current_directory_name ${CMAKE_CURRENT_SOURCE_DIR} NAME)

#-----------------------------------------------------#
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#-----------------------------------------------------#
if(DEFINED ENV{YUNETAS_BASE})
  set(YUNETAS_BASE_DIR $ENV{YUNETAS_BASE})
else()
  set(YUNETAS_BASE_DIR "/yuneta/development/yunetas")
endif()
include("${YUNETAS_BASE_DIR}/tools/cmake/project.cmake")

##############################################
#   Source
##############################################
set(SRCS
    test_tr_treedb.c
    test_departments.c
    test_users.c
    test_compound.c
    foto_final_departments.c
    foto_final_users.c
    users_file.c
    foto_final1.c
    foto_final2.c
)

##############################################
#   Tests
##############################################
add_executable(test_tr_treedb ${SRCS})
target_link_libraries(test_tr_treedb
    libyunetas-c_prot.a
    libyunetas-core-linux.a
    libtimeranger2.a
    libyunetas-gobj.a

    libjansson.a
    liburing.a
    libpcre2-8.a
    m
    uuid
    bfd     # to stacktrace
)

add_test(NAME Test_tr_treedb COMMAND test_tr_treedb)
