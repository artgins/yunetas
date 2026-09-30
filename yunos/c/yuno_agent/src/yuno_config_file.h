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

/*
 *  Narrow to YUNO_CONFIG_FILE_PERMISSION (never widen) every
 *  <n>-<role_plus_name>.json of `bin_path` with n > n_written: the
 *  configuration files of an earlier launch that wrote more of them, which
 *  this launch did not rewrite. They are not removed: the launch script
 *  does not name them, and an operator may still want to read them.
 *  A symbolic link or anything but a regular file is left as it is.
 *  Returns 0, or -1 if one could not be narrowed (logged, the others are
 *  still narrowed).
 */
PUBLIC int narrow_stale_yuno_config_files(
    hgobj gobj,
    const char *bin_path,
    const char *role_plus_name,
    int n_written
);

#ifdef __cplusplus
}
#endif
