/****************************************************************************
 *          ydaemon.h
 *
 *  Work inspired in daemon.c from NXWEB project.
 *  https://github.com/yarosla/nxweb
 *  Copyright (c) 2011-2012 Yaroslav Stavnichiy <yarosla@gmail.com>
 *
 *          Copyright (c) 2013 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
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
PUBLIC int daemon_run(
    void (*process)(const char *process_name, const char *work_dir, const char *domain_dir),
    const char *process_name,
    const char *work_dir,
    const char *domain_dir,
    void (*catch_signals)(void)
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
