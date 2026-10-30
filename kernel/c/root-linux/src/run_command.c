/****************************************************************************
 *              RUN_EXECUTABLE.C
 *              Run a executable
 *
 *              Copyright (c) 2015 Niyamaka.
 *              Copyright (c) 2025-2026, ArtGins.
 *              All Rights Reserved.
 ****************************************************************************/
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <poll.h>
#include <pty.h>
#include <spawn.h>
#include "run_command.h"

/***************************************************************************
 *      Constants
 ***************************************************************************/
/*
 *  Kernel headers older than 5.3 have no SYS_pidfd_open: its number is the
 *  same on these architectures. Elsewhere pty_sync_spawn() looks for the
 *  end of its command every 100 ms.
 */
#if !defined(SYS_pidfd_open) && (defined(__x86_64__) || defined(__i386__) || \
        defined(__aarch64__) || defined(__arm__) || defined(__riscv))
    #define SYS_pidfd_open 434
#endif

/***************************************************************************
 *      Prototypes
 ***************************************************************************/

#if !(defined(__GLIBC__) && __GLIBC_PREREQ(2, 34))
/***************************************************************************
 *  A C library with no posix_spawn_file_actions_addclosefrom_np() (glibc
 *  older than 2.34, that a source build may still have): each fd above
 *  stderr open now is closed one by one. 0 or the error.
 *
 *  posix_spawn_file_actions_addclose() refuses (EBADF) an fd at or above
 *  the soft limit of open files, which one opened before the limit was
 *  lowered is: that one gets O_CLOEXEC instead, and the exec of the
 *  program closes it. A yuno never execs itself, so the flag changes
 *  nothing for it. Without it, such an fd would fail the whole spawn.
 ***************************************************************************/
PRIVATE int add_close_of_open_fds(posix_spawn_file_actions_t *actions)
{
    DIR *d = opendir("/proc/self/fd");
    if(!d) {
        return errno;
    }
    long soft_limit = sysconf(_SC_OPEN_MAX);
    int ret = 0;
    int dfd = dirfd(d);
    struct dirent *e;
    while(ret == 0 && (e = readdir(d)) != NULL) {
        char *end;
        long fd = strtol(e->d_name, &end, 10);
        if(end == e->d_name || *end || fd <= STDERR_FILENO || fd == dfd) {
            continue;
        }
        if(soft_limit > 0 && fd >= soft_limit) {
            int flags = fcntl((int)fd, F_GETFD);
            if(flags < 0 || fcntl((int)fd, F_SETFD, flags|FD_CLOEXEC) < 0) {
                ret = errno;
            }
            continue;
        }
        ret = posix_spawn_file_actions_addclose(actions, (int)fd);
    }
    closedir(d);
    return ret;
}
#endif

/***************************************************************************
 *  What a program run from here starts with, whatever the caller has: no
 *  signal blocked, every signal at its default, and no fd open but stdin,
 *  stdout and stderr (posix_spawn_file_actions_addclosefrom_np(), or one
 *  by one on a glibc older than 2.34). With `out_fd` >= 0 (run_command())
 *  its stdout and stderr go to `out_fd` and its stdin is /dev/null.
 *
 *  A yuno keeps its signals blocked for its signalfd, and a blocked or an
 *  ignored signal stays so across exec: left so, every program a yuno ran
 *  would have SIGTERM, SIGINT, SIGQUIT, SIGALRM, SIGUSR1, SIGUSR2 and
 *  SIGPIPE blocked, so a `timeout` it ran could not stop its child with
 *  SIGTERM, nor a ctrl+c the command of a console. And it would inherit
 *  every fd of the yuno without O_CLOEXEC, and hold it open after the yuno
 *  closed it.
 *
 *  NULL when both are made (destroy both), else the call that failed, its
 *  error in *perr, and nothing to destroy.
 ***************************************************************************/
