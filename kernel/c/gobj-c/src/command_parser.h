/****************************************************************************
 *          COMMAND_PARSER.H
 *
 *          Command parser
 *
 *          Copyright (c) 2017-2023 Niyamaka.
 *
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include "gobj.h"

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************
 *              Prototypes
 **************************************************/
PUBLIC json_t *command_parser(
    hgobj gobj,
    const char *command,
    json_t *kw,
    hgobj src
);

PUBLIC json_t *gobj_build_cmds_doc(
    hgobj gobj,
    json_t *kw
);

/*
 *  To commands and stats
 */
PUBLIC json_t *build_command_response( // OLD build_webix()
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment, // owned
    json_t *jn_schema,  // owned
    json_t *jn_data     // owned
);

PUBLIC const sdata_desc_t *command_get_cmd_desc(
    const sdata_desc_t *command_table,
    const char *cmd
);

/*
 *  Search a command in gobj, if not found then
 *      level == 1 search in bottom_gobjs
 *      level == 2 search in all children
 */
PUBLIC const sdata_desc_t *search_command_desc(
    hgobj gobj,
    const char *command,
    int level,
    hgobj *gobj_found
);

#ifdef __cplusplus
}
#endif
