/****************************************************************************
 *          test_spawn_copy.c
 *
 *          run_process2() (root-linux, run_command.c) and copyfile()
 *          (gobj-c, helpers.c), both written again after 7.28.4 to take out
 *          code of others: the system() of TLPI and an answer of
 *          StackOverflow (see CHANGELOG.md).
 *
 *          run_process2(): the status of waitpid(2) for a program that
 *          exits 0 or not, the argv as given (no shell), -1 for a program
 *          that does not exist.
 *          copyfile(): the same bytes, the mode asked, an existing
 *          destination kept unless `overwrite`, -1 for a missing source.
 *
 *          A program run by run_process2(), run_command() or launch_daemon()
 *          starts with no signal blocked and none ignored, whatever the
 *          caller has: a yuno keeps its signals blocked for its signalfd.
 *          run_command() takes stdout and stderr, and a command longer than
 *          its old 4 KB buffer.
 *          launch_daemon(): the pid of the daemon (kill() reaches it), no
 *          zombie left, -1 for a program that cannot be run.
 *
 *          None of them gives its program an fd of the caller above stderr
 *          (one without O_CLOEXEC); run_command() gives it /dev/null as
 *          stdin, not the caller's. launch_daemon() with SIGCHLD at SIG_IGN
 *          (its first child reaped by the kernel) answers the pid and says
 *          no error. launch_daemon() from a caller with stdin and stdout
 *          closed says an exec failure. pty_sync_spawn() leaves no fd and no
 *          zombie, and passes the end of its stdin to the command (each
 *          ctrl+d of a terminal); it answers 0 with SIGCHLD at SIG_IGN, also
 *          with no WARNING for a command the kernel reaped before its pidfd
 *          was opened, and ends with its command, not with a process left in
 *          the background.
 *
 *          syscall() is wrapped (--wrap=syscall, CMakeLists.txt): with
 *          `wait_for_end_before_pidfd` set, a pidfd_open() waits until its
 *          process is gone, so the command is always reaped before its
 *          pidfd is opened, and not only when the scheduler says so.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <pty.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <yunetas.h>

#define APP "test_spawn_copy"

PRIVATE int global_result = 0;
PRIVATE int wait_for_end_before_pidfd = 0;

/***************************************************************************
 *  The wrap of syscall(): see the header
 ***************************************************************************/
long __real_syscall(long number, ...);
long __wrap_syscall(long number, ...);
long __wrap_syscall(long number, ...)
{
    va_list ap;
    va_start(ap, number);
    long a1 = va_arg(ap, long);
    long a2 = va_arg(ap, long);
    long a3 = va_arg(ap, long);
    long a4 = va_arg(ap, long);
    long a5 = va_arg(ap, long);
    long a6 = va_arg(ap, long);
    va_end(ap);

#ifdef SYS_pidfd_open
    if(number == SYS_pidfd_open && wait_for_end_before_pidfd) {
        for(int i = 0; i < 500 && kill((pid_t)a1, 0) == 0; i++) {
            usleep(10*1000);
        }
    }
#endif
    return __real_syscall(number, a1, a2, a3, a4, a5, a6);
}

