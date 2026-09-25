/****************************************************************************
 *          test_dir_read_error.c
 *
 *          A directory that opens and then cannot be READ (readdir() fails
 *          with EIO, as on a disk error or a stale NFS handle) is a listing
 *          that fails, never a short one:
 *
 *              1. find_files_with_suffix_array() answers -1, empty, logged
 *              2. walk_dir_array() answers -1 when its root cannot be read
 *              3. and when a SUBdirectory cannot be read
 *              4. walk_dir_tree() answers -1
 *
 *          Up to 7.25.4 the failure was taken as the end of the directory:
 *          the listing answered 0 without the entries not read yet, and
 *          timeranger2 read a key without one of its md2 files, with no
 *          flag set.
 *
 *          And two more of the same functions:
 *
 *              5. walk_dir_array() and get_ordered_filename_array() with
 *                 `re` NULL list every entry, as documented. Up to 7.25.4
 *                 they crashed in regcomp().
 *              6. walk_dir_tree() of a root that exists and cannot be
 *                 opened (mode 0) answers -1 AND logs it: its callers say
 *                 "Error already logged". Up to 7.25.4 nothing was logged.
 *
 *          And the rules of the walks:
 *
 *              7. a SUBdirectory whose opendir() fails with EMFILE (a
 *                 transient cause) fails the walk: -1, empty, logged. Up
 *                 to 7.25.4 it was skipped, and the listing answered 0,
 *                 short.
 *              8. one whose opendir() fails with EACCES is skipped, with a
 *                 warning (up to 7.25.4 without one), and the walk answers
 *                 0 with the rest.
 *              9. a callback that returns FALSE in a subdirectory stops
 *                 the WHOLE walk. Up to 7.25.4 only that directory
 *                 stopped, and the walk went on with the next one.
 *             10. a tree whose paths do not fit in PATH_MAX: the walk
 *                 answers -1, "Path too long". Up to 7.25.4 build_path()
 *                 dropped the name, the walk went into the same directory
 *                 again, forever: a crash (stack overflow), or with few
 *                 files open a walk that answered 0 with the directory
 *                 given to the callback as its own entry.
 *             11. find_files_with_suffix_array() on a file system that
 *                 gives no d_type (DT_UNKNOWN): the files are listed, and
 *                 a name whose path does not fit fails the listing. Up to
 *                 7.25.4 the directory was stat'ed instead of the file,
 *                 and the file dropped with no log.
 *             12. a tree deeper than 1024 levels: -1, logged, no crash.
 *             13. an entry whose lstat() fails with EIO (not EACCES, not
 *                 ENOENT) fails the walk: -1, logged.
 *             14. find_files_with_suffix_array() without d_type, and the
 *                 lstat() of a file fails with EIO: -1, empty, logged.
 *             15. a SUBdirectory whose opendir() fails with ENOTDIR or
 *                 ELOOP (it is no longer a directory) is skipped, with a
 *                 warning, like EACCES.
 *             16. rmrcontentdir() and rmrdir() of a directory whose
 *                 readdir() fails: -1, logged "readdir() FAILED", what was
 *                 not read stays. Up to this fix rmrcontentdir() took the
 *                 failure as the end and answered 0 with nothing removed
 *                 and nothing logged, and rmrdir() blamed the rmdir().
 *             17. mkrdir() that fails leaves the cause in errno, after its
 *                 own log: a path under a FILE is ENOTDIR, a path longer than
 *                 PATH_MAX is ENAMETOOLONG. Up to this fix the log changed
 *                 errno, and a caller that logged strerror(errno) said
 *                 "Success".
 *
 *          The failure of readdir() is made by __wrap_readdir() below, for
 *          the directory named by `failing_dir` (seen at its opendir()).
 *          The failure of opendir() by __wrap_opendir(), for the directory
 *          named by `failing_open_dir`. __wrap_readdir() also hides the
 *          d_type of every entry when `hide_d_type` is set. The failure of
 *          lstat() by __wrap_lstat(), for the path `failing_lstat`.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <yunetas.h>

#define APP "test_dir_read_error"

#define BASE    "/tmp/test_dir_read_error"
#define SUB     BASE "/sub"

/***************************************************************
 *              The readdir() of a failing disk
 ***************************************************************/
DIR *__real_opendir(const char *name);
DIR *__wrap_opendir(const char *name);
struct dirent *__real_readdir(DIR *dirp);
struct dirent *__wrap_readdir(DIR *dirp);

