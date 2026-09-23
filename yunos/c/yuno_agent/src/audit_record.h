/****************************************************************************
 *          audit_record.h
 *
 *          What the agent writes to its audit file for one command.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C"{
#endif

/***************************************************************
 *              Prototypes
 ***************************************************************/
/*
 *  Build the audit record of a command. `kw` is not owned and not
 *  modified. Return a new json object (yours), NULL on error (logged).
 */
PUBLIC json_t *audit_record_build(
    const char *command,
    json_t *kw,         // not owned
    const char *date
);

#ifdef __cplusplus
}
#endif
