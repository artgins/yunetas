/****************************************************************************
 *          ENTRY_POING.H
 *          Entry point for yunos (yuneta daemons).
 *
 *          Copyright (c) 2014-2015 Niyamaka, 2024- ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <ginsfsm.h>
#include "msglog_yuneta.h"
#include "yuneta_version.h"
#include "yuneta_register.h"
#include "yuneta_environment.h"
#include "dbsimple.h"
#include "dbsimple2.h"
#include "c_yuno.h"         // the grandmother

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *  Prototypes
 *********************************************************************/
/*
 *  Set functions of gobj_start_up() function.
 *  DEPRECATED use yuneta_setup()
 */
PUBLIC int yuneta_set_gobj_startup_functions(
    int (*load_persistent_attrs)(hgobj gobj, json_t *jn_attrs),
    int (*save_persistent_attrs)(hgobj gobj, json_t *jn_attrs),
    int (*remove_persistent_attrs)(hgobj gobj, json_t *jn_attrs),
    json_t * (*list_persistent_attrs)(hgobj gobj, json_t *jn_attrs),
    json_function_t global_command_parser,
    json_function_t global_stats_parser
);

/*
 *  New yuneta setup function.
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