PRIVATE const char *spawn_setup(
    posix_spawn_file_actions_t *actions,
    posix_spawnattr_t *attr,
    int out_fd,
    int *perr
)
{
    const char *failed = NULL;
    int ret = posix_spawn_file_actions_init(actions);
    if(ret != 0) {
        *perr = ret;
        return "posix_spawn_file_actions_init()";
    }
    if(out_fd >= 0) {
        ret = posix_spawn_file_actions_addopen(actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        if(ret != 0) {
            failed = "posix_spawn_file_actions_addopen(/dev/null)";
        }
        if(!failed) {
            ret = posix_spawn_file_actions_adddup2(actions, out_fd, STDOUT_FILENO);
            if(ret != 0) {
                failed = "posix_spawn_file_actions_adddup2(stdout)";
            }
        }
        if(!failed) {
            ret = posix_spawn_file_actions_adddup2(actions, out_fd, STDERR_FILENO);
            if(ret != 0) {
                failed = "posix_spawn_file_actions_adddup2(stderr)";
            }
        }
    }
    if(!failed) {
#if defined(__GLIBC__) && __GLIBC_PREREQ(2, 34)
        ret = posix_spawn_file_actions_addclosefrom_np(actions, STDERR_FILENO + 1);
        if(ret != 0) {
            failed = "posix_spawn_file_actions_addclosefrom_np()";
        }
#else
        ret = add_close_of_open_fds(actions);
        if(ret != 0) {
            failed = "posix_spawn_file_actions_addclose() of /proc/self/fd";
        }
#endif
    }
    if(failed) {
        posix_spawn_file_actions_destroy(actions);
        *perr = ret;
        return failed;
    }

    ret = posix_spawnattr_init(attr);
    if(ret != 0) {
        posix_spawn_file_actions_destroy(actions);
        *perr = ret;
        return "posix_spawnattr_init()";
    }
    sigset_t none;
    sigset_t all;
    sigemptyset(&none);
    sigfillset(&all);
    ret = posix_spawnattr_setsigmask(attr, &none);
    if(ret != 0) {
        failed = "posix_spawnattr_setsigmask()";
    }
    if(!failed) {
        ret = posix_spawnattr_setsigdefault(attr, &all);
        if(ret != 0) {
            failed = "posix_spawnattr_setsigdefault()";
        }
    }
    if(!failed) {
        ret = posix_spawnattr_setflags(attr, POSIX_SPAWN_SETSIGMASK|POSIX_SPAWN_SETSIGDEF);
        if(ret != 0) {
            failed = "posix_spawnattr_setflags()";
        }
    }
    if(failed) {
        posix_spawnattr_destroy(attr);
        posix_spawn_file_actions_destroy(actions);
        *perr = ret;
        return failed;
    }
    return NULL;
}

/***************************************************************************
 *  Run a command with /bin/sh -c and get its output, stdout and stderr in
 *  one, when it ends. Synchronous. NULL (logged) when it cannot be run.
 *  The command starts as spawn_setup() says: clean signals, no fd of the
 *  caller, stdin on /dev/null, not the caller's. The command is passed
 *  whole, of any length, and stdout and stderr of every command of a list
 *  go to the output.
 *
 *  Its end is the end of its output: every process that still has the
 *  command's stdout or stderr holds it, a process left in the background
 *  too ("sleep 9 >/dev/null &" holds it 9 s: its stderr is the pipe). Such
 *  a process must have both elsewhere: "sleep 9 >/dev/null 2>&1 &".
 ***************************************************************************/
PUBLIC gbuffer_t *run_command(const char *command) // posix_spawn() of sh -c, synchronous
{
    if(empty_string(command)) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "No command to run",
            NULL
        );
        return NULL;
    }

    gbuffer_t *gbuf = gbuffer_create(4*1024, gbmem_get_maximum_block());
    if(!gbuf) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "Can't create gbuffer",
            "command",      "%s", command,
            NULL
        );
        return NULL;
    }

    int fds[2];
    if(pipe2(fds, O_CLOEXEC) < 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "pipe2() FAILED",
            "command",      "%s", command,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        gbuffer_decref(gbuf);
        return NULL;
    }

    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    int err = 0;
    pid_t pid = 0;
    const char *failed = spawn_setup(&actions, &attr, fds[1], &err);
    if(!failed) {
        char *const argv[] = {"sh", "-c", (char *)command, NULL};
        err = posix_spawn(&pid, "/bin/sh", &actions, &attr, argv, environ);
        if(err != 0) {
            failed = "posix_spawn()";
        }
        posix_spawnattr_destroy(&attr);
        posix_spawn_file_actions_destroy(&actions);
    }
    close(fds[1]);
    if(failed) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Command not run: a call to spawn it FAILED",
            "failed",       "%s", failed,
            "command",      "%s", command,
            "errno",        "%d", err,
            "strerror",     "%s", strerror(err),
            NULL
        );
        close(fds[0]);
        gbuffer_decref(gbuf);
        return NULL;
    }

    BOOL cut = FALSE;
    char bf[4*1024];
    while(1) {
        ssize_t n = read(fds[0], bf, sizeof(bf));
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "read() of the output FAILED: output cut",
                "command",      "%s", command,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            break;
        }
        if(n == 0) {
            break;
        }
        if(!cut && gbuffer_append(gbuf, bf, (size_t)n) != (size_t)n) {
            cut = TRUE;     // read on, so the command does not block on a full pipe
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "Output of the command too large: cut",
                "command",      "%s", command,
                "size",         "%lu", (unsigned long)gbuffer_leftbytes(gbuf),
                NULL
            );
        }
    }
    close(fds[0]);

    int status;
    while(waitpid(pid, &status, 0) < 0) {
        if(errno == ECHILD) {
            break;  // reaped already (SIGCHLD at SIG_IGN, or a handler that reaps): no status needed here
        }
        if(errno != EINTR) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "waitpid() FAILED",
                "command",      "%s", command,
                "pid",          "%d", (int)pid,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            break;
        }
    }

    return gbuf;
}

