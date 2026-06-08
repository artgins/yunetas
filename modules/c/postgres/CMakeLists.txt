##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.11)
project(yunetas-module-postgres C)

#-----------------------------------------------------#
#   Resolve YUNETAS_BASE
#   Get yunetas base path:
#   - defined in environment variable YUNETAS_BASE
#   - else default "/yuneta/development/yunetas"
#   - else default "/yuneta/development"
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

message(DEBUG "Using YUNETAS_BASE: ${YUNETAS_BASE}")

# Ensure the expected cmake file exists
set(_yunetas_project_cmake "${YUNETAS_BASE}/tools/cmake/project.cmake")
if(NOT EXISTS "${_yunetas_project_cmake}")
    message(FATAL_ERROR "Missing: ${_yunetas_project_cmake}")
endif()

include("${_yunetas_project_cmake}")

##############################################
#   Source
##############################################
set (SRCS
    src/c_postgres.c
)

set (HDRS
    src/c_postgres.h
)

##############################################
#   Compilation condition
##############################################
if(CONFIG_MODULE_POSTGRES)

##############################################
#   Lib
##############################################
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

#
#   libpq header location is distro-dependent:
#     - Debian/Ubuntu: /usr/include/postgresql  (pg_config --includedir)
#     - RHEL/Rocky/Alma/Fedora: /usr/include
#   The source includes <libpq-fe.h>, so add the dir pg_config reports.
#
find_program(PG_CONFIG_EXECUTABLE NAMES pg_config)
if(PG_CONFIG_EXECUTABLE)
    execute_process(
        COMMAND ${PG_CONFIG_EXECUTABLE} --includedir
        OUTPUT_VARIABLE LIBPQ_INCLUDEDIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(LIBPQ_INCLUDEDIR)
        target_include_directories(${PROJECT_NAME} PRIVATE ${LIBPQ_INCLUDEDIR})
        message(STATUS "libpq include dir: ${LIBPQ_INCLUDEDIR}")
    endif()
else()
    message(WARNING
        "pg_config not found; relying on the default include path for "
        "<libpq-fe.h>. Install libpq-dev (Debian) or libpq-devel (RHEL).")
endif()

target_link_options(${PROJECT_NAME} PUBLIC LINKER:-Map=${PROJECT_NAME}.map)

##############################################
#   System install
##############################################
install(FILES ${HDRS} DESTINATION ${INC_DEST_DIR})

install(
    TARGETS ${PROJECT_NAME}
    PERMISSIONS
    OWNER_READ OWNER_WRITE
    GROUP_READ GROUP_WRITE
    WORLD_READ
    DESTINATION ${LIB_DEST_DIR}
)

##############################################
#   Compilation condition
##############################################
else()
    install(CODE "message(STATUS \"Nothing to install\")")
endif()
