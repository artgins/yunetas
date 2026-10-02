/****************************************************************************
 *          test_dir_identity.c
 *
 *  dir_identity() tells a follower which directory a key directory is
 *  (inode and birth), on the path it takes without a descriptor per
 *  directory. Its callers take a FALSE as the directory gone and skip its
 *  scan, so a FALSE must be a directory gone, or be said:
 *
 *      1. a directory: TRUE, its inode;
 *      2. a path that does not exist, or under a file: FALSE, nothing logged;
 *      3. statx() refused (EPERM, a seccomp profile; ENOSYS): TRUE, the
 *         inode from lstat(), no birth (-1);
 *      4. statx() failing otherwise (EIO): FALSE and an ERROR, once for
 *         the same errno in a row, again after a success.
 *
 *  Up to 7.25.21 every failure of statx() answered FALSE with no log: 3
 *  and 4 were a directory taken for gone, in silence.
 *
 *  A unit test: it #includes timeranger2.c to reach the PRIVATE function,
 *  and fails statx() in its own __wrap_statx() (linked with --wrap=statx).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <syslog.h>
#include "timeranger2.c"

#define BASE    "/tmp/test_dir_identity"

/***************************************************************************
 *  statx() fails with `statx_errno` while it is not 0
 ***************************************************************************/
int __real_statx(int dirfd, const char *path, int flags, unsigned int mask, struct statx *stx);
int __wrap_statx(int dirfd, const char *path, int flags, unsigned int mask, struct statx *stx);

PRIVATE int statx_errno = 0;

int __wrap_statx(int dirfd, const char *path, int flags, unsigned int mask, struct statx *stx)
{
    if(statx_errno) {
        errno = statx_errno;
        return -1;
    }
    return __real_statx(dirfd, path, flags, mask, stx);
}

/***************************************************************************
 *  The ERRORs logged
 ***************************************************************************/
PRIVATE int errors_logged = 0;

PRIVATE int count_errors(void *v, int priority, const char *bf, size_t len)
{
    if(priority <= LOG_ERR) {
        errors_logged++;
    }
    return 0;
}

PRIVATE int check(const char *what, BOOL ok)
{
    if(!ok) {
        printf("%sFAILED%s %s\n", On_Red BWhite, Color_Off, what);
        return -1;
    }
    printf("ok   %s\n", what);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    int result = 0;

    glog_init();
    gobj_log_register_handler("errors", 0, count_errors, 0);
    gobj_log_add_handler("test_errors", "errors", LOG_OPT_UP_ERROR, 0);
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    rmrdir(BASE);
    mkdir(BASE, 0700);
    mkdir(BASE "/key", 0700);
    int fd = open(BASE "/file", O_CREAT|O_WRONLY|O_CLOEXEC, 0600);
    if(fd >= 0) {
        close(fd);
    }
    struct stat st;
    lstat(BASE "/key", &st);

    json_int_t ino = 0, bsec = 0, bnsec = 0;

    /*  1. A directory */
    BOOL ret = dir_identity(BASE "/key", &ino, &bsec, &bnsec);
    result += check("a directory: TRUE, its inode", ret && ino == (json_int_t)st.st_ino);

    /*  2. Gone, in silence */
    errors_logged = 0;
    ret = dir_identity(BASE "/nothing", &ino, &bsec, &bnsec);
    result += check("a path that does not exist: FALSE", !ret);
    ret = dir_identity(BASE "/file/key", &ino, &bsec, &bnsec);
    result += check("a path under a file: FALSE", !ret);
    result += check("a directory gone logs nothing", errors_logged == 0);

    /*  3. statx() refused: lstat()'s inode, no birth */
    statx_errno = EPERM;
    ino = 0;
    ret = dir_identity(BASE "/key", &ino, &bsec, &bnsec);
    result += check("statx() EPERM: TRUE, the inode of lstat(), no birth",
        ret && ino == (json_int_t)st.st_ino && bsec == -1 && bnsec == -1);
    statx_errno = ENOSYS;
    ino = 0;
    ret = dir_identity(BASE "/key", &ino, &bsec, &bnsec);
    result += check("statx() ENOSYS: TRUE, the inode of lstat()",
        ret && ino == (json_int_t)st.st_ino);
    statx_errno = EPERM;
    ret = dir_identity(BASE "/nothing", &ino, &bsec, &bnsec);
    result += check("statx() EPERM on a path gone: FALSE, nothing logged",
        !ret && errors_logged == 0);

    /*  4. statx() failing otherwise: said, once in a row */
    statx_errno = EIO;
    ret = dir_identity(BASE "/key", &ino, &bsec, &bnsec);
    result += check("statx() EIO: FALSE, one ERROR", !ret && errors_logged == 1);
    ret = dir_identity(BASE "/key", &ino, &bsec, &bnsec);
    result += check("statx() EIO again: not said again", !ret && errors_logged == 1);
    statx_errno = 0;
    dir_identity(BASE "/key", &ino, &bsec, &bnsec);
    statx_errno = EIO;
    ret = dir_identity(BASE "/key", &ino, &bsec, &bnsec);
    result += check("statx() EIO after a success: said again", !ret && errors_logged == 2);
    statx_errno = 0;

    rmrdir(BASE);
    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR%s --> system memory not free\n", On_Red BWhite, Color_Off);
        print_track_mem();
        result = -1;
    }

    printf("%s: %s\n", "test_dir_identity", result == 0? "PASS" : "FAIL");
    return result < 0? -1 : 0;
}