/***************************************************************************
 *  Run the program `path` with `argv` and wait for its end. No shell: argv
 *  reaches the program as it is. The program starts as spawn_setup() says:
 *  no signal blocked, every one at its default, no fd of the caller but
 *  stdin, stdout and stderr, not the caller's mask, ignores or fds
 *  without O_CLOEXEC.
 *
 *  Return the status of waitpid(2), 0 when the program exited with 0, or -1
 *  when it could not be run or waited for (logged).
 *
 *  Unlike system(3), SIGINT and SIGQUIT of the caller are not set to SIG_IGN
 *  while it waits: a yuno takes them from a signalfd, and ignoring them
 *  would throw away a stop that arrives in the meantime.
 ***************************************************************************/
PUBLIC int run_process2(const char *path, char *const argv[])
{
    if(empty_string(path) || !argv || !argv[0]) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "No program or no argv to run",
            "path",         "%s", path?path:"",
            NULL
        );
        return -1;
    }

    pid_t pid;
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    int err = 0;
    const char *failed = spawn_setup(&actions, &attr, -1, &err);
    if(!failed) {
        err = posix_spawn(&pid, path, &actions, &attr, argv, environ);
        if(err != 0) {
            failed = "posix_spawn()";
        }
        posix_spawnattr_destroy(&attr);
        posix_spawn_file_actions_destroy(&actions);
    }
    if(failed) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "Program not run: a call to spawn it FAILED",
            "failed",       "%s", failed,
            "path",         "%s", path,
            "errno",        "%d", err,
            "strerror",     "%s", strerror(err),
            NULL
        );
        return -1;
    }

    int status;
    while(waitpid(pid, &status, 0) < 0) {
        if(errno != EINTR) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "waitpid() FAILED",
                "path",         "%s", path,
                "pid",          "%d", (int)pid,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            return -1;
        }
    }

    return status;
}