PRIVATE void ok_or_fail(int cond, const char *name)
{
    if(cond) {
        printf("ok   %s\n", name);
    } else {
        printf("FAIL %s\n", name);
        global_result += -1;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_run_process2(void)
{
    char *const argv_true[] = {"true", NULL};
    ok_or_fail(run_process2("/bin/true", argv_true) == 0, "a program that exits 0 answers 0");

    char *const argv_false[] = {"false", NULL};
    int status = run_process2("/bin/false", argv_false);
    ok_or_fail(
        status > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 1,
        "a program that exits 1 answers its waitpid() status"
    );

    char *const argv_sh[] = {"sh", "-c", "exit $(( $# + 4 ))", "sh", "a b", "c", NULL};
    status = run_process2("/bin/sh", argv_sh);
    ok_or_fail(
        status > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 6,
        "argv reaches the program as given, with no shell splitting it"
    );

    printf("     (an ERROR \"Program not run: a call to spawn it FAILED\" follows, on purpose)\n");
    char *const argv_none[] = {"none", NULL};
    ok_or_fail(
        run_process2("/nonexistent/program", argv_none) == -1,
        "a program that does not exist answers -1"
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_copyfile(void)
{
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "/tmp/%s.%d", APP, (int)getpid());
    mkrdir(dir, 0700);

    char src[PATH_MAX];
    char dst[PATH_MAX];
    build_path(src, sizeof(src), dir, "source.bin", NULL);
    build_path(dst, sizeof(dst), dir, "copy.bin", NULL);

    /*
     *  More than one block of any buffer, and every byte value
     */
    size_t size = 300*1024 + 7;
    uint8_t *data = gbmem_malloc(size);
    if(!data) {
        ok_or_fail(0, "gbmem_malloc() of the source");
        return;
    }
    for(size_t i = 0; i < size; i++) {
        data[i] = (uint8_t)(i * 31 + (i >> 8));
    }
    FILE *f = fopen(src, "wb");
    if(!f || fwrite(data, 1, size, f) != size) {
        ok_or_fail(0, "write the source");
        if(f) {
            fclose(f);
        }
        gbmem_free(data);
        return;
    }
    fclose(f);

    ok_or_fail(copyfile(src, dst, 0640, FALSE) == 0, "copyfile() answers 0");

    struct stat st;
    ok_or_fail(stat(dst, &st) == 0 && (size_t)st.st_size == size, "the copy has the size of the source");
    ok_or_fail(stat(dst, &st) == 0 && (st.st_mode & 0777) == 0640, "the copy has the mode asked");

    uint8_t *copy = gbmem_malloc(size);
    f = fopen(dst, "rb");
    BOOL same = FALSE;
    if(copy && f && fread(copy, 1, size, f) == size) {
        same = memcmp(copy, data, size) == 0;
    }
    if(f) {
        fclose(f);
    }
    ok_or_fail(same, "the copy has the bytes of the source");

    printf("     (two ERRORs follow, on purpose)\n");
    ok_or_fail(copyfile(src, dst, 0640, FALSE) == -1, "an existing destination is kept without overwrite");
    ok_or_fail(copyfile(src, dst, 0600, TRUE) == 0, "an existing destination is replaced with overwrite");

    char missing[PATH_MAX];
    build_path(missing, sizeof(missing), dir, "missing.bin", NULL);
    ok_or_fail(copyfile(missing, dst, 0640, TRUE) == -1, "a missing source answers -1");

    gbmem_free(copy);
    gbmem_free(data);
    rmrdir(dir);
}

/***************************************************************************
 *  The caller as a yuno is: its signals blocked (C_YUNO's set), and one of
 *  them ignored, as a starter can leave it
 ***************************************************************************/
PRIVATE void test_clean_signals(void)
{
    sigset_t yuno_set;
    sigset_t old_mask;
    sigemptyset(&yuno_set);
    int sigs[] = {SIGPIPE, SIGTERM, SIGALRM, SIGQUIT, SIGINT, SIGUSR1, SIGUSR2};
    for(size_t i = 0; i < sizeof(sigs)/sizeof(sigs[0]); i++) {
        sigaddset(&yuno_set, sigs[i]);
    }
    sigprocmask(SIG_BLOCK, &yuno_set, &old_mask);
    struct sigaction ign;
    struct sigaction old_usr2;
    memset(&ign, 0, sizeof(ign));
    ign.sa_handler = SIG_IGN;
    sigaction(SIGUSR2, &ign, &old_usr2);

    char *const argv_status[] = {"sh", "-c",
        "grep -c -E '^Sig(Blk|Ign):[[:space:]]*0+$' /proc/self/status | grep -qx 2", NULL};
    ok_or_fail(
        run_process2("/bin/sh", argv_status) == 0,
        "run_process2(): the program has no signal blocked and none ignored"
    );

    gbuffer_t *gbuf = run_command("grep -E '^Sig(Blk|Ign)' /proc/self/status; echo to-stderr >&2");
    char *out = gbuf? gbuffer_cur_rd_pointer(gbuf): NULL;
    size_t len = gbuf? gbuffer_leftbytes(gbuf): 0;
    char text[512] = "";
    if(out) {
        snprintf(text, sizeof(text), "%.*s", (int)len, out);
    }
    ok_or_fail(
        strstr(text, "SigBlk:\t0000000000000000") && strstr(text, "SigIgn:\t0000000000000000"),
        "run_command(): the command has no signal blocked and none ignored"
    );
    ok_or_fail(strstr(text, "to-stderr") != NULL, "run_command(): stderr is in the output");
    GBUFFER_DECREF(gbuf)

    char long_cmd[6*1024];
    size_t n = (size_t)snprintf(long_cmd, sizeof(long_cmd), "printf %%s ");
    while(n < 5000) {
        long_cmd[n++] = 'x';
    }
    long_cmd[n] = 0;
    gbuf = run_command(long_cmd);
    ok_or_fail(
        gbuf && gbuffer_leftbytes(gbuf) == n - strlen("printf %s "),
        "run_command(): a command longer than 4 KB runs whole"
    );
    GBUFFER_DECREF(gbuf)

    char file[PATH_MAX];
    snprintf(file, sizeof(file), "/tmp/%s.%d.daemon", APP, (int)getpid());
    char cmd[3*PATH_MAX + 128];
    snprintf(cmd, sizeof(cmd), "grep -E '^Sig(Blk|Ign)' /proc/self/status > %s.tmp && mv %s.tmp %s",
        file, file, file);
    launch_daemon(TRUE, "/bin/sh", "-c", cmd, NULL);
    char daemon_text[256] = "";
    for(int i = 0; i < 200 && !daemon_text[0]; i++) {
        FILE *f = fopen(file, "r");
        if(f) {
            size_t r = fread(daemon_text, 1, sizeof(daemon_text) - 1, f);
            daemon_text[r] = 0;
            fclose(f);
        } else {
            usleep(10*1000);
        }
    }
    unlink(file);
    ok_or_fail(
        strstr(daemon_text, "SigBlk:\t0000000000000000") &&
        strstr(daemon_text, "SigIgn:\t0000000000000001"),
        "launch_daemon(): no signal blocked, only SIGHUP ignored (a daemon)"
    );

    sigaction(SIGUSR2, &old_usr2, NULL);
    sigprocmask(SIG_SETMASK, &old_mask, NULL);
}

/***************************************************************************
 *  How many children of this process are zombies
 ***************************************************************************/
PRIVATE int my_zombies(void)
{
    int zombies = 0;
    DIR *d = opendir("/proc");
    struct dirent *e;
    while(d && (e = readdir(d))) {
        char path[PATH_MAX];
        char bf[512];
        snprintf(path, sizeof(path), "/proc/%s/stat", e->d_name);
        FILE *f = fopen(path, "r");
        if(!f) {
            continue;
        }
        size_t r = fread(bf, 1, sizeof(bf) - 1, f);
        fclose(f);
        bf[r] = 0;
        char *p = strrchr(bf, ')');
        char state;
        int ppid;
        if(p && sscanf(p + 1, " %c %d", &state, &ppid) == 2 && ppid == (int)getpid() && state == 'Z') {
            zombies++;
        }
    }
    if(d) {
        closedir(d);
    }
    return zombies;
}

PRIVATE void test_launch_daemon(void)
{
    pid_t pid = launch_daemon(TRUE, "/bin/sleep", "30", NULL);
    char path[PATH_MAX];
    char cmdline[256] = "";
    snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
    FILE *f = (pid > 0)? fopen(path, "r"): NULL;
    if(f) {
        size_t r = fread(cmdline, 1, sizeof(cmdline) - 1, f);
        cmdline[r] = 0;
        fclose(f);
    }
    ok_or_fail(pid > 0 && strcmp(cmdline, "/bin/sleep") == 0,
        "launch_daemon(): answers the pid of the daemon, after its exec");

    BOOL gone = FALSE;
    if(pid > 0 && kill(pid, SIGKILL) == 0) {
        for(int i = 0; i < 200 && !gone; i++) {
            char st[512] = "";
            snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
            f = fopen(path, "r");
            if(!f) {
                gone = TRUE;
                break;
            }
            size_t r = fread(st, 1, sizeof(st) - 1, f);
            fclose(f);
            st[r] = 0;
            char *p = strrchr(st, ')');
            if(p && p[1] == ' ' && p[2] == 'Z') {
                gone = TRUE;
                break;
            }
            usleep(10*1000);
        }
    }
    ok_or_fail(gone, "launch_daemon(): a kill() of the pid answered ends the daemon");

    printf("     (an ERROR \"not launched: execvp()\" follows, on purpose)\n");
    ok_or_fail(launch_daemon(TRUE, "/nonexistent/program", NULL) == -1,
        "launch_daemon(): a program that cannot be run answers -1");

    ok_or_fail(my_zombies() == 0, "launch_daemon(): no zombie child left");
}

/***************************************************************************
 *  A file read whole, or "" (at most size - 1 bytes), waiting up to 2 s
 *  for it to appear
 ***************************************************************************/
PRIVATE void wait_and_read(const char *file, char *text, size_t size)
{
    text[0] = 0;
    for(int i = 0; i < 200 && !text[0]; i++) {
        FILE *f = fopen(file, "r");
        if(f) {
            size_t r = fread(text, 1, size - 1, f);
            text[r] = 0;
            fclose(f);
        } else {
            usleep(10*1000);
        }
    }
}

/***************************************************************************
 *  An fd of the caller without O_CLOEXEC, above stderr: no program run from
 *  here has it. Up to 7.29.3 every one did.
 ***************************************************************************/
#define LEAK_FD 77

PRIVATE void test_no_inherited_fds(void)
{
    int fd = open("/dev/null", O_RDONLY);   // no O_CLOEXEC, on purpose
    if(fd < 0 || dup2(fd, LEAK_FD) != LEAK_FD) {
        ok_or_fail(0, "an fd without O_CLOEXEC to leak");
        if(fd >= 0) {
            close(fd);
        }
        return;
    }
    close(fd);

    char check[128];
    snprintf(check, sizeof(check), "[ ! -e /proc/$$/fd/%d ]", LEAK_FD);
    char *const argv_check[] = {"sh", "-c", check, NULL};
    ok_or_fail(
        run_process2("/bin/sh", argv_check) == 0,
        "run_process2(): the program has no fd of the caller above stderr"
    );

    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "if [ -e /proc/$$/fd/%d ]; then echo inherited; else echo closed; fi", LEAK_FD);
    gbuffer_t *gbuf = run_command(cmd);
    char text[256] = "";
    if(gbuf) {
        snprintf(text, sizeof(text), "%.*s",
            (int)gbuffer_leftbytes(gbuf), (char *)gbuffer_cur_rd_pointer(gbuf));
    }
    GBUFFER_DECREF(gbuf)
    ok_or_fail(strcmp(text, "closed\n") == 0,
        "run_command(): the command has no fd of the caller above stderr");

    char file[PATH_MAX];
    snprintf(file, sizeof(file), "/tmp/%s.%d.fds", APP, (int)getpid());
    char dcmd[3*PATH_MAX + 256];
    snprintf(dcmd, sizeof(dcmd),
        "if [ -e /proc/$$/fd/%d ]; then echo inherited; else echo closed; fi > %s.tmp && mv %s.tmp %s",
        LEAK_FD, file, file, file);
    launch_daemon(TRUE, "/bin/sh", "-c", dcmd, NULL);
    wait_and_read(file, text, sizeof(text));
    unlink(file);
    ok_or_fail(strcmp(text, "closed\n") == 0,
        "launch_daemon(): the daemon has no fd of the caller above stderr");

    close(LEAK_FD);
}

/***************************************************************************
 *  run_command(): the command reads /dev/null, not the caller's stdin (a
 *  pipe here, as a yuno's could be anything)
 ***************************************************************************/
PRIVATE void test_run_command_stdin(void)
{
    int p[2];
    int saved = dup(STDIN_FILENO);
    if(saved < 0 || pipe(p) < 0 || dup2(p[0], STDIN_FILENO) != STDIN_FILENO) {
        ok_or_fail(0, "a pipe as the caller's stdin");
        return;
    }
    close(p[0]);
    gbuffer_t *gbuf = run_command("readlink /proc/self/fd/0");
    char text[256] = "";
    if(gbuf) {
        snprintf(text, sizeof(text), "%.*s",
            (int)gbuffer_leftbytes(gbuf), (char *)gbuffer_cur_rd_pointer(gbuf));
    }
    GBUFFER_DECREF(gbuf)
    dup2(saved, STDIN_FILENO);
    close(saved);
    close(p[1]);
    ok_or_fail(strcmp(text, "/dev/null\n") == 0,
        "run_command(): the command's stdin is /dev/null, not the caller's");
}

/***************************************************************************
 *  launch_daemon() with SIGCHLD at SIG_IGN: the kernel reaps the first
 *  child, waitpid() answers ECHILD, and that is no error. Up to this fix
 *  it said "waitpid() failed ... errno 10".
 ***************************************************************************/
PRIVATE void test_launch_daemon_sigchld_ignored(void)
{
    struct sigaction ign;
    struct sigaction old_chld;
    memset(&ign, 0, sizeof(ign));
    ign.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &ign, &old_chld);

    char outfile[PATH_MAX];
    snprintf(outfile, sizeof(outfile), "/tmp/%s.%d.stdout", APP, (int)getpid());
    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    int efd = open(outfile, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if(saved < 0 || efd < 0 || dup2(efd, STDOUT_FILENO) != STDOUT_FILENO) {
        ok_or_fail(0, "stdout to a file (print_error() writes there)");
        sigaction(SIGCHLD, &old_chld, NULL);
        return;
    }
    close(efd);

    pid_t pid = launch_daemon(TRUE, "/bin/sleep", "30", NULL);

    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);
    sigaction(SIGCHLD, &old_chld, NULL);

    char said[1024] = "";
    FILE *f = fopen(outfile, "r");
    if(f) {
        size_t r = fread(said, 1, sizeof(said) - 1, f);
        said[r] = 0;
        fclose(f);
    }
    unlink(outfile);
    if(said[0]) {
        printf("     said: %s", said);
    }
    ok_or_fail(pid > 0, "launch_daemon() with SIGCHLD ignored: answers the pid of the daemon");
    ok_or_fail(strstr(said, "waitpid") == NULL,
        "launch_daemon() with SIGCHLD ignored: says no waitpid() error");
    if(pid > 0) {
        kill(pid, SIGKILL);
    }
}

/***************************************************************************
 *  Run fn() in a child of its own, with its stdin and stdout as given (-1:
 *  the caller's), and wait for it up to `seconds`: its exit code, or -2
 *  when it did not end (it is killed then)
 ***************************************************************************/
PRIVATE int run_in_child(int (*fn)(void), int in_fd, int out_fd, int seconds)
{
    fflush(stdout);
    pid_t pid = fork();
    if(pid < 0) {
        return -1;
    }
    if(pid == 0) {
        if(in_fd >= 0) {
            dup2(in_fd, STDIN_FILENO);
        }
        if(out_fd >= 0) {
            dup2(out_fd, STDOUT_FILENO);
        }
        _exit(fn());
    }
    for(int i = 0; i < seconds * 100; i++) {
        int status;
        pid_t w = waitpid(pid, &status, WNOHANG);
        if(w == pid) {
            return WIFEXITED(status)? WEXITSTATUS(status) : -1;
        }
        usleep(10*1000);
    }
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return -2;
}

/***************************************************************************
 *  launch_daemon() from a caller with stdin and stdout closed: the ends of
 *  its pipe are not 0 and 1, so the daemon's /dev/null does not take the
 *  place of the one that says an exec failure. Up to this fix the failure
 *  went to /dev/null and the pid of a program never run was answered.
 ***************************************************************************/
PRIVATE int launch_with_stdio_closed(void)
{
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    pid_t pid = launch_daemon(TRUE, "/nonexistent/program", NULL);
    if(pid > 0) {
        kill(pid, SIGKILL);
        return 1;
    }
    pid = launch_daemon(TRUE, "/bin/sleep", "30", NULL);
    if(pid <= 0) {
        return 2;
    }
    char path[PATH_MAX];
    char target[PATH_MAX];
    int ret = 0;
    for(int fd = 0; fd <= 2; fd++) {
        snprintf(path, sizeof(path), "/proc/%d/fd/%d", (int)pid, fd);
        ssize_t n = readlink(path, target, sizeof(target) - 1);
        target[n > 0? n : 0] = 0;
        if(strcmp(target, "/dev/null") != 0) {
            ret = 3;
        }
    }
    kill(pid, SIGKILL);
    return ret;
}

PRIVATE void test_launch_daemon_stdio_closed(void)
{
    int devnull = open("/dev/null", O_WRONLY|O_CLOEXEC);
    int r = run_in_child(launch_with_stdio_closed, -1, devnull, 10);
    close(devnull);
    if(r != 0) {
        printf("     the child answered %d\n", r);
    }
    ok_or_fail(r == 0,
        "launch_daemon() with stdin and stdout closed: an exec failure answers -1, "
        "a daemon has /dev/null as 0, 1 and 2"
    );
}

/***************************************************************************
 *  pty_sync_spawn(): no fd and no zombie left per call, and the end of the
 *  caller's stdin reaches the command as the end of its terminal's input.
 *  Up to this fix the master of the terminal stayed open (one fd per call),
 *  the command was not reaped when the master was read to its end first,
 *  and a command that read its stdin to the end never got it.
 ***************************************************************************/
PRIVATE int count_my_fds(void)
{
    int n = 0;
    DIR *d = opendir("/proc/self/fd");
    struct dirent *e;
    while(d && (e = readdir(d))) {
        if(e->d_name[0] != '.') {
            n++;
        }
    }
    if(d) {
        closedir(d);
    }
    return n;
}

PRIVATE int pty_no_fd_no_zombie(void)
{
    int before = count_my_fds();
    for(int i = 0; i < 3; i++) {
        if(pty_sync_spawn("true") != 0) {
            return 1;
        }
        if(my_zombies() != 0) {
            return 2;
        }
    }
    return (count_my_fds() == before)? 0 : 3;
}

PRIVATE int pty_stdin_end(void)
{
    return pty_sync_spawn("cat >/dev/null; echo DONE") == 0? 0 : 1;
}

PRIVATE int pty_stdin_end_in_a_line(void)
{
    return pty_sync_spawn("cat; echo; echo DONE") == 0? 0 : 1;
}

/***************************************************************************
 *  pty_sync_spawn() with SIGCHLD at SIG_IGN: the kernel reaps the command,
 *  waitpid() answers ECHILD, and that is the end of a command, not an error.
 *  A command reaped before its pidfd is opened (pidfd_open() answers ESRCH,
 *  forced here by the wrap of syscall()) has ended too. Its status is lost,
 *  so a command killed by a signal answers 0 as well.
 ***************************************************************************/
PRIVATE int pty_sigchld_ignored(void)
{
    struct sigaction ign;
    memset(&ign, 0, sizeof(ign));
    ign.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &ign, NULL);
    if(pty_sync_spawn("true") != 0) {
        return 1;
    }
    wait_for_end_before_pidfd = 1;
    int r = pty_sync_spawn("true");
    wait_for_end_before_pidfd = 0;
    if(r != 0) {
        return 2;
    }
    return pty_sync_spawn("kill -9 $$") == 0? 0 : 3;
}