PRIVATE const char *failing_dir = NULL;    // the directory whose readdir() fails
PRIVATE int entries_before_failure = 0;    // entries it gives before failing
PRIVATE DIR *failing_dirp = NULL;
PRIVATE int entries_given = 0;
PRIVATE int failures = 0;
PRIVATE const char *failing_open_dir = NULL;   // the directory whose opendir() fails
PRIVATE int failing_open_errno = 0;
PRIVATE BOOL hide_d_type = FALSE;

DIR *__wrap_opendir(const char *name)
{
    if(failing_open_dir && strcmp(name, failing_open_dir) == 0) {
        errno = failing_open_errno;
        return NULL;
    }
    DIR *dirp = __real_opendir(name);
    if(dirp && failing_dir && strcmp(name, failing_dir) == 0) {
        failing_dirp = dirp;
        entries_given = 0;
    }
    return dirp;
}

struct dirent *__wrap_readdir(DIR *dirp)
{
    if(dirp && dirp == failing_dirp) {
        if(entries_given >= entries_before_failure) {
            failing_dirp = NULL;
            failures++;
            errno = EIO;
            return NULL;
        }
        entries_given++;
    }
    struct dirent *dent = __real_readdir(dirp);
    if(dent && hide_d_type) {
        dent->d_type = DT_UNKNOWN;
    }
    return dent;
}

int __real_lstat(const char *path, struct stat *st);
int __wrap_lstat(const char *path, struct stat *st);

PRIVATE const char *failing_lstat = NULL;  // the path whose lstat() fails (EIO)
PRIVATE int lstat_failures = 0;

int __wrap_lstat(const char *path, struct stat *st)
{
    if(failing_lstat && strcmp(path, failing_lstat) == 0) {
        lstat_failures++;
        errno = EIO;
        return -1;
    }
    return __real_lstat(path, st);
}

PRIVATE void fail_readdir_of(const char *directory, int entries)
{
    failing_dir = directory;
    entries_before_failure = entries;
    failing_dirp = NULL;
    failures = 0;
}

/***************************************************************
 *              Checks
 ***************************************************************/
PRIVATE int global_result = 0;

PRIVATE void ok_or_fail(int cond, const char *name)
{
    if(cond) {
        printf("ok   %s\n", name);
    } else {
        printf("FAIL %s\n", name);
        global_result += -1;
    }
}

/*
 *  The warnings and errors logged
 */
PRIVATE int s_logs = 0;
PRIVATE char s_last_log[4096];

PRIVATE int capture_logs(void *h, int priority, const char *bf, size_t len)
{
    snprintf(s_last_log, sizeof(s_last_log), "%.*s", (int)len, bf);
    s_logs++;
    return 0;
}

