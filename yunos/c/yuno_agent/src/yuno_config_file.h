/****************************************************************************
 *          yuno_config_file.h
 *
 *          The configuration files the agent writes for a yuno it runs
 *          (bin/<n>-<role>^<name>.json): what the yuno is started with.
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
 *              Constants
 ***************************************************************/
/*
 *  The mode of a yuno's configuration file
 */
#define YUNO_CONFIG_FILE_PERMISSION 0640

/***************************************************************
 *              Prototypes
 ***************************************************************/
/*
 *  Write `gbuf` (owned) to the configuration file `path` of a yuno,
 *  replacing it. Returns 0, or -1 (logged).
 */
PUBLIC int write_yuno_config_file(
    hgobj gobj,
    gbuffer_t *gbuf,
    const char *path
);

#ifdef __cplusplus
}
#endif