/***************************************************************************
 *  The end of the caller's stdin, passed to the command of pty_sync_spawn()
 *  as a person types it: the end-of-file character of its terminal (VEOF,
 *  ctrl+d). In canonical mode it ends a read() with what the line holds,
 *  so a line not ended takes two: the first gives the line, the second the
 *  end. Nothing in raw mode (the command set it so): there VEOF is a byte
 *  like any other.
 ***************************************************************************/
PRIVATE void send_end_of_input(int master, BOOL line_open)
{
    struct termios tio;
    if(tcgetattr(master, &tio) < 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "tcgetattr() FAILED: the end of stdin not passed to the command",
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return;
    }
    if(!(tio.c_lflag & ICANON)) {
        return;
    }
    cc_t eof = tio.c_cc[VEOF];
    int times = line_open? 2 : 1;
    for(int i = 0; i < times; i++) {
        if(write(master, &eof, 1) != 1) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "write() FAILED: the end of stdin not passed to the command",
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            return;
        }
    }
}

/***************************************************************************
 *  Reap the command of pty_sync_spawn(): 1 when it ended (its status in
 *  `status`, or 0 when the kernel reaped it: SIGCHLD at SIG_IGN, or a
 *  handler that reaps, and waitpid() answers ECHILD), 0 when it runs yet
 *  (with WNOHANG), -1 when it could not be waited for (logged)
 ***************************************************************************/
PRIVATE int reap_command(const char *command, pid_t pid, int *status, int flags)
{
    while(1) {
        pid_t w = waitpid(pid, status, flags);
        if(w == pid) {
            return 1;
        }
        if(w == 0) {
            return 0;
        }
        if(errno == ECHILD) {
            *status = 0;    // reaped already: no status to read
            return 1;
        }
        if(errno != EINTR) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "waitpid() FAILED",
                "command",      "%s", command,
                "pid",          "%d", (int)pid,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            return -1;
        }
    }
}

/***************************************************************************
 *  Run a command with /bin/sh -c in a pseudo terminal, and pass the
 *  caller's terminal to it and its output to the caller's, until it ends.
 *  The command starts with no signal blocked, every signal at its default
 *  and no fd of the caller but the terminal (clean_child_before_exec()).
 *
 *  The end of the caller's stdin reaches the command as the end of its
 *  terminal's input (send_end_of_input()), so `cat >/dev/null` ends. When
 *  stdin is a terminal each end typed there (ctrl+d) is passed, and its
 *  input goes on reaching the command afterwards. Any other stdin ends
 *  once: the command gets ONE end of input, so only its first reader of
 *  the terminal sees it, and a second reader waits (a terminal has no end
 *  that stays, as a pipe has).
 *
 *  The end of the command is seen by a pidfd, so a process it left in the
 *  background holding the terminal does not hold this call (without a
 *  pidfd, the exit is looked for every 100 ms). Then what is left in the
 *  terminal is passed on without waiting for such a process, the terminal
 *  is closed and the command reaped.
 *
 *  0 when it exited (whatever its exit code; also when the kernel reaped
 *  it, SIGCHLD at SIG_IGN, and its status is lost: then a command killed
 *  by a signal answers 0 too), -1 when it could not be run, was killed by
 *  a signal, or could not be followed (said).
 ***************************************************************************/
PUBLIC int pty_sync_spawn(
    const char *command
)
{
    int master, pid;

    struct winsize size = {24, 80, 0, 0 };
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);

    pid = forkpty(&master, NULL, NULL, &size);
    if (pid < 0) {
        // Can't fork
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "forkpty() FAILED",
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
        return -1;

    } else if (pid == 0) {
        // Child
        clean_child_before_exec(-1);
        int ret = execlp("/bin/sh","/bin/sh", "-c", command, (char *)NULL);
        if (ret < 0) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "execlp() FAILED",
                "command",      "%s", command,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
        }
        _exit(-errno);
    }

    /*
     *  Parent
     */
    int reaped = 0;
    int status = 0;
