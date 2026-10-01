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
 *  replacing it: the content goes to a new file of the agent, mode
 *  YUNO_CONFIG_FILE_PERMISSION, in the same directory, renamed over `path`.
 *  The directory must be writable by the agent. A symbolic link at `path`
 *  is replaced, not followed. On failure `path` is left as it was.
 *  Returns 0, or -1 (logged).
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
 *  The temporary files of write_yuno_config_file() that an interrupted
 *  write left (.config.XXXXXX, and .<n>-<role_plus_name>.json.XXXXXX of
 *  7.25.21; regular files only) are removed, logged: call it after this
 *  launch's writes.
 *  Returns 0, or -1 if one could not be narrowed or removed (logged, the
 *  others are still done).
 */
PUBLIC int narrow_stale_yuno_config_files(
    hgobj gobj,
    const char *bin_path,
    const char *role_plus_name,
    int n_written
);

/*
 *  Remove the temporary files of write_yuno_config_file() in `bin_path`,
 *  the bin directory of one yuno: .config.XXXXXX, and the
 *  .<n>-<role>^<name>.json.XXXXXX of 7.25.21 whatever their yuno. Regular
 *  files only, each logged. Only a write the agent did not finish leaves
 *  one, so the agent runs it over every yuno at its start, when no write is
 *  under way: a yuno that is not launched again keeps none.
 *  A bin directory that does not exist is no error.
 *  Returns 0, or -1 if one could not be removed (logged).
 */
PUBLIC int remove_temp_yuno_config_files(
    hgobj gobj,
    const char *bin_path
);

#ifdef __cplusplus
}
#endif
