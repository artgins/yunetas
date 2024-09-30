/****************************************************************************
 *          entry_point.c
 *
 *          Entry point for yunos (yuneta daemons).
 *
 *          Copyright (c) 2014-2015 Niyamaka, 2024- ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gobj.h"

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
    void *(*startup_persistent_attrs)(void),
    void (*end_persistent_attrs)(void),
    int (*load_persistent_attrs)(hgobj gobj, json_t *jn_attrs),
    int (*save_persistent_attrs)(hgobj gobj, json_t *jn_attrs),
    int (*remove_persistent_attrs)(hgobj gobj, json_t *jn_attrs),
    json_t * (*list_persistent_attrs)(hgobj gobj, json_t *jn_attrs),
    json_function_t global_command_parser,
    json_function_t global_stats_parser,
    authz_checker_fn global_authz_checker,
    authenticate_parser_fn global_authenticate_parser
);

PUBLIC int yuneta_entry_point(int argc, char *argv[],
    const char *APP_NAME,
    const char *APP_VERSION,
    const char *APP_SUPPORT,
    const char *APP_DOC,
    const char *APP_DATETIME,
    const char *fixed_config,
    const char *variable_config,
    void (*register_yunos_and_more)(void)
);

/*
 *  Assure to kill yuno if doesn't respond to first kill order
 *  Default value: 5 seconds
 */
PUBLIC void set_assure_kill_time(int seconds);

/*
 *  For TEST: kill the yuno in `timeout` seconds.
 */
PUBLIC void set_auto_kill_time(int seconds);

/*
 *  By defaul the daemons are quick death.
 *  If you want a ordered death, set this true.
 */
PUBLIC void set_ordered_death(BOOL quick_death);

#ifdef __cplusplus
}
#endif
