/****************************************************************************
 *          ydaemon.h
 *
 *  Work inspired in daemon.c from NXWEB project.
 *  https://github.com/yarosla/nxweb
 *  Copyright (c) 2011-2012 Yaroslav Stavnichiy <yarosla@gmail.com>
 *
 *          Copyright (c) 2013 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Prototypes
 *********************************************************************/
#ifdef __linux__
PUBLIC int get_watcher_pid(void);
PUBLIC void daemon_shutdown(const char *process_name);

/*
 *  With --start: the file where the pid of the WATCHER (the process that
 *  stays, and relaunches the yuno) is written, before the process that was
 *  started returns -- what a systemd unit of Type=forking reads as PIDFile=.
 *  `path` must live until daemon_run() (an argv string does).
 */
PUBLIC void daemon_set_pid_file(const char *path);
PUBLIC int daemon_run(
    void (*process)(
        const char *process_name,
        const char *work_dir,
        const char *domain_dir,
        void (*cleaning_fn)(void)
    ),
    const char *process_name,
    const char *work_dir,
    const char *domain_dir,
    void (*cleaning_fn)(void)
);

PUBLIC int search_process(
    const char *process_name,
    void (*cb)(void *self, const char *name, pid_t pid),
    void *self
);
PUBLIC int get_relaunch_times(void);
PUBLIC int daemon_set_debug_mode(BOOL set);
PUBLIC BOOL daemon_get_debug_mode(void);

#endif  /* __linux__ */

#ifdef __cplusplus
}
#endif
