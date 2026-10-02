/****************************************************************************
 *          ydaemon.c
 *
 *  Parent → daemon_catch_signals() → ignores signals → pure waitpid().
 *  Child → daemon_catch_signals_child() → installs signalfd() → clean shutdown → _exit().
 *
 *          Copyright (c) 2014-2018 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#ifdef __linux__
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <signal.h>
#include "entry_point.h"
#include "ydaemon.h"

/******************************************************
 *      Constants
 ******************************************************/

/******************************************************
 *      Data
 ******************************************************/
PRIVATE volatile int relaunch_times = 0;
PRIVATE volatile int debug = 0;
PRIVATE volatile int exit_code;
PRIVATE volatile int signal_code;
PRIVATE volatile int watcher_pid = 0;
PRIVATE volatile sig_atomic_t stop_requested = 0;   // the watcher got SIGQUIT: no relaunch
PRIVATE const char *pid_file = NULL;

/***************************************************************************
 *  Parent → daemon_catch_signals() → ignores signals → pure waitpid().
 *  Child → daemon_catch_signals_child() → installs signalfd() → clean shutdown → _exit().
 *
 *  SIGQUIT, the stop, is not ignored by the watcher: it is noted, and the
 *  child that ends after it is not relaunched, whatever its end (SA_RESTART:
 *  the waitpid() goes on). Up to 7.25.22 it was ignored, and an agent that
 *  crashed in its orderly stop was relaunched 2 s later.
 ***************************************************************************/
PRIVATE void on_watcher_sigquit(int sig)
{
    stop_requested = 1;
}

PRIVATE void daemon_catch_signals(void)
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGALRM, SIG_IGN);
    signal(SIGINT, SIG_IGN);     // ctrl+c
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_watcher_sigquit;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGQUIT, &sa, NULL);
}

/***************************************************************************
 *  The pid of the watcher in `pid_file`, whole or not at all (written aside
 *  and renamed): systemd reads it as soon as the process it started exits
 ***************************************************************************/
PRIVATE void write_pid_file(pid_t pid)
{
    char tmp[PATH_MAX];
    if(snprintf(tmp, sizeof(tmp), "%s.tmp", pid_file) >= (int)sizeof(tmp)) {
        print_error(0, "pid file path too long: %s", pid_file);
        return;
    }
    int fd = open(tmp, O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, 0644);
    if(fd < 0) {
        print_error(0, "Cannot create the pid file %s, errno %d %s", tmp, errno, strerror(errno));
        return;
    }
    char bf[32];
    int len = snprintf(bf, sizeof(bf), "%d\n", (int)pid);
    if(write(fd, bf, (size_t)len) != len) {
        print_error(0, "Cannot write the pid file %s, errno %d %s", tmp, errno, strerror(errno));
        close(fd);
        unlink(tmp);
        return;
    }
    close(fd);
    if(rename(tmp, pid_file) < 0) {
        print_error(0, "Cannot rename the pid file to %s, errno %d %s", pid_file, errno, strerror(errno));
        unlink(tmp);
    }
}

/***************************************************************************
 *  First fork
 ***************************************************************************/
PRIVATE void continue_as_daemon(const char *work_dir, const char *process_name)
{
    /* Our process ID and Session ID */
    pid_t pid, sid;

    /*
     *  From fork(2) - Linux man page
     *
     *  On success, the PID of the child process is returned in the parent,
     *  and 0 is returned in the child.
     *  On failure, -1 is returned **in the parent**, no child process is created,
     *  and errno is set appropriately.
     */

    /*
     *  Fork off the parent process
     *  The first fork will change our pid
     *  but the sid and pgid will be the calling process.
     */
    pid = fork();

    switch(pid) {
        case -1:
            print_error(PEF_EXIT, "fork1 FAILED, errno %d %s", errno, strerror(errno));
            break;
        case 0:
            break;                  // child falls through
        default:
            /* If we got a good PID, then we can exit the parent process. */
            if(pid_file) {
                write_pid_file(pid);    // the child is the watcher
            }
            _exit(EXIT_SUCCESS);   // parent terminates
    }

    watcher_pid = getpid();

    /*
     *  Create a new SID for the child process
     *  Run the process in a new session without a controlling
     *  terminal. The process group ID will be the process ID
     *  and thus, the process will be the process group leader.
     *  After this call the process will be in a new session,
     *  and it will be the progress group leader in a new
     *  process group.
     */
    sid = setsid(); // become leader of new session
    if (sid < 0) {
        print_error(PEF_EXIT, "setsid() FAILED, errno %d %s", errno, strerror(errno));
    }
}

