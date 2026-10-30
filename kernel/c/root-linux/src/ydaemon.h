/****************************************************************************
 *          ydaemon.h
 *
 *  The design of the watcher (a first fork that stays and watches, a
 *  second one that runs the yuno and is relaunched when it fails) follows
 *  the one of daemon.c of NXWEB (https://github.com/yarosla/nxweb, by
 *  Yaroslav Stavnichiy). No code of it is here.
 *
 *          Copyright (c) 2013 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <signal.h>
#include <gobj.h>

#ifdef __cplusplus
extern "C"{
#endif

/*********************************************************************
 *      Prototypes
 *********************************************************************/
#ifdef __linux__
PUBLIC int get_watcher_pid(void);
PUBLIC int daemon_shutdown(const char *process_name); // 0, or -1 if a process could not be signalled

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

/*
 *  The signals a yuno reads from its signalfd (C_YUNO): SIGPIPE, SIGTERM,
 *  SIGALRM, SIGQUIT, SIGINT, SIGUSR1 and SIGUSR2. A yuno keeps them blocked
 *  from its start (yuneta_entry_point()) and every child of the watcher
 *  from its fork, so one that comes before the signalfd waits for it.
 */
PUBLIC void get_yuno_signal_set(sigset_t *set);
PUBLIC int daemon_set_debug_mode(BOOL set);
PUBLIC BOOL daemon_get_debug_mode(void);

#endif  /* __linux__ */

#ifdef __cplusplus
}
#endif