/***************************************************************************
 *  pty_sync_spawn() ends with its command, not with a process the command
 *  left in the background that holds the terminal (3 s here)
 ***************************************************************************/
PRIVATE int pty_background_left(void)
{
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int r = pty_sync_spawn("(trap '' HUP; exec sleep 3) & sleep 0.2");
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double secs = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec)/1e9;
    if(r != 0) {
        return 1;
    }
    return secs < 2.0? 0 : 2;
}

/***************************************************************************
 *  pty_sync_spawn() with a terminal as stdin: each end of input typed there
 *  (ctrl+d) reaches the command, so a second reader gets its own
 ***************************************************************************/
PRIVATE int pty_two_readers(void)
{
    return pty_sync_spawn("cat >/dev/null; echo A; cat >/dev/null; echo B") == 0? 0 : 1;
}

PRIVATE void test_pty_sync_spawn(void)
{
    char file[PATH_MAX];
    snprintf(file, sizeof(file), "/tmp/%s.%d.pty", APP, (int)getpid());
    int devnull = open("/dev/null", O_RDONLY|O_CLOEXEC);
    int out = open(file, O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, 0600);

    int r = run_in_child(pty_no_fd_no_zombie, devnull, out, 10);
    if(r != 0) {
        printf("     the child answered %d\n", r);
    }
    ok_or_fail(r == 0, "pty_sync_spawn(): answers 0, leaves no fd and no zombie");

    r = run_in_child(pty_stdin_end, devnull, out, 10);
    char text[512];
    wait_and_read(file, text, sizeof(text));
    ok_or_fail(r == 0 && strstr(text, "DONE") != NULL,
        "pty_sync_spawn(): a command that reads its stdin to the end ends (stdin at its end)");

    if(ftruncate(out, 0) == 0 && lseek(out, 0, SEEK_SET) == 0) {
        int p[2];
        if(pipe2(p, O_CLOEXEC) == 0) {
            ssize_t w = write(p[1], "abc", 3);     // a line not ended
            close(p[1]);
            r = run_in_child(pty_stdin_end_in_a_line, p[0], out, 10);
            close(p[0]);
            wait_and_read(file, text, sizeof(text));
            ok_or_fail(w == 3 && r == 0 && strstr(text, "abc") && strstr(text, "DONE"),
                "pty_sync_spawn(): the end of stdin after a line not ended reaches the command too");
        } else {
            ok_or_fail(0, "a pipe as the stdin of pty_sync_spawn()");
        }
    }
    if(ftruncate(out, 0) == 0 && lseek(out, 0, SEEK_SET) == 0) {
        r = run_in_child(pty_sigchld_ignored, devnull, out, 10);
        wait_and_read(file, text, sizeof(text));
        if(r != 0 || strstr(text, "FAILED")) {
            printf("     the child answered %d, said: %s\n", r, text);
        }
        ok_or_fail(r == 0 && strstr(text, "FAILED") == NULL,
            "pty_sync_spawn() with SIGCHLD ignored: answers 0 and says no error");
    }

    if(ftruncate(out, 0) == 0 && lseek(out, 0, SEEK_SET) == 0) {
        r = run_in_child(pty_background_left, devnull, out, 10);
        if(r != 0) {
            printf("     the child answered %d\n", r);
        }
        ok_or_fail(r == 0,
            "pty_sync_spawn(): ends with its command, not with a process it left in the background");
    }

    if(ftruncate(out, 0) == 0 && lseek(out, 0, SEEK_SET) == 0) {
        int tty_master = -1;
        int tty_slave = -1;
        if(openpty(&tty_master, &tty_slave, NULL, NULL, NULL) == 0) {
            const char typed[] = "abc\n\x04\x04";    // a line, then ctrl+d twice
            ssize_t w = write(tty_master, typed, sizeof(typed) - 1);
            r = run_in_child(pty_two_readers, tty_slave, out, 10);
            close(tty_slave);
            close(tty_master);
            wait_and_read(file, text, sizeof(text));
            if(r != 0) {
                printf("     the child answered %d\n", r);
            }
            ok_or_fail(w == (ssize_t)sizeof(typed) - 1 && r == 0 && strstr(text, "A") && strstr(text, "B"),
                "pty_sync_spawn(): with a terminal as stdin, each ctrl+d reaches the command");
        } else {
            ok_or_fail(0, "a terminal as the stdin of pty_sync_spawn()");
        }
    }

    close(out);
    close(devnull);
    unlink(file);
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    gbmem_setup(
        0,          // mem_max_block, the default
        0,          // mem_max_system_memory, the default
        FALSE,      // use_own_system_memory
        0,          // mem_min_block
        0           // mem_superblock
    );

    sys_malloc_fn_t malloc_func;
    sys_free_fn_t free_func;

    gbmem_get_json_allocators(&malloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    gobj_start_up(
        argc,
        argv,
        NULL,   // jn_global_settings
        NULL,   // persistent_attrs
        NULL,   // global_command_parser
        NULL,   // global_stats_parser
        NULL,   // global_authz_checker
        NULL    // global_authentication_parser
    );

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    test_run_process2();
    test_copyfile();
    test_clean_signals();
    test_launch_daemon();
    test_no_inherited_fds();
    test_run_command_stdin();
    test_launch_daemon_sigchld_ignored();
    test_launch_daemon_stdio_closed();
    test_pty_sync_spawn();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