/***************************************************************************
 *  Second fork
 ***************************************************************************/
PRIVATE int relauncher(
    void (*process) (
        const char *process_name,
        const char *work_dir,
        const char *domain_dir,
        void (*cleaning_fn)(void)
    ),
    const char *process_name,
    const char *work_dir,
    const char *domain_dir,
    void (*cleaning_fn)(void)
) {
    if(debug) {
        print_error(0, "Relaunches %d times, process %s, pid %d",
            relaunch_times,
            process_name,
            getpid()
        );
    }

    /*
     *  We will fork again, also known as a
     *  double fork. This second fork will orphan
     *  our process because the parent will exit.
     *  When the parent process exits the child
     *  process will be adopted by the init process
     *  with process ID 1.
     *  The result of this second fork is a process
     *  with the parent as the init process with an ID
     *  of 1. The process will be in it's own session
     *  and process group and will have no controlling
     *  terminal. Furthermore, the process will not
     *  be the process group leader and thus, cannot
     *  have the controlling terminal if there was one.
     */
    pid_t pid = fork();
    if(debug) {
        print_error(0, "fork return pid %d, process %s, pid %d",
            pid,
            process_name,
            getpid()
        );
    }

    if (pid < 0) {
        print_error(PEF_EXIT, "fork2 FAILED, errno %d %s", errno, strerror(errno));
        return 1;
    } else if (pid > 0) {
        /*------------------------*
         *  we are the parent
         *------------------------*/
        daemon_catch_signals();
        int status;
        if(waitpid(pid, &status, 0) == -1) {
            print_error(PEF_EXIT,
                "waitpid() failed, errno %d %s", errno, strerror(errno)
            );
        }
        exit_code = 0;
        signal_code = 0;
        if(debug) {
            print_error(0, "waitpid() return status %d", status);
        }

        if(stop_requested) {
            return 1;   // stopped: whatever its end, not relaunched
        }

        if(WIFSIGNALED(status)) {
            signal_code = (int)(int8_t)(WTERMSIG(status));
            if(debug) {
                print_error(0, "Process child signalized with signal %d, process %s, pid %d",
                    signal_code,
                    process_name,
                    getpid()
                );
            }
            if(signal_code == SIGKILL) {
                return 1; // Exit
            }
            return -1; // relaunch

        } else if(WIFEXITED(status)) {
            exit_code = (int)(int8_t)(WEXITSTATUS(status));
            if(debug) {
                print_error(0, "Process child exiting with code %d, process %s, pid %d",
                    exit_code,
                    process_name,
                    getpid()
                );
            }
            if(exit_code) {
                // relaunch the child
                return -1;
            }
            return 1;

        } else {
            print_error(0, "Case not implemented, process %s, pid %d, status %d",
                process_name,
                getpid(),
                status
            );
        }
        return -1; // relaunch

    } else {
        /*------------------------*
         *  we are the child
         *  (return 0)
         *------------------------*/
        /* Clear umask to enable explicit file modes. */
        umask(0);

        /* Change the current working directory */
        if(!empty_string(work_dir)) {
            if(chdir(work_dir) < 0) {
                gobj_log_error(0, 0,
                    "gobj",             "%s", __FILE__,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_SYSTEM,
                    "msg",              "%s", "chdir() FAILED",
                    "work_dir",         "%s", work_dir,
                    "errno",            "%d", errno,
                    "serror",           "%s", strerror(errno),
                    NULL
                );
            }
        }

        gobj_trace_msg(0, "\n"); // Blank line
        char temp[120];
        snprintf(temp, sizeof(temp), "\n=====> Starting yuno '%s', times: %d, pid: %d\n",
            process_name,
            relaunch_times,
            (int)getpid());
        gobj_trace_msg(0, "%s", temp);

        if(relaunch_times > 0) {
            gobj_log_error(0,0,
                "gobj",             "%s", __FILE__,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_SYSTEM,
                "msg",              "%s", "Daemon relaunched",
                "process",          "%s", process_name,
                "pid",              "%d", (int)getpid(),
                "relaunch_times",   "%d", relaunch_times,
                "signal_code",      "%d", signal_code,
                "exit_code",        "%d", exit_code,
                NULL
            );
        } else {
            gobj_log_debug(0,0,
                "gobj",             "%s", __FILE__,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_INFO,
                "msg",              "%s", "Daemon started",
                "process",          "%s", process_name,
                "pid",              "%d", (int)getpid(),
                "relaunch_times",   "%d", relaunch_times,
                NULL
            );
        }

        process(process_name, work_dir, domain_dir, cleaning_fn);
        if(debug) {
            gobj_log_debug(0,0,
                "gobj",             "%s", __FILE__,
                "function",         "%s", __FUNCTION__,
                "msgset",           "%s", MSGSET_INFO,
                "msg",              "%s", "*process* returned",
                "process",          "%s", process_name,
                "pid",              "%d", (int)getpid(),
                "relaunch_times",   "%d", relaunch_times,
                NULL
            );
        }
        return 0;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
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
)
{
    int ret;

    continue_as_daemon(work_dir, process_name);
    while((ret=relauncher(process, process_name, work_dir, domain_dir, cleaning_fn))<0) {
        // sleep 2 sec and launch again while relauncher return negative
        relaunch_times++;
        sleep(2);
        if(stop_requested) {
            ret = 1;    // stopped while waiting to relaunch
            break;
        }
    }
    if(ret==1) { // the watcher returns 1
        if(debug) {
            print_error(0, "Watcher exiting, process %s, pid %d",
                process_name,
                getpid()
            );
        }
        // parent doesn't return
        _exit(0);
    }
    if(debug) {
        print_error(0, "relauncher returning, process %s, pid %d",
            process_name,
            getpid()
        );
    }
    return 0;
}

/***************************************************************************
 *  Stop the daemon: every process of its name (the watcher and its child)
 *  is asked to end (SIGQUIT), the watchers first: a watcher notes it and
 *  does not relaunch its child, whatever its end; the child shuts down in
 *  order and exits, and its watcher with it. They are given STOP_WAIT_MS to
 *  be gone. Then every process of the name still there is killed --
 *  collected again, so a child relaunched meanwhile is not left an orphan.
 *  Each signal is checked and said when it fails (another user's process:
 *  EPERM). Return 0 when every process of the name is gone or killed, -1
 *  if one could not be signalled.
 *  Up to 7.25.21 each was killed 1 s after its SIGQUIT, one after the
 *  other. Up to 7.25.22 kill() was not checked (EPERM waited 10 s, said
 *  "killed" and exited 0), and an agent that crashed in its stop was
 *  relaunched by its watcher and left alive.
 ***************************************************************************/
#define STOP_WAIT_MS    (10*1000)
#define MAX_STOP_PIDS   64

typedef struct {
    pid_t pids[MAX_STOP_PIDS];
    int n;
} stop_pids_t;

PRIVATE void collect_proc(void *self, const char *name, pid_t pid)
{
    stop_pids_t *stop = self;
    if(pid == getpid() || pid <= 0) {
        return; // I am the killer
    }
    if(stop->n >= MAX_STOP_PIDS) {
        print_error(0, "--stop: more than %d processes named %s, pid %d left alone",
            MAX_STOP_PIDS, name, (int)pid
        );
        return;
    }
    stop->pids[stop->n++] = pid;
}

/*
 *  The fields of /proc/<pid>/stat after the command: its state and its
 *  parent. FALSE when it is gone.
 */
PRIVATE BOOL stop_pid_stat(pid_t pid, char *state, pid_t *ppid)
{
    char path[PATH_MAX];
    char bf[512];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    int fd = open(path, O_RDONLY|O_CLOEXEC);
    if(fd < 0) {
        return FALSE;
    }
    ssize_t n = read(fd, bf, sizeof(bf) - 1);
    close(fd);
    if(n <= 0) {
        return FALSE;
    }
    bf[n] = 0;
    const char *p = strrchr(bf, ')');
    int pp = 0;
    if(!p || sscanf(p + 1, " %c %d", state, &pp) != 2) {
        return FALSE;
    }
    *ppid = (pid_t)pp;
    return TRUE;
}

/*
 *  Gone: it does not exist, or it is a zombie (dead, its parent has not
 *  reaped it yet)
 */
PRIVATE BOOL stop_pid_is_gone(pid_t pid)
{
    if(kill(pid, 0) < 0 && errno == ESRCH) {
        return TRUE;
    }
    char state;
    pid_t ppid;
    if(!stop_pid_stat(pid, &state, &ppid)) {
        return TRUE;
    }
    return (state == 'Z')? TRUE : FALSE;
}

/*
 *  A watcher: its parent is not a process of the name (a child's parent is
 *  its watcher)
 */
PRIVATE BOOL stop_pid_is_watcher(stop_pids_t *stop, pid_t pid)
{
    char state;
    pid_t ppid;
    if(!stop_pid_stat(pid, &state, &ppid)) {
        return FALSE;
    }
    for(int i = 0; i < stop->n; i++) {
        if(stop->pids[i] == ppid) {
            return FALSE;
        }
    }
    return TRUE;
}

PRIVATE int stop_signal(const char *process_name, pid_t pid, int sig)
{
    if(kill(pid, sig) < 0) {
        if(errno == ESRCH) {
            return 0;   // gone meanwhile
        }
        print_error(0, "--stop: cannot signal %s pid %d (%s): errno %d %s",
            process_name, (int)pid, sig == SIGKILL? "SIGKILL" : "SIGQUIT",
            errno, strerror(errno)
        );
        return -1;
    }
    return 0;
}

PUBLIC int daemon_shutdown(const char *process_name)
{
    int ret = 0;
    stop_pids_t stop = {0};
    search_process(process_name, collect_proc, &stop);

    /*
     *  The watchers first (they then do not relaunch), then their children
     *  (soft exit, let them delete the pid file). One that cannot be
     *  signalled is not waited for.
     */
    BOOL watcher[MAX_STOP_PIDS] = {0};
    pid_t refused[MAX_STOP_PIDS] = {0};
    for(int i = 0; i < stop.n; i++) {
        watcher[i] = stop_pid_is_watcher(&stop, stop.pids[i]);
    }
    for(int pass = 0; pass < 2; pass++) {
        for(int i = 0; i < stop.n; i++) {
            if(!stop.pids[i] || watcher[i] != (pass == 0)) {
                continue;
            }
            if(stop_signal(process_name, stop.pids[i], SIGQUIT) < 0) {
                ret = -1;
                refused[i] = stop.pids[i];
                stop.pids[i] = 0;
            }
        }
    }

    uint64_t wait_until = start_msectimer(STOP_WAIT_MS);
    while(TRUE) {
        int alive = 0;
        for(int i = 0; i < stop.n; i++) {
            if(stop.pids[i] && stop_pid_is_gone(stop.pids[i])) {
                stop.pids[i] = 0;
            }
            if(stop.pids[i]) {
                alive++;
            }
        }
        if(!alive || test_msectimer(wait_until)) {
            break;
        }
        usleep(100*1000);
    }

    /*
     *  What is left of the name now, collected again: a child relaunched
     *  while the stop ran is not in the first list
     */
    stop_pids_t left = {0};
    search_process(process_name, collect_proc, &left);
    for(int i = 0; i < left.n; i++) {
        if(stop_pid_is_gone(left.pids[i])) {
            continue;
        }
        BOOL first = FALSE;
        BOOL was_refused = FALSE;
        for(int j = 0; j < stop.n; j++) {
            if(stop.pids[j] == left.pids[i]) {
                first = TRUE;
            }
            if(refused[j] == left.pids[i]) {
                was_refused = TRUE;
            }
        }
        if(was_refused) {
            continue;   // said already
        }
        if(stop_signal(process_name, left.pids[i], SIGKILL) < 0) {
            ret = -1;
            continue;
        }
        print_error(0, "--stop: %s pid %d %s: killed (SIGKILL)",
            process_name, (int)left.pids[i],
            first? "still alive after the wait" : "started while the stop ran"
        );
    }
    return ret;
}

/***************************************************************************
 *  Search process by his name and exec
 *  Return number of processes found.
 ***************************************************************************/
PRIVATE int linux_search_process(
    const char *process_name,
    void (*cb)(void *self, const char *name, pid_t pid),
    void *self)
{
    int found = 0;
    pid_t pid;
    glob_t pglob;
    char *procname, *readbuf;
    int buflen = (int)strlen(process_name) + 2;
    unsigned i;

    /* Get a list of all comm files. man 5 proc */
    if (glob("/proc/*/comm", 0, NULL, &pglob) != 0) {
        return 0;
    }

    /* The comm files include trailing newlines, so... */
    procname = gbmem_malloc(buflen);
    if(!procname) {
        // Error already logged
        globfree(&pglob);
        return 0;
    }
    strcpy(procname, process_name);
    procname[buflen - 2] = '\n';
    procname[buflen - 1] = 0;

    /* readbuff will hold the contents of the comm files. */
    readbuf = gbmem_malloc(buflen);
    if(!readbuf) {
        // Error already logged
        gbmem_free(procname);
        globfree(&pglob);
        return 0;
    }

    for (i = 0; i < pglob.gl_pathc; ++i) {
        FILE *comm;
        char *ret;

        /* Read the contents of the file. */
        if ((comm = fopen(pglob.gl_pathv[i], "r")) == NULL) {
            continue;
        }
        ret = fgets(readbuf, buflen, comm);
        fclose(comm);
        if (ret == NULL) {
            continue;
        }

        /*
         *  If comm matches our process name, extract the process ID from the
         *  path, convert it to a pid_t, and call callback function.
        */
        int n = strlen(procname);
        if(n > 15) {
            n = 15;
        }
        if (strncmp(readbuf, procname, n) == 0) {
            pid = (pid_t)atoi(pglob.gl_pathv[i] + strlen("/proc/"));
            if(cb) {
                cb(self, procname, pid);
            }
            found ++;
        }
    }

    /* Clean up. */
    gbmem_free(procname);
    gbmem_free(readbuf);
    globfree(&pglob);

    return found;
}

/***************************************************************************
 *  Search process by his name and exec callback if not null.
 *  Return number of processes found.
 ***************************************************************************/
PUBLIC int search_process(
    const char *process_name,
    void (*cb)(void *self, const char *name, pid_t pid),
    void *self)
{
    return linux_search_process(process_name, cb, self);
}

/***************************************************************************
 *  Return number relaunch_times
 ***************************************************************************/
PUBLIC int get_relaunch_times(void)
{
    return relaunch_times;
}

/***************************************************************************
 *  Set debug mode
 ***************************************************************************/
PUBLIC int daemon_set_debug_mode(BOOL set)
{
    debug = set?1:0;
    return 0;
}

/***************************************************************************
 *  Get debug mode
 ***************************************************************************/
PUBLIC BOOL daemon_get_debug_mode(void)
{
    return debug;
}

/***************************************************************************
 *  See ydaemon.h
 ***************************************************************************/
PUBLIC void daemon_set_pid_file(const char *path)
{
    pid_file = (path && *path)? path : NULL;
}

/***************************************************************************
 *  Get debug mode
 ***************************************************************************/
PUBLIC int get_watcher_pid(void)
{
    return watcher_pid;
}

#endif /* __linux__ */
