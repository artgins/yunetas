/****************************************************************************
 *          COMMAND_PARSER.H
 *
 *          Command parser
 *
 *          Copyright (c) 2017 Niyamaka.
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
PUBLIC json_t * command_parser(
    hgobj gobj,
    const char *command,
    json_t *kw,
    hgobj src
);

PUBLIC json_t *gobj_build_cmds_doc(
    hgobj gobj,
    json_t *kw
);

PUBLIC const sdata_desc_t *command_get_cmd_desc(
    const sdata_desc_t *command_table,
    const char *cmd
);

PUBLIC json_t *expand_command(
    const char *gobj_name,
    const sdata_desc_t *command_table,
    const char *command,
    json_t *kw,     // NOT owned
    const sdata_desc_t **cmd_desc
);

PUBLIC json_t *build_cmd_kw(
    const char *gobj_name,
    const char *command,
    const sdata_desc_t *cnf_cmd,
    char *parameters,
    json_t *kw, // not owned
    int *result
);

#ifdef __cplusplus
}
#endif
