/****************************************************************************
 *              RUN_EXECUTABLE.H
 *              Run a executable
 *
 *              Copyright (c) 2015 Niyamaka.
*               Copyright (c) 2025, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#pragma once

/*
 *  Dependencies
 */
#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

PUBLIC int run_command(  // use popen(), synchronous
    const char *command,
    char *bf,   // buffer to put the output of command
    size_t bfsize
);

PUBLIC int run_process2(  // use fork(), synchronous
    const char *path, char *const argv[]
);

PUBLIC int pty_sync_spawn( // user forkpty() execlp(), synchronous
    const char *command
);

#ifdef __cplusplus
}
#endif