PRIVATE int entries = 0;
PRIVATE BOOL count_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,
    char *fullpath,
    const char *directory,
    char *name,
    int level,
    wd_option opt
)
{
    entries++;
    return TRUE;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void make_tree(void)
{
    rmrdir(BASE);
    mkrdir(SUB, 02770);
    const char *files[] = {BASE "/a.md2", BASE "/b.md2", BASE "/c.md2", SUB "/d.log"};
    for(size_t i = 0; i < ARRAY_SIZE(files); i++) {
        int fd = newfile(files[i], 0660, FALSE);
        if(fd >= 0) {
            close(fd);
        }
    }
}

/***************************************************************************
 *  1-4. A directory that cannot be read
 ***************************************************************************/
PRIVATE void test_read_error(void)
{
    dir_array_t da;
    int ret;

    /*
     *  1.
     */
    fail_readdir_of(BASE, 4);   // at least one .md2 is listed before: the listing would be short
    gobj_log_set_last_message("%s", "");
    ret = find_files_with_suffix_array(0, BASE, ".md2", &da);
    ok_or_fail(failures == 1, "1. the readdir() of the directory failed");
    ok_or_fail(ret == -1, "1. find_files_with_suffix_array() answers -1 when readdir() fails");
    ok_or_fail(da.count == 0 && da.items == NULL, "1. and the listing is empty, not short");
    ok_or_fail(strstr(gobj_log_last_message(), "readdir() FAILED") != NULL, "1. and it is logged");
    dir_array_free(&da);

    /*
     *  2.
     */
    fail_readdir_of(BASE, 4);
    gobj_log_set_last_message("%s", "");
    ret = walk_dir_array(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(failures == 1, "2. the readdir() of the root failed");
    ok_or_fail(ret == -1, "2. walk_dir_array() answers -1 when the root cannot be read");
    ok_or_fail(da.count == 0 && da.items == NULL, "2. and the listing is empty, not short");
    ok_or_fail(strstr(gobj_log_last_message(), "Cannot list directory tree") != NULL,
        "2. and it is logged");
    dir_array_free(&da);

    /*
     *  3.
     */
    fail_readdir_of(SUB, 0);
    ret = walk_dir_array(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(failures == 1, "3. the readdir() of the subdirectory failed");
    ok_or_fail(ret == -1, "3. walk_dir_array() answers -1 when a subdirectory cannot be read");
    ok_or_fail(da.count == 0 && da.items == NULL, "3. and the listing is empty, not short");
    dir_array_free(&da);

    /*
     *  4.
     */
    fail_readdir_of(BASE, 1);
    entries = 0;
    gobj_log_set_last_message("%s", "");
    ret = walk_dir_tree(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, count_cb, NULL);
    ok_or_fail(failures == 1, "4. the readdir() of the root failed");
    ok_or_fail(ret == -1, "4. walk_dir_tree() answers -1 when the root cannot be read");
    ok_or_fail(strstr(gobj_log_last_message(), "readdir() FAILED") != NULL, "4. and it is logged");

    fail_readdir_of(NULL, 0);

    /*
     *  Without failures, the same calls list the whole tree
     */
    ret = find_files_with_suffix_array(0, BASE, ".md2", &da);
    ok_or_fail(ret == 0 && da.count == 3, "a directory that can be read lists all its entries");
    dir_array_free(&da);
    ret = walk_dir_array(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(ret == 0 && da.count == 4, "a tree that can be read lists all its entries");
    dir_array_free(&da);
}

/***************************************************************************
 *  5. `re` NULL is every entry
 ***************************************************************************/
PRIVATE void test_re_null(void)
{
    dir_array_t da;
    int ret = walk_dir_array(0, BASE, NULL, WD_RECURSIVE|WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(ret == 0 && da.count == 4, "5. walk_dir_array() with re NULL lists every entry");
    dir_array_free(&da);

    ret = get_ordered_filename_array(0, BASE, NULL, WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(ret == 0 && da.count == 3, "5. get_ordered_filename_array() with re NULL too");
    dir_array_free(&da);
}

/***************************************************************************
 *  6. A root that cannot be opened is logged. Skipped as root, which
 *  opens it.
 ***************************************************************************/
PRIVATE void test_root_cannot_be_opened(void)
{
    if(geteuid() == 0) {
        printf("skip 6. as root every directory opens\n");
        return;
    }
    chmod(BASE, 0);
    gobj_log_set_last_message("%s", "");
    int ret = walk_dir_tree(0, BASE, ".*", WD_MATCH_REGULAR_FILE, count_cb, NULL);
    chmod(BASE, 02770);
    ok_or_fail(ret == -1, "6. walk_dir_tree() of a root that cannot be opened answers -1");
    ok_or_fail(strstr(gobj_log_last_message(), "Cannot open directory") != NULL,
        "6. and it is logged, as its callers say");
}

/***************************************************************************
 *  7-8. A SUBdirectory that cannot be opened
 ***************************************************************************/
PRIVATE void test_subdir_open_error(void)
{
    dir_array_t da;
    int ret;

    /*
     *  7. EMFILE: transient, the walk fails
     */
    failing_open_dir = SUB;
    failing_open_errno = EMFILE;
    gobj_log_set_last_message("%s", "");
    ret = walk_dir_array(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(ret == -1, "7. walk_dir_array() answers -1 when a subdirectory cannot be opened (EMFILE)");
    ok_or_fail(da.count == 0 && da.items == NULL, "7. and the listing is empty, not short");
    ok_or_fail(strstr(gobj_log_last_message(), "Cannot list directory tree") != NULL,
        "7. and it is logged");
    dir_array_free(&da);

    entries = 0;
    ret = walk_dir_tree(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, count_cb, NULL);
    ok_or_fail(ret == -1, "7. walk_dir_tree() answers -1 too");

    /*
     *  8. EACCES: skipped, with a warning
     */
    failing_open_errno = EACCES;
    int logs_before = s_logs;
    ret = walk_dir_array(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(ret == 0 && da.count == 3,
        "8. a subdirectory that cannot be opened (EACCES) is skipped, the rest listed");
    ok_or_fail(s_logs == logs_before + 1 && strstr(s_last_log, "it is skipped") != NULL,
        "8. with a warning");
    dir_array_free(&da);

    failing_open_dir = NULL;
    failing_open_errno = 0;
}

/***************************************************************************
 *  13-15. An entry that cannot be stat'ed, a subdirectory that is no
 *  longer one
 ***************************************************************************/
PRIVATE void test_stat_errors(void)
{
    dir_array_t da;
    int ret;
    int logs_before;

    /*
     *  13. lstat() EIO in a walk
     */
    failing_lstat = BASE "/b.md2";
    lstat_failures = 0;
    logs_before = s_logs;
    entries = 0;
    ret = walk_dir_tree(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, count_cb, NULL);
    ok_or_fail(lstat_failures == 1, "13. the lstat() of the entry failed");
    ok_or_fail(ret == -1, "13. walk_dir_tree() answers -1 when an entry cannot be stat'ed (EIO)");
    ok_or_fail(s_logs > logs_before && strstr(s_last_log, "stat() FAILED") != NULL,
        "13. and it is logged");
    ret = walk_dir_array(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, &da);
    ok_or_fail(ret == -1 && da.count == 0 && da.items == NULL,
        "13. walk_dir_array() answers -1, empty");
    dir_array_free(&da);

    /*
     *  14. lstat() EIO in find_files_with_suffix_array() without d_type
     */
    hide_d_type = TRUE;
    lstat_failures = 0;
    logs_before = s_logs;
    ret = find_files_with_suffix_array(0, BASE, ".md2", &da);
    hide_d_type = FALSE;
    ok_or_fail(lstat_failures == 1, "14. the lstat() of the file failed");
    ok_or_fail(ret == -1 && da.count == 0 && da.items == NULL,
        "14. find_files_with_suffix_array() without d_type answers -1, empty");
    ok_or_fail(s_logs > logs_before && strstr(s_last_log, "stat() FAILED") != NULL,
        "14. and it is logged");
    dir_array_free(&da);
    failing_lstat = NULL;

    /*
     *  15. ENOTDIR and ELOOP: skipped, with a warning
     */
    int errs[] = {ENOTDIR, ELOOP};
    for(size_t i = 0; i < ARRAY_SIZE(errs); i++) {
        failing_open_dir = SUB;
        failing_open_errno = errs[i];
        logs_before = s_logs;
        ret = walk_dir_array(0, BASE, ".*", WD_RECURSIVE|WD_MATCH_REGULAR_FILE, &da);
        char name[128];
        snprintf(name, sizeof(name),
            "15. a subdirectory that cannot be opened (%s) is skipped, the rest listed",
            strerror(errs[i]));
        ok_or_fail(ret == 0 && da.count == 3, name);
        ok_or_fail(s_logs == logs_before + 1 && strstr(s_last_log, "it is skipped") != NULL,
            "15. with a warning");
        dir_array_free(&da);
    }
    failing_open_dir = NULL;
    failing_open_errno = 0;
}

/***************************************************************************
 *  16. A removal whose readdir() fails
 ***************************************************************************/
#define RM_BASE     "/tmp/test_dir_read_error_rm"

PRIVATE void make_rm_tree(void)
{
    rmrdir(RM_BASE);
    mkrdir(RM_BASE "/sub", 02770);
    const char *files[] = {RM_BASE "/f1", RM_BASE "/sub/f2"};
    for(size_t i = 0; i < ARRAY_SIZE(files); i++) {
        int fd = newfile(files[i], 0660, FALSE);
        if(fd >= 0) {
            close(fd);
        }
    }
}

PRIVATE void test_remove_read_error(void)
{
    int ret;

    make_rm_tree();
    fail_readdir_of(RM_BASE, 0);
    gobj_log_set_last_message("%s", "");
    ret = rmrcontentdir(RM_BASE);
    ok_or_fail(failures == 1, "16. the readdir() of the directory failed");
    ok_or_fail(ret == -1, "16. rmrcontentdir() answers -1 when readdir() fails");
    ok_or_fail(strstr(gobj_log_last_message(), "readdir() FAILED") != NULL, "16. and it is logged");
    ok_or_fail(access(RM_BASE "/f1", F_OK) == 0, "16. and what was not read stays");

    fail_readdir_of(RM_BASE "/sub", 0);
    gobj_log_set_last_message("%s", "");
    ret = rmrdir(RM_BASE);
    ok_or_fail(failures == 1, "16. the readdir() of the subdirectory failed");
    ok_or_fail(ret == -1, "16. rmrdir() answers -1 when a readdir() fails");
    ok_or_fail(strstr(gobj_log_last_message(), "readdir() FAILED") != NULL,
        "16. and it logs the readdir(), not the rmdir() that follows");
    ok_or_fail(access(RM_BASE "/sub/f2", F_OK) == 0, "16. and what was not read stays");
    fail_readdir_of(NULL, 0);

    ret = rmrcontentdir(RM_BASE);
    ok_or_fail(ret == 0 && access(RM_BASE "/sub", F_OK) != 0,
        "16. without the failure rmrcontentdir() empties it");
    ret = rmrdir(RM_BASE);
    ok_or_fail(ret == 0 && access(RM_BASE, F_OK) != 0, "16. and rmrdir() removes it");
}

/***************************************************************************
 *  17. The errno of a failed mkrdir()
 ***************************************************************************/
PRIVATE void test_mkrdir_errno(void)
{
    errno = 0;
    int ret = mkrdir(BASE "/a.md2/sub", 02770);
    int err = errno;
    ok_or_fail(ret == -1 && err == ENOTDIR,
        "17. mkrdir() under a file answers -1 with errno ENOTDIR, after its log");

    char long_path[PATH_MAX + 16];
    memset(long_path, 'x', sizeof(long_path) - 1);
    long_path[0] = '/';
    long_path[sizeof(long_path) - 1] = 0;
    errno = 0;
    ret = mkrdir(long_path, 02770);
    err = errno;
    ok_or_fail(ret == -1 && err == ENAMETOOLONG,
        "17. mkrdir() of a path too long answers -1 with errno ENAMETOOLONG");
}

/***************************************************************************
 *  9. A callback that stops the walk in a subdirectory
 ***************************************************************************/
#define STOP_BASE   "/tmp/test_dir_read_error_stop"

PRIVATE int stop_calls = 0;
PRIVATE BOOL stop_in_subdir_cb(
    hgobj gobj,
    void *user_data,
    wd_found_type type,
    char *fullpath,
    const char *directory,
    char *name,
    int level,
    wd_option opt
)
{
    stop_calls++;
    return (level == 2)? FALSE: TRUE;
}

PRIVATE void test_callback_stops(void)
{
    rmrdir(STOP_BASE);
    const char *files[] = {STOP_BASE "/s1/f", STOP_BASE "/s2/f", STOP_BASE "/s3/f"};
    for(size_t i = 0; i < ARRAY_SIZE(files); i++) {
        char dir[PATH_MAX];
        snprintf(dir, sizeof(dir), "%s", files[i]);
        *strrchr(dir, '/') = 0;
        mkrdir(dir, 02770);
        int fd = newfile(files[i], 0660, FALSE);
        if(fd >= 0) {
            close(fd);
        }
    }

    stop_calls = 0;
    int ret = walk_dir_tree(0, STOP_BASE, NULL, WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        stop_in_subdir_cb, NULL);
    ok_or_fail(ret == 0 && stop_calls == 1,
        "9. a callback that returns FALSE in a subdirectory stops the whole walk");
    if(stop_calls != 1) {
        printf("     (the callback was called %d times)\n", stop_calls);
    }
    rmrdir(STOP_BASE);
}

/***************************************************************************
 *  10-12. Paths that do not fit, a tree too deep
 ***************************************************************************/
#define DEEP_BASE   "/tmp/test_dir_read_error_deep"

/*
 *  Directories of `name_len` chars, with fds, while the path fits under
 *  `max_len`; then `leaves` entries of 250 chars in the deepest one
 *  (directories, or files). The path of the deepest one is left in `deepest`.
 */
PRIVATE void make_long_tree(size_t max_len, int levels, size_t name_len, BOOL leaf_files, char *deepest)
{
    mkdir(DEEP_BASE, 0775);
    int fd = open(DEEP_BASE, O_RDONLY|O_DIRECTORY);
    char name[NAME_MAX+1];
    memset(name, 'd', name_len);
    name[name_len] = 0;
    size_t len = strlen(DEEP_BASE);
    snprintf(deepest, PATH_MAX, "%s", DEEP_BASE);
    for(int i=0; fd >= 0 && (levels == 0 || i < levels) && len + 1 + name_len < max_len; i++) {
        mkdirat(fd, name, 0775);
        int fd2 = openat(fd, name, O_RDONLY|O_DIRECTORY);
        close(fd);
        fd = fd2;
        if(len + 1 + name_len < PATH_MAX) {
            snprintf(deepest + len, PATH_MAX - len, "/%s", name);
        }
        len += 1 + name_len;
    }
    if(fd >= 0 && levels == 0) {
        char leaf[256];
        memset(leaf, 'a', 250);
        leaf[250] = 0;
        for(int i=0; i<2; i++) {
            leaf[0] = (char)('a' + i);
            if(leaf_files) {
                int f = openat(fd, leaf, O_CREAT|O_WRONLY, 0664);
                if(f >= 0) {
                    close(f);
                }
            } else {
                mkdirat(fd, leaf, 0775);
            }
        }
    }
    if(fd >= 0) {
        close(fd);
    }
}

PRIVATE void remove_long_tree(void)
{
    if(system("rm -rf " DEEP_BASE) != 0) {
        printf("FAIL cannot remove %s\n", DEEP_BASE);
        global_result += -1;
    }
}

/*
 *  10. The deepest directory is ~3900 chars: two subdirectories of 250
 *  chars in it do not fit. Run LAST: up to 7.25.4 it crashed.
 */
PRIVATE void test_walk_path_too_long(void)
{
    char deepest[PATH_MAX];
    int ret;
    dir_array_t da;

    remove_long_tree();

    make_long_tree(PATH_MAX - 150, 0, 200, FALSE, deepest);
    entries = 0;
    int logs_before = s_logs;
    ret = walk_dir_tree(0, DEEP_BASE, NULL, WD_RECURSIVE|WD_MATCH_DIRECTORY, count_cb, NULL);
    ok_or_fail(ret == -1, "10. walk_dir_tree() of paths longer than PATH_MAX answers -1");
    ok_or_fail(s_logs > logs_before && strstr(s_last_log, "Path too long") != NULL,
        "10. and logs \"Path too long\"");
    ok_or_fail(entries < 30, "10. and it does not walk the same directory again");

    ret = walk_dir_array(0, DEEP_BASE, NULL, WD_RECURSIVE|WD_MATCH_DIRECTORY, &da);
    ok_or_fail(ret == -1 && da.count == 0, "10. walk_dir_array() answers -1, empty");
    dir_array_free(&da);
    remove_long_tree();
}

PRIVATE void test_long_paths(void)
{
    char deepest[PATH_MAX];
    int ret;
    dir_array_t da;
    int logs_before;

    remove_long_tree();

    /*
     *  11. No d_type: the files are stat'ed
     */
    hide_d_type = TRUE;
    ret = find_files_with_suffix_array(0, BASE, ".md2", &da);
    ok_or_fail(ret == 0 && da.count == 3,
        "11. find_files_with_suffix_array() without d_type lists the files");
    dir_array_free(&da);

    make_long_tree(PATH_MAX - 150, 0, 200, TRUE, deepest);
    logs_before = s_logs;
    ret = find_files_with_suffix_array(0, deepest, "", &da);
    ok_or_fail(ret == -1 && da.count == 0,
        "11. and a file whose path does not fit fails the listing");
    ok_or_fail(s_logs > logs_before && strstr(s_last_log, "Path too long") != NULL,
        "11. logged");
    dir_array_free(&da);
    hide_d_type = FALSE;
    remove_long_tree();

    /*
     *  12. 1100 levels of 1 char: it fits, and it is deeper than a walk goes
     */
    make_long_tree(PATH_MAX, 1100, 1, FALSE, deepest);
    logs_before = s_logs;
    ret = walk_dir_tree(0, DEEP_BASE, NULL, WD_RECURSIVE|WD_MATCH_DIRECTORY, count_cb, NULL);
    ok_or_fail(ret == -1 && s_logs > logs_before,
        "12. a tree of 1100 levels: walk_dir_tree() answers -1, logged");
    printf("     (%s)\n", strstr(s_last_log, "Tree too deep")? "tree too deep":
        "stopped before: no more open files");
    remove_long_tree();
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IOLBF, 0);   // what was checked is printed, also before a crash

    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
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
    gobj_log_register_handler("capture_logs", 0, capture_logs, 0);
    gobj_log_add_handler("capture_logs", "capture_logs", LOG_OPT_UP_WARNING, 0);

    make_tree();
    test_read_error();
    test_root_cannot_be_opened();
    test_subdir_open_error();
    test_stat_errors();
    test_remove_read_error();
    test_mkrdir_errno();
    test_callback_stops();
    test_long_paths();
    test_re_null();     // it crashed up to 7.25.4
    test_walk_path_too_long();  // LAST: it crashed up to 7.25.4
    rmrdir(BASE);

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
