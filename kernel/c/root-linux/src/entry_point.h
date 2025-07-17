/****************************************************************************
 *          entry_point.c
 *
 *          Entry point for yunos (yuneta daemons).
 *
 *          Copyright (c) 2014-2015 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *  Prototypes
 *********************************************************************/
/*
 *  Yuneta setup function.
 */
PUBLIC int yuneta_setup(
    const persistent_attrs_t    *persistent_attrs,      // default internal dbsimple
    json_function_fn            command_parser,         // default internal command_parser
    json_function_fn            stats_parser,           // default internal stats_parser
    authorization_checker_fn    authz_checker,          // default Monoclass C_AUTHZ
    authentication_parser_fn    authenticate_parser,    // default Monoclass C_AUTHZ
    size_t                      mem_max_block,
    size_t                      mem_max_system_memory,
    BOOL                        use_own_system_memory,
    size_t                      mem_min_block,
    size_t                      mem_superblock
);

PUBLIC int yuneta_entry_point(int argc, char *argv[],
    const char *APP_NAME,
    const char *APP_VERSION,
    const char *APP_SUPPORT,
    const char *APP_DOC,
    const char *APP_DATETIME,
    const char *fixed_config,
    const char *variable_config,
    int (*register_yuno_and_more)(void), // HACK This function is executed on yunetas environment (mem, log, paths) BEFORE creating the yuno
    void (*cleaning_fn)(void) // HACK This function is executed after free all yuneta resources
);

/*
 *  For TEST: kill the yuno in `timeout` seconds, only if not running as daemon.
 */
PUBLIC void set_auto_kill_time(int seconds);

#ifdef __cplusplus
}
#endif
