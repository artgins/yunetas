/****************************************************************************
 *          COMMAND_PARSER.H
 *
 *          Command parser
 *
 *          Copyright (c) 2017-2023 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
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
    const char *command
);

/*
 *  What a trace shows of a command: its SDF_SECRET parameters (by the
 *  command table of `gobj`) and the keys with a secret's name
 *  (is_secret_name(): the free keys of a SDF_WILD_CMD command) masked as
 *  "********", whatever the json type of the value.
 *  command_mask_secret_kw() answers a NEW reference (a masked copy, or kw
 *  itself when nothing is secret; NULL for a NULL kw): decref it.
 *  command_mask_secret_line() answers a gbmem string ("name key=value..."
 *  with the secret values masked, a value longer than 1 KB shown as
 *  "<N bytes>"): GBMEM_FREE it.
 */
PUBLIC json_t *command_mask_secret_kw(
    hgobj gobj,
    const char *command,    // "name [parameters]"
    json_t *kw              // not owned
);
PUBLIC char *command_mask_secret_line(
    hgobj gobj,
    const char *command     // "name [parameters]"
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