#ifdef SYS_pidfd_open
    int pidfd = (int)syscall(SYS_pidfd_open, pid, 0);
    int pidfd_errno = errno;
#else
    int pidfd = -1;
    int pidfd_errno = ENOSYS;
#endif
    if(pidfd < 0 && pidfd_errno == ESRCH) {
        /*
         *  No process with that pid: the command ended and was reaped
         *  already (SIGCHLD at SIG_IGN, or a handler that reaps), so its
         *  status is lost.
         */
        reaped = 1;
    } else if(pidfd < 0) {
        gobj_log_warning(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "pidfd_open() FAILED: the end of the command is looked for every 100 ms",
            "command",      "%s", command,
            "errno",        "%d", pidfd_errno,
            "strerror",     "%s", strerror(pidfd_errno),
            NULL
        );
    }

    BOOL stdin_is_tty = isatty(STDIN_FILENO)? TRUE : FALSE;
    BOOL stdin_open = (master != STDIN_FILENO)? TRUE : FALSE;  // the master is fd 0 when the caller has none
    BOOL line_open = FALSE;     // bytes passed to the command since its last newline
    while(!reaped) {
        struct pollfd pfd[3] = {
            {.fd = master,                          .events = POLLIN},
            {.fd = stdin_open? STDIN_FILENO : -1,   .events = POLLIN},
            {.fd = pidfd,                           .events = POLLIN}
        };

        int n = poll(pfd, 3, pidfd >= 0? -1 : 100);
        if(n < 0) {
            if(errno == EINTR) {
                continue;
            }
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM,
                "msg",          "%s", "poll() FAILED",
                "command",      "%s", command,
                "errno",        "%d", errno,
                "strerror",     "%s", strerror(errno),
                NULL
            );
            break;
        }

        if(pfd[0].revents) {
            char output;
            ssize_t x = read(master, &output, 1);   // read from program
            if(x > 0) {
                x = write(STDOUT_FILENO, &output, 1);   // write to tty
                if(x) {} // avoid warning
            } else if(!(x < 0 && errno == EINTR)) {
                break;  // EIO: the terminal has no user left, the command ended
            }
        }

        if(stdin_open && pfd[1].revents) {
            char input;
            ssize_t x = read(STDIN_FILENO, &input, 1);  // read from tty
            if(x > 0) {
                x = write(master, &input, 1);   // write to program
                if(x > 0) {
                    line_open = (input != '\n')? TRUE : FALSE;
                }
            } else if(x == 0 && stdin_is_tty && !(pfd[1].revents & POLLHUP)) {
                send_end_of_input(master, line_open);   // a ctrl+d: the terminal goes on
                line_open = FALSE;
            } else if(x == 0 || (errno != EINTR && errno != EAGAIN)) {
                stdin_open = FALSE;
                send_end_of_input(master, line_open);
            }
        }

        reaped = reap_command(command, pid, &status, WNOHANG);
        if(reaped < 0) {
            break;  // Error already logged
        }
    }
    if(pidfd >= 0) {
        close(pidfd);
    }

    /*
     *  What the command left in the terminal, without waiting: a process
     *  it left in the background may hold the terminal open
     */
    int flags = fcntl(master, F_GETFL);
    if(flags < 0 || fcntl(master, F_SETFL, flags|O_NONBLOCK) < 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "fcntl() FAILED: what the command left in its terminal is not passed on",
            "command",      "%s", command,
            "errno",        "%d", errno,
            "strerror",     "%s", strerror(errno),
            NULL
        );
    } else {
        char drain_buf[4096];
        ssize_t n;
        while((n = read(master, drain_buf, sizeof(drain_buf))) > 0) {
            ssize_t w = write(STDOUT_FILENO, drain_buf, (size_t)n);
            if(w) {} // avoid warning
        }
    }
    close(master);

    if(reaped < 0) {
        return -1;
    }
    if(!reaped && reap_command(command, pid, &status, 0) < 0) {
        return -1;  // Error already logged
    }

    return WIFEXITED(status)? 0 : -1;
}
