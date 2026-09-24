/****************************************************************************
 *          test_rotatory.c
 *
 *          Retention of a rotatory directory: rotatory_remove_old_files()
 *          removes ONLY the old files of its own mask (and their .OLD),
 *          never the current file, a symbolic link, a directory, or a file
 *          of another name. And the rotation calls the newfile callback,
 *          which is where a user (the agent's audit) applies it.
 *          And the write path: whole records across a size rotation, and
 *          a removed file created again. And a clock set back across
 *          midnight empties no file. And a piece of 0 bytes, a failed
 *          write, the open of an old "W" file, a new day on a full disk.
 *          And the newfile callback runs for a NEW file only (never for
 *          the same file opened again), a failed rename of a keep_all
 *          handle is not tried at every record, and a fixed name (or a
 *          "MM" name within its month) is not emptied at the open.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/resource.h>
#include <signal.h>
#include <yunetas.h>

#define APP "test_rotatory"

#define BASE        "/tmp/test_rotatory_retention"
#define AUDIT_DIR   BASE "/audit"
#define OUTSIDE     BASE "/outside"
#define MASK        "ZZZ-DD_MM_CCYY.log"

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE int global_result = 0;
PRIVATE int s_errors = 0;
PRIVATE int s_newfile_calls = 0;
PRIVATE int s_removed_at_rotation = 0;

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int count_errors(void *h, int priority, const char *bf, size_t len)
{
    s_errors++;
    return 0;
}

PRIVATE void check(BOOL ok, const char *name)
{
    if(ok) {
        printf("ok   %s\n", name);
    } else {
        printf("FAIL %s\n", name);
        global_result += -1;
    }
}

PRIVATE size_t count_lines(const char *path);

PRIVATE BOOL exists_no_follow(const char *path)
{
    struct stat st;
    return lstat(path, &st) == 0;
}

/***************************************************************************
 *  Set the mtime of the path itself (a link is not followed)
 ***************************************************************************/
PRIVATE void set_age(const char *path, int days_ago)
{
    struct timespec times[2];
    times[0].tv_sec = time(NULL) - (time_t)days_ago*24*60*60;
    times[0].tv_nsec = 0;
    times[1] = times[0];
    if(utimensat(AT_FDCWD, path, times, AT_SYMLINK_NOFOLLOW) != 0) {
        printf("FAIL cannot set the age of %s\n", path);
        global_result += -1;
    }
}

/***************************************************************************
 *  Create a file of `size` bytes and set its mtime `days_ago` days back
 ***************************************************************************/
PRIVATE void make_file(const char *path, size_t size, int days_ago)
{
    FILE *f = fopen(path, "w");
    if(!f) {
        printf("FAIL cannot create %s\n", path);
        global_result += -1;
        return;
    }
    for(size_t i=0; i<size; i++) {
        fputc('x', f);
    }
    fclose(f);
    set_age(path, days_ago);
}

/***************************************************************************
 *  The newfile callback: where the agent applies its retention
 ***************************************************************************/
PRIVATE int newfile_cb(void *user_data, const char *old_filename, const char *new_filename)
{
    hrotatory_h hr = user_data;
    s_newfile_calls++;
    int removed = rotatory_remove_old_files(hr, 7, NULL, NULL);
    if(removed > 0) {
        s_removed_at_rotation += removed;
    }
    return 0;
}

/***************************************************************************
 *  A newfile callback that only counts, with its names
 ***************************************************************************/
PRIVATE int s_count_newfile = 0;
PRIVATE int s_count_newfile_same = 0;
PRIVATE char s_newfile_old[PATH_MAX] = "";
PRIVATE char s_newfile_new[PATH_MAX] = "";

PRIVATE int count_newfile_cb(void *user_data, const char *old_filename, const char *new_filename)
{
    s_count_newfile++;
    if(strcmp(old_filename, new_filename) == 0) {
        s_count_newfile_same++;
    }
    snprintf(s_newfile_old, sizeof(s_newfile_old), "%s", old_filename);
    snprintf(s_newfile_new, sizeof(s_newfile_new), "%s", new_filename);
    return 0;
}

/***************************************************************************
 *  Only the old files of the mask go
 ***************************************************************************/
PRIVATE void test_retention(void)
{
    rmrdir(BASE);
    mkrdir(AUDIT_DIR, 02775);
    mkrdir(OUTSIDE, 02775);

    hrotatory_h hr = rotatory_open(
        AUDIT_DIR "/" MASK,
        0,          // default buffer
        1,          // 1 mega: for the size rotation below
        1,          // min free disk percentage: do not stop on a full /tmp
        02775,
        0660,
        FALSE
    );
    if(!hr) {
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }

    const char *current = rotatory_path(hr);
    char current_old[PATH_MAX];
    snprintf(current_old, sizeof(current_old), "%s.OLD", current);

    /*
     *  Candidates (old files of the mask, and their .OLD)
     */
    make_file(AUDIT_DIR "/001-01_01_2026.log", 1000, 10);
    make_file(AUDIT_DIR "/001-01_01_2026.log.OLD", 2000, 10);

    /*
     *  Everything else
     */
    make_file(AUDIT_DIR "/002-02_01_2026.log", 10, 1);          // recent
    make_file(AUDIT_DIR "/notes.txt", 10, 30);                  // another name
    make_file(AUDIT_DIR "/001-01_01_2026.logx", 10, 30);        // another name
    make_file(AUDIT_DIR "/0a1-01_01_2026.log", 10, 30);         // a letter for a digit
    make_file(AUDIT_DIR "/001-01_01_2026.OLD", 10, 30);         // .OLD of no file shape
    make_file(OUTSIDE "/keep.log", 10, 30);
    if(symlink(OUTSIDE "/keep.log", AUDIT_DIR "/003-03_01_2026.log") != 0) {
        printf("FAIL symlink\n");
        global_result += -1;
    }
    set_age(AUDIT_DIR "/003-03_01_2026.log", 30);               // an old LINK with the shape
    mkrdir(AUDIT_DIR "/004-04_01_2026.log", 02775);             // a DIRECTORY with the shape
    set_age(AUDIT_DIR "/004-04_01_2026.log", 30);
    set_age(current, 30);                                       // the current file, old
    make_file(current_old, 10, 30);                             // its .OLD, old

    /*
     *  keep_days 0: nothing
     */
    int errors_before = s_errors;
    json_t *jn_removed = json_array();
    uint64_t removed_bytes = 99;
    int removed = rotatory_remove_old_files(hr, 0, jn_removed, &removed_bytes);
    check(removed == 0 && json_array_size(jn_removed) == 0 && removed_bytes == 0 &&
        exists_no_follow(AUDIT_DIR "/001-01_01_2026.log"),
        "keep_days 0 removes nothing"
    );

    /*
     *  keep_days 7
     */
    removed = rotatory_remove_old_files(hr, 7, jn_removed, &removed_bytes);
    check(removed == 2 && json_array_size(jn_removed) == 2 && removed_bytes == 3000,
        "keep_days 7 removes the two old files of the mask"
    );
    check(!exists_no_follow(AUDIT_DIR "/001-01_01_2026.log") &&
        !exists_no_follow(AUDIT_DIR "/001-01_01_2026.log.OLD"),
        "  the old file and its .OLD are gone"
    );
    check(exists_no_follow(AUDIT_DIR "/002-02_01_2026.log"), "  a recent file stays");
    check(exists_no_follow(AUDIT_DIR "/notes.txt") &&
        exists_no_follow(AUDIT_DIR "/001-01_01_2026.logx") &&
        exists_no_follow(AUDIT_DIR "/0a1-01_01_2026.log") &&
        exists_no_follow(AUDIT_DIR "/001-01_01_2026.OLD"),
        "  files of other names stay"
    );
    check(exists_no_follow(AUDIT_DIR "/003-03_01_2026.log") &&
        exists_no_follow(OUTSIDE "/keep.log"),
        "  an old symbolic link stays, and its target"
    );
    check(is_directory(AUDIT_DIR "/004-04_01_2026.log"), "  a directory with the shape stays");
    check(exists_no_follow(current) && exists_no_follow(current_old),
        "  the current file and its .OLD stay, even old"
    );
    check(s_errors == errors_before, "  no error logged");
    JSON_DECREF(jn_removed)

    /*
     *  A NULL handle is an error, logged
     */
    errors_before = s_errors;
    removed = rotatory_remove_old_files(NULL, 7, NULL, NULL);
    check(removed == -1 && s_errors - errors_before == 1, "NULL handle: -1, logged");

    /*
     *  At rotation: the size limit (1 mega) starts a new file, the newfile
     *  callback runs, and it removes the old file created before
     */
    make_file(AUDIT_DIR "/005-05_01_2026.log", 10, 10);
    rotatory_subscribe2newfile(hr, newfile_cb, hr);

    char line[1024];
    memset(line, 'a', sizeof(line)-1);
    line[sizeof(line)-1] = 0;
    for(int i=0; i<2200; i++) {    // > 2 megas: siz/1M > 1
        rotatory_write(hr, LOG_AUDIT, line, strlen(line));
    }
    check(s_newfile_calls >= 1 && s_removed_at_rotation == 1 &&
        !exists_no_follow(AUDIT_DIR "/005-05_01_2026.log"),
        "a rotation calls newfile, which applies the retention"
    );

    /*
     *  What a first sweep of a big directory costs (at start only)
     */
    for(int i=0; i<400; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%03d-01_01_2025.log", AUDIT_DIR, i % 366 + 1);
        if(i >= 366) {
            snprintf(path, sizeof(path), "%s/%03d-01_01_2025.log.OLD", AUDIT_DIR, i - 365);
        }
        make_file(path, 100, 60);
    }
    uint64_t t0 = time_in_milliseconds_monotonic();
    removed = rotatory_remove_old_files(hr, 7, NULL, NULL);
    uint64_t t1 = time_in_milliseconds_monotonic();
    check(removed == 400, "a first sweep of 400 old files removes all of them");
    printf("     (400 files swept in %" PRIu64 " ms)\n", t1 - t0);

    rotatory_close(hr);
    rmrdir(BASE);
}

/***************************************************************************
 *  The write path: a record is never split between two files, and a
 *  file removed from the directory is created again at the next record.
 *
 *  Up to 7.25.4 the file was checked before EACH PIECE of a record (the
 *  text, then its "\n"): when the text crossed the size limit, the size
 *  rotation came between the two, the .OLD ended without its newline and
 *  the new file began with it.
 ***************************************************************************/
PRIVATE char *read_whole_file(const char *path, size_t *plen)
{
    *plen = 0;
    FILE *f = fopen(path, "r");
    if(!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *bf = gbmem_malloc((size_t)size + 1);
    if(bf) {
        *plen = fread(bf, 1, (size_t)size, f);
        bf[*plen] = 0;
    }
    fclose(f);
    return bf;
}

PRIVATE void test_write_path(void)
{
    rmrdir(BASE);
    mkrdir(AUDIT_DIR, 02775);

    hrotatory_h hr = rotatory_open(
        AUDIT_DIR "/" MASK,
        0,          // default buffer
        1,          // 1 mega
        1,          // min free disk percentage: do not stop on a full /tmp
        02775,
        0660,
        FALSE
    );
    if(!hr) {
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    char current[PATH_MAX];
    snprintf(current, sizeof(current), "%s", rotatory_path(hr));
    char current_old[PATH_MAX+8];
    snprintf(current_old, sizeof(current_old), "%s.OLD", current);

    /*
     *  Records of 1000 bytes + "\n" until the size rotation
     */
    char line[1001];
    memset(line, 'r', sizeof(line)-1);
    line[sizeof(line)-1] = 0;
    for(int i=0; i<2200 && !exists_no_follow(current_old); i++) {
        rotatory_write(hr, LOG_AUDIT, line, strlen(line));
    }
    rotatory_write(hr, LOG_AUDIT, line, strlen(line));
    rotatory_flush(hr);

    size_t len_old = 0, len_cur = 0;
    char *old = read_whole_file(current_old, &len_old);
    char *cur = read_whole_file(current, &len_cur);
    check(old && len_old > 0 && (len_old % 1001) == 0 && old[len_old-1] == '\n',
        "size rotation: the .OLD holds whole records"
    );
    check(cur && len_cur > 0 && (len_cur % 1001) == 0 && cur[0] == 'r',
        "size rotation: the new file begins with a whole record"
    );
    GBMEM_FREE(old);
    GBMEM_FREE(cur);

    /*
     *  The current file removed: the next record creates it again.
     *  It is the same file (same name, no size rotation): no newfile
     *  callback.
     */
    rotatory_subscribe2newfile(hr, count_newfile_cb, NULL);
    s_count_newfile = 0;
    unlink(current);
    rotatory_write(hr, LOG_AUDIT, "after the rm", strlen("after the rm"));
    rotatory_flush(hr);
    cur = read_whole_file(current, &len_cur);
    check(cur && strcmp(cur, "after the rm\n") == 0,
        "a removed file is created again at the next record"
    );
    check(s_count_newfile == 0, "a removed file created again: no newfile callback");
    GBMEM_FREE(cur);

    rotatory_close(hr);
    rmrdir(BASE);
}

/***************************************************************************
 *  keep_all: a size rotation never removes a piece of the day.
 *
 *  Up to 7.25.4 each size rotation renamed the file to .OLD and removed
 *  the previous .OLD: on a day that crossed the limit twice, the first
 *  part of the day was deleted (the agent audit of wattyzer lost the
 *  mornings of 22 and 23 September 2026 this way).
 ***************************************************************************/
PRIVATE void test_keep_all_old(void)
{
    rmrdir(BASE);
    mkrdir(AUDIT_DIR, 02775);

    hrotatory_h hr = rotatory_open(AUDIT_DIR "/" MASK, 0, 1, 1, 02775, 0660, FALSE);
    if(!hr) {
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    rotatory_keep_all_old_files(hr, TRUE);
    char current[PATH_MAX];
    snprintf(current, sizeof(current), "%s", rotatory_path(hr));

    /*
     *  7 megas of 1001-byte records: with a limit of 1 mega the file
     *  rotates when it is over 2 megas (whole megas), so three rotations
     */
    char line[1001];
    memset(line, 'k', sizeof(line)-1);
    line[sizeof(line)-1] = 0;
    int n_records = 7000;
    for(int i=0; i<n_records; i++) {
        rotatory_write(hr, LOG_AUDIT, line, strlen(line));
    }
    rotatory_flush(hr);

    char path[PATH_MAX+16];
    size_t lines = count_lines(current);
    int pieces = 0;
    for(int n=1; n<10; n++) {
        snprintf(path, sizeof(path), "%s.OLD.%d", current, n);
        if(exists_no_follow(path)) {
            pieces++;
            lines += count_lines(path);
        }
    }
    snprintf(path, sizeof(path), "%s.OLD", current);
    check(pieces >= 3 && !exists_no_follow(path), "keep_all: numbered .OLD.<n> pieces, no .OLD");
    check(lines == (size_t)n_records, "keep_all: every record of the day is kept");
    printf("     (%d pieces, %d records of %d)\n", pieces, (int)lines, n_records);

    /*
     *  The retention knows the numbered pieces
     */
    make_file(AUDIT_DIR "/001-01_01_2026.log.OLD.3", 10, 30);
    make_file(AUDIT_DIR "/001-01_01_2026.log.OLD.x", 10, 30);
    make_file(AUDIT_DIR "/001-01_01_2026.log.OLD.", 10, 30);
    int removed = rotatory_remove_old_files(hr, 7, NULL, NULL);
    check(removed == 1 && !exists_no_follow(AUDIT_DIR "/001-01_01_2026.log.OLD.3") &&
        exists_no_follow(AUDIT_DIR "/001-01_01_2026.log.OLD.x") &&
        exists_no_follow(AUDIT_DIR "/001-01_01_2026.log.OLD."),
        "keep_all: the retention removes an old .OLD.<n>, and nothing of another shape"
    );
    snprintf(path, sizeof(path), "%s.OLD.1", current);
    set_age(path, 30);
    removed = rotatory_remove_old_files(hr, 7, NULL, NULL);
    check(removed == 0 && exists_no_follow(path),
        "keep_all: a piece of the current day stays, even old"
    );

    rotatory_close(hr);
    rmrdir(BASE);
}

/***************************************************************************
 *  The free-space probe, faked for ONE directory.
 *
 *  This test binary is linked with -Wl,--wrap=statvfs,--wrap=fstatvfs
 *  (see CMakeLists.txt): the rotatory's calls land here, and the answer is
 *  the real one except for the files under s_fake_dir, which get
 *  s_fake_free_percent. Production code is not touched.
 ***************************************************************************/
PRIVATE char s_fake_dir[PATH_MAX] = "";
PRIVATE int s_fake_free_percent = -1;   // -1 = the real answer

int __real_statvfs(const char *path, struct statvfs *buf);
int __wrap_statvfs(const char *path, struct statvfs *buf);
int __real_fstatvfs(int fd, struct statvfs *buf);
int __wrap_fstatvfs(int fd, struct statvfs *buf);

PRIVATE void fake_free_space(const char *path, struct statvfs *buf)
{
    if(s_fake_free_percent < 0 || !*s_fake_dir) {
        return;
    }
    if(strncmp(path, s_fake_dir, strlen(s_fake_dir)) != 0) {
        return;
    }
    buf->f_blocks = 1000;
    buf->f_bavail = (fsblkcnt_t)s_fake_free_percent * 10;
    buf->f_bfree = buf->f_bavail;
}

int __wrap_statvfs(const char *path, struct statvfs *buf)
{
    int ret = __real_statvfs(path, buf);
    if(ret == 0) {
        fake_free_space(path, buf);
    }
    return ret;
}

int __wrap_fstatvfs(int fd, struct statvfs *buf)
{
    int ret = __real_fstatvfs(fd, buf);
    if(ret == 0) {
        char link[64];
        char path[PATH_MAX];
        snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
        ssize_t n = readlink(link, path, sizeof(path)-1);
        if(n > 0) {
            path[n] = 0;
            fake_free_space(path, buf);
        }
    }
    return ret;
}

PRIVATE size_t count_lines(const char *path)
{
    size_t len = 0;
    char *bf = read_whole_file(path, &len);
    size_t lines = 0;
    for(size_t i=0; bf && i<len; i++) {
        if(bf[i] == '\n') {
            lines++;
        }
    }
    GBMEM_FREE(bf);
    return lines;
}

/***************************************************************************
 *  A full disk stops ONE handle, and it writes again when the space is
 *  back. Up to 7.25.4 the "disk full" state was one flag for every
 *  handle and was never cleared: once any directory went below its
 *  min_free_disk_percentage, EVERY rotatory of the process stopped
 *  writing until the process was restarted.
 ***************************************************************************/
PRIVATE void test_disk_full_per_handle(void)
{
    rmrdir(BASE);
    mkrdir(BASE "/a", 02775);
    mkrdir(BASE "/b", 02775);

    /*
     *  `b` asks 1% (the real free space of /tmp answers for it, and a /tmp
     *  more than 80% full would stop it): when one flag served every
     *  handle, the full disk of `a` stopped `b` whatever `b` asked.
     */
    hrotatory_h hr_a = rotatory_open(BASE "/a/a-W.log", 0, 0, 20, 02775, 0660, FALSE);
    hrotatory_h hr_b = rotatory_open(BASE "/b/b-W.log", 0, 0, 1, 02775, 0660, FALSE);
    if(!hr_a || !hr_b) {
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    char path_a[PATH_MAX];
    char path_b[PATH_MAX];
    snprintf(path_a, sizeof(path_a), "%s", rotatory_path(hr_a));
    snprintf(path_b, sizeof(path_b), "%s", rotatory_path(hr_b));

    /*
     *  The disk of `a` goes below 20% free: 10%
     */
    snprintf(s_fake_dir, sizeof(s_fake_dir), "%s", BASE "/a");
    s_fake_free_percent = 10;
    for(int i=0; i<300; i++) {
        rotatory_write(hr_a, LOG_INFO, "a", 1);
        rotatory_write(hr_b, LOG_INFO, "b", 1);
    }
    rotatory_flush(NULL);
    size_t lines_a = count_lines(path_a);
    size_t lines_b = count_lines(path_b);
    check(lines_a < 300, "disk full: the handle on the full disk stops");
    check(lines_b == 300, "disk full: a handle on another disk goes on");

    /*
     *  The space is back
     */
    s_fake_free_percent = 50;
    for(int i=0; i<300; i++) {
        rotatory_write(hr_a, LOG_INFO, "a", 1);
    }
    rotatory_flush(NULL);
    size_t lines_a2 = count_lines(path_a);
    check(lines_a2 >= lines_a + 200, "disk full: the handle writes again when the space is back");
    printf("     (a: %d lines while full, %d after; b: %d)\n",
        (int)lines_a, (int)lines_a2, (int)lines_b);

    s_fake_free_percent = -1;
    s_fake_dir[0] = 0;
    rotatory_close(hr_a);
    rotatory_close(hr_b);
    rmrdir(BASE);
}

/***************************************************************************
 *  After rotatory_end() a handle is gone: a write through it, a flush,
 *  a truncate or a second close must not touch freed memory. The file
 *  log handler of glogger keeps its pointer, and entry_point logs after
 *  the end (up to 7.25.4 the leak report did).
 ***************************************************************************/
PRIVATE void test_write_after_end(void)
{
    rmrdir(BASE);
    mkrdir(BASE, 02775);

    hrotatory_h hr = rotatory_open(BASE "/end-W.log", 0, 0, 1, 02775, 0660, FALSE);
    if(!hr) {
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s", rotatory_path(hr));
    gobj_log_add_handler("to_file", "file", LOG_OPT_ALL, hr);
    gobj_log_info(0, 0, "msg", "%s", "before the end", NULL);

    rotatory_end();

    gobj_log_info(0, 0, "msg", "%s", "after the end", NULL);
    int ret = rotatory_write(hr, LOG_INFO, "after", 5);
    rotatory_fwrite(hr, LOG_INFO, "%s", "after");
    rotatory_flush(hr);
    rotatory_truncate(hr);
    rotatory_close(hr);                 // a second close: no double free
    gobj_log_del_handler("to_file");    // glogger closes it too: no double free

    size_t len = 0;
    char *bf = read_whole_file(path, &len);
    check(bf && strstr(bf, "before the end") && !strstr(bf, "after"),
        "after rotatory_end(): nothing written, nothing freed twice"
    );
    check(ret == 0, "after rotatory_end(): rotatory_write() answers 0 (other handlers go on)");
    GBMEM_FREE(bf);

    rotatory_start_up();    // for whatever runs after
    rmrdir(BASE);
}

/***************************************************************************
 *  The clock, faked.
 *
 *  This test binary is linked with -Wl,--wrap=time (see CMakeLists.txt):
 *  while s_fake_now is not 0, time() answers it, in the rotatory too.
 ***************************************************************************/
PRIVATE time_t s_fake_now = 0;

time_t __real_time(time_t *t);
time_t __wrap_time(time_t *t);

time_t __wrap_time(time_t *t)
{
    time_t now = s_fake_now? s_fake_now: __real_time(NULL);
    if(t) {
        *t = now;
    }
    return now;
}

/***************************************************************************
 *  The monotonic clock, faked too: -Wl,--wrap=clock_gettime (see
 *  CMakeLists.txt). CLOCK_MONOTONIC answers the real one plus
 *  s_fake_mono_seconds, so start_msectimer()/test_msectimer() of the
 *  rotatory see the steps of step_clocks(). It only moves forward.
 ***************************************************************************/
PRIVATE time_t s_fake_mono_seconds = 0;

int __real_clock_gettime(clockid_t clk, struct timespec *ts);
int __wrap_clock_gettime(clockid_t clk, struct timespec *ts);

int __wrap_clock_gettime(clockid_t clk, struct timespec *ts)
{
    int ret = __real_clock_gettime(clk, ts);
    if(ret == 0 && clk == CLOCK_MONOTONIC) {
        ts->tv_sec += s_fake_mono_seconds;
    }
    return ret;
}

/*
 *  `wall` seconds on the wall clock (s_fake_now, may be negative: the
 *  clock set back) and `mono` seconds of real time (the monotonic clock)
 */
PRIVATE void step_clocks(time_t wall, time_t mono)
{
    s_fake_now += wall;
    s_fake_mono_seconds += mono;
}

/***************************************************************************
 *  What the rotatory prints (print_error(): stdout and syslog, it is the
 *  sink of the log), taken from stdout. It is printed again after.
 ***************************************************************************/
#define CAPTURE_FILE "/tmp/test_rotatory_stdout.txt"
PRIVATE int s_saved_stdout = -1;

PRIVATE void capture_stdout_begin(void)
{
    fflush(stdout);
    s_saved_stdout = dup(STDOUT_FILENO);
    int fd = open(CAPTURE_FILE, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if(fd < 0 || s_saved_stdout < 0) {
        printf("FAIL cannot capture stdout\n");
        global_result += -1;
        return;
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
}

PRIVATE char *capture_stdout_end(void)
{
    fflush(stdout);
    if(s_saved_stdout >= 0) {
        dup2(s_saved_stdout, STDOUT_FILENO);
        close(s_saved_stdout);
        s_saved_stdout = -1;
    }
    size_t len = 0;
    char *bf = read_whole_file(CAPTURE_FILE, &len);
    unlink(CAPTURE_FILE);
    if(bf) {
        fwrite(bf, 1, len, stdout);
    }
    return bf;
}

PRIVATE int count_of(const char *text, const char *what)
{
    int n = 0;
    const char *p = text;
    while(p && (p = strstr(p, what)) != NULL) {
        n++;
        p += strlen(what);
    }
    return n;
}

PRIVATE BOOL file_holds(const char *path, const char *text)
{
    size_t len = 0;
    char *bf = read_whole_file(path, &len);
    BOOL found = (bf && strstr(bf, text))? TRUE: FALSE;
    GBMEM_FREE(bf);
    return found;
}

PRIVATE void file_of(time_t t, const char *dir, const char *mask, char *bf, size_t bfsize)
{
    char name[NAME_MAX];
    formatdate(t, name, sizeof(name), mask);
    build_path(bf, bfsize, dir, name, NULL);
}

/***************************************************************************
 *  A clock set back across midnight, and forward again, empties no file.
 *
 *  Up to 7.25.4 a new name was opened with "w": a step back of a few
 *  seconds at 00:00:05 opened the file of the day before again and emptied
 *  it, and the step forward emptied the file of today. For the agent audit
 *  (keep all) no existing file is ever emptied. For the yuno logs (the "W"
 *  mask, one file for each week day) the file of LAST week is still emptied
 *  when its day comes: a file is emptied only when it was last written
 *  before the day that its name is used for.
 ***************************************************************************/
PRIVATE void test_clock_set_back(void)
{
    #define CLOCK_DIR BASE "/clock"
    rmrdir(BASE);
    mkrdir(CLOCK_DIR, 02775);

    time_t real_now = __real_time(NULL);
    struct tm tm;
    localtime_r(&real_now, &tm);
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    time_t midnight = mktime(&tm);  // today 00:00, a day of the real clock (for the mtimes)

    /*
     *  The agent audit: keep all old files
     */
    char yesterday[PATH_MAX];
    char today[PATH_MAX];
    file_of(midnight - 2, CLOCK_DIR, MASK, yesterday, sizeof(yesterday));
    file_of(midnight + 5, CLOCK_DIR, MASK, today, sizeof(today));

    FILE *f = fopen(yesterday, "w");
    if(f) {
        fputs("{\"command\":\"yesterday\"}\n\n", f);
        fclose(f);
    }

    s_fake_now = midnight + 5;
    hrotatory_h hr = rotatory_open(CLOCK_DIR "/" MASK, 0, 500, 1, 02775, 0660, FALSE);
    if(!hr) {
        s_fake_now = 0;
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    rotatory_keep_all_old_files(hr, TRUE);
    rotatory_write(hr, LOG_AUDIT, "{\"command\":\"today 1\"}", strlen("{\"command\":\"today 1\"}"));
    s_fake_now = midnight - 2;      // the clock is set back 7 seconds
    rotatory_write(hr, LOG_AUDIT, "{\"command\":\"back\"}", strlen("{\"command\":\"back\"}"));
    s_fake_now = midnight + 10;     // and forward again
    rotatory_write(hr, LOG_AUDIT, "{\"command\":\"today 2\"}", strlen("{\"command\":\"today 2\"}"));
    rotatory_flush(hr);

    check(file_holds(yesterday, "\"yesterday\"") && file_holds(yesterday, "\"back\""),
        "clock set back: the audit file of yesterday is not emptied (the record goes on at its end)");
    check(file_holds(today, "\"today 1\"") && file_holds(today, "\"today 2\""),
        "clock forward again: the audit file of today keeps its first record");
    rotatory_close(hr);

    /*
     *  The yuno logs: the W mask, one file for each week day
     */
    char w_yesterday[PATH_MAX];
    char w_today[PATH_MAX];
    file_of(midnight - 2, CLOCK_DIR, "log-W.log", w_yesterday, sizeof(w_yesterday));
    file_of(midnight + 5, CLOCK_DIR, "log-W.log", w_today, sizeof(w_today));

    s_fake_now = midnight - 5;
    hr = rotatory_open(CLOCK_DIR "/log-W.log", 0, 500, 1, 02775, 0660, FALSE);
    if(!hr) {
        s_fake_now = 0;
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    rotatory_write(hr, LOG_INFO, "yesterday", strlen("yesterday"));
    rotatory_flush(hr);

    s_fake_now = 0;
    f = fopen(w_today, "w");
    if(f) {
        fputs("INFO: last week\n", f);
        fclose(f);
    }
    set_age(w_today, 7);            // written 7 days ago: last week's file

    s_fake_now = midnight + 5;
    rotatory_write(hr, LOG_INFO, "today 1", strlen("today 1"));
    rotatory_flush(hr);
    check(!file_holds(w_today, "last week") && file_holds(w_today, "today 1"),
        "W mask, new day: the file of last week is emptied (the name is the retention)");

    s_fake_now = midnight - 2;      // the clock is set back
    rotatory_write(hr, LOG_INFO, "back", strlen("back"));
    s_fake_now = midnight + 10;     // and forward again
    rotatory_write(hr, LOG_INFO, "today 2", strlen("today 2"));
    rotatory_flush(hr);
    s_fake_now = 0;

    check(file_holds(w_yesterday, "yesterday") && file_holds(w_yesterday, "back"),
        "W mask, clock set back: the file of yesterday is not emptied");
    check(file_holds(w_today, "today 1") && file_holds(w_today, "today 2"),
        "W mask, clock forward again: the file of today is not emptied");
    rotatory_close(hr);

    rmrdir(BASE);
}

/***************************************************************************
 *  A piece of 0 bytes writes nothing, and the file stays open.
 *  Up to 7.25.4 fwrite() of 0 bytes was taken as a failure: the file was
 *  closed, and nothing more was written until the next name (next day).
 ***************************************************************************/
PRIVATE void test_zero_length_piece(void)
{
    rmrdir(BASE);
    mkrdir(BASE, 02775);

    hrotatory_h hr = rotatory_open(BASE "/zero-W.log", 0, 500, 1, 02775, 0660, FALSE);
    if(!hr) {
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s", rotatory_path(hr));

    int errors = s_errors;
    rotatory_write(hr, LOG_AUDIT, "0123456789", 10);
    rotatory_write(hr, LOG_AUDIT, "", 0);
    for(int i=0; i<50; i++) {
        rotatory_write(hr, LOG_AUDIT, "0123456789", 10);
    }
    rotatory_flush(hr);

    struct stat st;
    BOOL ok = stat(path, &st) == 0 && st.st_size == 11 + 1 + 50*11;
    check(ok, "a piece of 0 bytes: nothing lost after it");
    if(!ok) {
        printf("     size %ld, expected %d\n", (long)st.st_size, 11 + 1 + 50*11);
    }
    check(s_errors == errors, "a piece of 0 bytes: no error");

    rotatory_close(hr);
    rmrdir(BASE);
}

/***************************************************************************
 *  A write that fails closes the file; the next record opens it again.
 *  The failure: a file size limit (RLIMIT_FSIZE, SIGXFSZ ignored),
 *  lifted afterwards.
 ***************************************************************************/
PRIVATE void test_write_failure_reopens(void)
{
    rmrdir(BASE);
    mkrdir(BASE, 02775);

    hrotatory_h hr = rotatory_open(BASE "/fail-W.log", 0, 500, 1, 02775, 0660, FALSE);
    if(!hr) {
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s", rotatory_path(hr));
    rotatory_subscribe2newfile(hr, count_newfile_cb, NULL);
    s_count_newfile = 0;
    s_count_newfile_same = 0;

    char line[1001];
    memset(line, 'f', sizeof(line)-1);
    line[sizeof(line)-1] = 0;

    struct sigaction sa_old;
    struct sigaction sa_ign;
    memset(&sa_ign, 0, sizeof(sa_ign));
    sa_ign.sa_handler = SIG_IGN;
    sigaction(SIGXFSZ, &sa_ign, &sa_old);

    struct rlimit rl_old;
    getrlimit(RLIMIT_FSIZE, &rl_old);
    struct rlimit rl = {20000, rl_old.rlim_max};
    setrlimit(RLIMIT_FSIZE, &rl);
    for(int i=0; i<100; i++) {
        rotatory_write(hr, LOG_AUDIT, line, strlen(line));  // 100 KB: fails at 20 000 bytes
    }
    rotatory_flush(hr);
    struct stat st;
    off_t size_at_failure = (stat(path, &st) == 0)? st.st_size: -1;

    setrlimit(RLIMIT_FSIZE, &rl_old);
    sigaction(SIGXFSZ, &sa_old, NULL);

    rotatory_write(hr, LOG_AUDIT, "after the limit", strlen("after the limit"));
    rotatory_flush(hr);
    off_t size_after = (stat(path, &st) == 0)? st.st_size: -1;

    check(size_at_failure > 0 && size_at_failure <= 20000,
        "a write that fails: the file stops at the limit");
    check(size_after > size_at_failure && file_holds(path, "after the limit"),
        "a write that fails: the next record opens the file again and is written");
    printf("     (size at the failure %ld, after %ld)\n", (long)size_at_failure, (long)size_after);
    check(s_count_newfile == 0,
        "a write that fails: the same file opened again is not a new file (no newfile callback)");
    if(s_count_newfile != 0) {
        printf("     newfile callback called %d times (%d with old == new)\n",
            s_count_newfile, s_count_newfile_same);
    }

    rotatory_close(hr);
    rmrdir(BASE);
}

/***************************************************************************
 *  The open applies the rule of a new name: a yuno that starts on the
 *  week day of an old "W" file does not go on with LAST week's file.
 *  Up to 7.25.4 rotatory_open() always appended: one file held 8 days.
 *  A file of today is appended to, and a mask with the year (the audit)
 *  never empties a file.
 ***************************************************************************/
PRIVATE void test_open_applies_the_day(void)
{
    #define OPEN_DIR BASE "/open"
    rmrdir(BASE);
    mkrdir(OPEN_DIR, 02775);

    char w_today[PATH_MAX];
    file_of(time(NULL), OPEN_DIR, "log-W.log", w_today, sizeof(w_today));

    /*
     *  Last week's file of this week day
     */
    FILE *f = fopen(w_today, "w");
    if(f) {
        fputs("INFO: last week\n", f);
        fclose(f);
    }
    set_age(w_today, 7);
    hrotatory_h hr = rotatory_open(OPEN_DIR "/log-W.log", 0, 500, 1, 02775, 0660, FALSE);
    if(!hr) {
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    rotatory_write(hr, LOG_INFO, "today 1", strlen("today 1"));
    rotatory_close(hr);
    check(!file_holds(w_today, "last week") && file_holds(w_today, "today 1"),
        "open, W mask: the file of last week is emptied");

    /*
     *  Opened again the same day: appended to
     */
    hr = rotatory_open(OPEN_DIR "/log-W.log", 0, 500, 1, 02775, 0660, FALSE);
    rotatory_write(hr, LOG_INFO, "today 2", strlen("today 2"));
    rotatory_close(hr);
    check(file_holds(w_today, "today 1") && file_holds(w_today, "today 2"),
        "open, W mask: the file of today is appended to");

    /*
     *  A mask with the year: never emptied, even old
     */
    char audit_today[PATH_MAX];
    file_of(time(NULL), OPEN_DIR, MASK, audit_today, sizeof(audit_today));
    f = fopen(audit_today, "w");
    if(f) {
        fputs("{\"command\":\"old\"}\n\n", f);
        fclose(f);
    }
    set_age(audit_today, 7);
    hr = rotatory_open(OPEN_DIR "/" MASK, 0, 500, 1, 02775, 0660, FALSE);
    rotatory_write(hr, LOG_AUDIT, "{\"command\":\"new\"}", strlen("{\"command\":\"new\"}"));
    rotatory_close(hr);
    check(file_holds(audit_today, "\"old\"") && file_holds(audit_today, "\"new\""),
        "open, mask with the year: never emptied");

    /*
     *  A fixed name (no date letter): one file for ever, never emptied
     */
    f = fopen(OPEN_DIR "/app.log", "w");
    if(f) {
        fputs("INFO: the history of many days\n", f);
        fclose(f);
    }
    set_age(OPEN_DIR "/app.log", 2);
    hr = rotatory_open(OPEN_DIR "/app.log", 0, 500, 1, 02775, 0660, FALSE);
    rotatory_write(hr, LOG_INFO, "today", strlen("today"));
    rotatory_close(hr);
    check(file_holds(OPEN_DIR "/app.log", "history") && file_holds(OPEN_DIR "/app.log", "today"),
        "open, fixed name (no date letter): not emptied on a later day");

    /*
     *  keep_all set right after the open applies to the file of the open:
     *  last week's "W" file is not emptied
     */
    f = fopen(w_today, "w");
    if(f) {
        fputs("INFO: last week, kept\n", f);
        fclose(f);
    }
    set_age(w_today, 7);
    hr = rotatory_open(OPEN_DIR "/log-W.log", 0, 500, 1, 02775, 0660, FALSE);
    rotatory_keep_all_old_files(hr, TRUE);
    rotatory_write(hr, LOG_INFO, "today 3", strlen("today 3"));
    rotatory_close(hr);
    check(file_holds(w_today, "last week, kept") && file_holds(w_today, "today 3"),
        "open, W mask, keep_all set right after the open: never emptied");

    /*
     *  A "MM" name (the month only): its file is the one of the month
     *  until the month ends. The 15th of this month, a file written on the
     *  12th is appended to; one written before the month began is emptied.
     */
    time_t real_now = __real_time(NULL);
    struct tm tm;
    localtime_r(&real_now, &tm);
    tm.tm_mday = 15;
    tm.tm_hour = 12;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    s_fake_now = mktime(&tm);

    char mm_path[PATH_MAX];
    file_of(s_fake_now, OPEN_DIR, "month-MM.log", mm_path, sizeof(mm_path));
    f = fopen(mm_path, "w");
    if(f) {
        fputs("INFO: the 12th\n", f);
        fclose(f);
    }
    set_age(mm_path, 3);    // time() is faked: 3 days before the 15th
    hr = rotatory_open(OPEN_DIR "/month-MM.log", 0, 500, 1, 02775, 0660, FALSE);
    rotatory_write(hr, LOG_INFO, "the 15th", strlen("the 15th"));
    rotatory_close(hr);
    check(file_holds(mm_path, "the 12th") && file_holds(mm_path, "the 15th"),
        "open, MM mask: a file written earlier in the month is appended to");

    f = fopen(mm_path, "w");
    if(f) {
        fputs("INFO: last year\n", f);
        fclose(f);
    }
    set_age(mm_path, 40);
    hr = rotatory_open(OPEN_DIR "/month-MM.log", 0, 500, 1, 02775, 0660, FALSE);
    rotatory_write(hr, LOG_INFO, "this month", strlen("this month"));
    rotatory_close(hr);
    check(!file_holds(mm_path, "last year") && file_holds(mm_path, "this month"),
        "open, MM mask: a file written before the month began is emptied");
    s_fake_now = 0;

    rmrdir(BASE);
}

/***************************************************************************
 *  keep_all and a rename that fails (a directory that refuses renames:
 *  chattr +a, a read-only bind, a MAC denial). The file is kept and
 *  grows, and the rename is NOT tried again at every record: each try
 *  was a line to syslog and a new open. The next name tries at once.
 *
 *  This test binary is linked with -Wl,--wrap=rename (see CMakeLists.txt).
 ***************************************************************************/
PRIVATE BOOL s_fail_rename = FALSE;
PRIVATE int s_rename_calls = 0;

int __real_rename(const char *oldpath, const char *newpath);
int __wrap_rename(const char *oldpath, const char *newpath);

int __wrap_rename(const char *oldpath, const char *newpath)
{
    s_rename_calls++;
    if(s_fail_rename) {
        errno = EACCES;
        return -1;
    }
    return __real_rename(oldpath, newpath);
}

PRIVATE void test_keep_all_rename_fails(void)
{
    #define RENAME_DIR BASE "/rename"
    rmrdir(BASE);
    mkrdir(RENAME_DIR, 02775);

    time_t real_now = __real_time(NULL);
    struct tm tm;
    localtime_r(&real_now, &tm);
    tm.tm_hour = 12;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    s_fake_now = mktime(&tm);

    hrotatory_h hr = rotatory_open(RENAME_DIR "/" MASK, 0, 1, 1, 02775, 0660, FALSE);
    if(!hr) {
        s_fake_now = 0;
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    rotatory_keep_all_old_files(hr, TRUE);
    rotatory_subscribe2newfile(hr, count_newfile_cb, NULL);
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s", rotatory_path(hr));

    char line[1024];
    memset(line, 'n', sizeof(line)-1);
    line[sizeof(line)-1] = 0;

    capture_stdout_begin();
    s_count_newfile = 0;
    s_rename_calls = 0;
    s_fail_rename = TRUE;
    int n_records = 4400;   // 4.4 MB: over the limit (whole megas > 1) from ~2 MB on
    for(int i=0; i<n_records; i++) {
        rotatory_write(hr, LOG_AUDIT, line, strlen(line));
    }
    rotatory_flush(hr);
    int calls_at_first = s_rename_calls;
    int newfile_at_first = s_count_newfile;
    size_t lines_at_first = count_lines(path);

    /*
     *  The retry: after 60 seconds of REAL time (the monotonic clock),
     *  whatever the wall clock does
     */
    step_clocks(30, 30);
    for(int i=0; i<100; i++) {
        rotatory_write(hr, LOG_AUDIT, line, strlen(line));
    }
    int calls_at_30 = s_rename_calls;

    step_clocks(31, 31);    // 61 s
    for(int i=0; i<100; i++) {
        rotatory_write(hr, LOG_AUDIT, line, strlen(line));
    }
    int calls_at_61 = s_rename_calls;

    step_clocks(-3600, 61); // the wall clock set back one hour, 61 s of real time
    for(int i=0; i<100; i++) {
        rotatory_write(hr, LOG_AUDIT, line, strlen(line));
    }
    int calls_clock_back = s_rename_calls;

    step_clocks(7200, 30);  // the wall clock set forward two hours, 30 s of real time
    for(int i=0; i<100; i++) {
        rotatory_write(hr, LOG_AUDIT, line, strlen(line));
    }
    int calls_clock_forward = s_rename_calls;

    /*
     *  The rename works again
     */
    s_fail_rename = FALSE;
    step_clocks(61, 61);
    int n_after = 2048;     // the first one rotates; 2 MB: over the limit, the next record of the day would rotate
    for(int i=0; i<n_after; i++) {
        rotatory_write(hr, LOG_AUDIT, line, strlen(line));
    }
    rotatory_flush(hr);
    int calls_works = s_rename_calls;
    int newfile_works = s_count_newfile;
    char old1_day1[PATH_MAX+16];
    snprintf(old1_day1, sizeof(old1_day1), "%s.OLD.1", path);
    char *printed = capture_stdout_end();

    check(calls_at_first == 1,
        "keep_all, rename fails: tried once, not at every record");
    check(newfile_at_first == 0,
        "keep_all, rename fails: no new file, no newfile callback");
    check(lines_at_first == (size_t)n_records,
        "keep_all, rename fails: the file is kept, every record in it");
    check(calls_at_30 == 1,
        "keep_all, rename fails: not tried again after 30 s");
    check(calls_at_61 == 2,
        "keep_all, rename fails: tried again after 60 s");
    check(calls_clock_back == 3,
        "keep_all, rename fails: the wall clock set back does not stop the retry (monotonic)");
    check(calls_clock_forward == 3,
        "keep_all, rename fails: the wall clock set forward does not bring the retry (monotonic)");
    check(calls_works == 4 && newfile_works == 1 && exists_no_follow(old1_day1) &&
        count_lines(old1_day1) == (size_t)n_records + 400 && count_lines(path) == (size_t)n_after,
        "keep_all, the rename works again: one new file, the newfile callback once");
    check(count_of(printed, "Cannot rename") == 1 && count_of(printed, "works again") == 1,
        "keep_all, rename fails: printed once, and once when it works again");
    printf("     (rename tried %d/%d/%d/%d/%d/%d times, newfile callback %d times)\n",
        calls_at_first, calls_at_30, calls_at_61, calls_clock_back, calls_clock_forward,
        calls_works, newfile_works);
    GBMEM_FREE(printed);

    /*
     *  The next name (next day) tries at once, and the rename works.
     *  The file of day 1 stays as it is: it is not size-rotated at the
     *  first record of day 2 (it was renamed to .OLD.<n> there, and the
     *  day lost its main file).
     */
    step_clocks(86400, 1);
    s_rename_calls = 0;
    for(int i=0; i<2200; i++) {
        rotatory_write(hr, LOG_AUDIT, line, strlen(line));
    }
    rotatory_flush(hr);
    char path2[PATH_MAX];
    snprintf(path2, sizeof(path2), "%s", rotatory_path(hr));
    char old1[PATH_MAX+16];
    snprintf(old1, sizeof(old1), "%s.OLD.1", path2);
    char old2_day1[PATH_MAX+16];
    snprintf(old2_day1, sizeof(old2_day1), "%s.OLD.2", path);
    check(strcmp(path, path2) != 0 && exists_no_follow(old1),
        "keep_all: the next name rotates at once, the rename works again");
    check(count_lines(path) == (size_t)n_after && !exists_no_follow(old2_day1),
        "keep_all: the file of day 1 is not size-rotated at the first record of day 2");
    printf("     (next day: rename tried %d times, newfile callback %d times)\n",
        s_rename_calls, s_count_newfile);

    rotatory_close(hr);
    s_fake_now = 0;
    rmrdir(BASE);
}

/***************************************************************************
 *  A new name is not size-rotated: the size limit is of the file being
 *  written, and at a new name the file of the day before is left. Up to
 *  this fix the size check ran on the file of the day before at the
 *  first record of the new day: that file was renamed to .OLD (the .OLD
 *  of that day, its earlier piece, was removed first), and when that
 *  rename failed the file of the NEW day was emptied.
 ***************************************************************************/
PRIVATE void test_new_name_no_size_rotation(void)
{
    #define NEWNAME_DIR BASE "/newname"
    rmrdir(BASE);
    mkrdir(NEWNAME_DIR, 02775);

    time_t real_now = __real_time(NULL);
    struct tm tm;
    localtime_r(&real_now, &tm);
    tm.tm_hour = 23;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    s_fake_now = mktime(&tm);

    hrotatory_h hr = rotatory_open(NEWNAME_DIR "/CCYY-MM-DD.log", 0, 1, 1, 02775, 0660, FALSE);
    if(!hr) {
        s_fake_now = 0;
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    char day1[PATH_MAX];
    snprintf(day1, sizeof(day1), "%s", rotatory_path(hr));
    char day1_old[PATH_MAX+8];
    snprintf(day1_old, sizeof(day1_old), "%s.OLD", day1);

    size_t n = 2200*1024;
    char *big = gbmem_malloc(n + 1);
    if(!big) {
        s_fake_now = 0;
        printf("FAIL no memory\n");
        global_result += -1;
        return;
    }
    memset(big, 'A', n);
    big[n] = 0;
    rotatory_write(hr, LOG_AUDIT, big, n);      // 2.2 MB of 'A'
    memset(big, 'B', n);
    rotatory_write(hr, LOG_AUDIT, big, n);      // the size rotation: 'A' to .OLD, 'B' in the file
    rotatory_flush(hr);

    step_clocks(2*3600, 2*3600);                // the next day, 01:00
    rotatory_write(hr, LOG_AUDIT, "day 2", strlen("day 2"));
    rotatory_flush(hr);

    check(file_holds(day1_old, "AAAA") && file_holds(day1, "BBBB"),
        "a new name: the file of the day before is not size-rotated (both pieces stay)");
    check(file_holds(rotatory_path(hr), "day 2"), "a new name: the record goes to the new file");
    rotatory_close(hr);

    /*
     *  Without keep_all, a rename that fails at a size rotation empties
     *  the file. At a new name that was the file of the new day: a file
     *  that already held records of that day (a restart, a clock set
     *  back) lost them.
     */
    rmrdir(BASE);
    mkrdir(NEWNAME_DIR, 02775);
    s_fake_now = mktime(&tm);
    hr = rotatory_open(NEWNAME_DIR "/CCYY-MM-DD.log", 0, 1, 1, 02775, 0660, FALSE);
    if(!hr) {
        GBMEM_FREE(big);
        s_fake_now = 0;
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    memset(big, 'A', n);
    rotatory_write(hr, LOG_AUDIT, big, n);      // over the limit at the next record
    rotatory_flush(hr);

    char day2[PATH_MAX];
    file_of(s_fake_now + 2*3600, NEWNAME_DIR, "CCYY-MM-DD.log", day2, sizeof(day2));
    FILE *f = fopen(day2, "w");
    if(f) {
        fputs("earlier today\n", f);
        fclose(f);
    }

    s_fail_rename = TRUE;
    step_clocks(2*3600, 2*3600);
    rotatory_write(hr, LOG_AUDIT, "day 2", strlen("day 2"));
    rotatory_flush(hr);
    s_fail_rename = FALSE;

    check(file_holds(day2, "earlier today") && file_holds(day2, "day 2"),
        "a new name, the rename would fail: the file of the new day is not emptied");
    rotatory_close(hr);

    GBMEM_FREE(big);
    s_fake_now = 0;
    rmrdir(BASE);
}

/***************************************************************************
 *  The newfile callback of a new day is not lost when the first open of
 *  the new name fails (EMFILE, a quota, a directory that refuses writes a
 *  moment): it runs at the next open that works. Before, the name and the
 *  day had already moved when the open failed, the next record opened the
 *  same name again (not a new file), and the callback of that day never
 *  ran: no retention of the agent audit, no summary of the logcenter.
 *  The failure here: a DIRECTORY with the name of the new file (fopen()
 *  refuses it, also for root), removed afterwards.
 ***************************************************************************/
PRIVATE int s_pending_newfile_calls = 0;

PRIVATE int newfile_pending_frees_space_cb(void *user_data, const char *old_filename, const char *new_filename)
{
    s_pending_newfile_calls++;
    s_fake_free_percent = 50;   // the retention frees the space
    return 0;
}

PRIVATE void test_newfile_after_failed_open(void)
{
    #define PENDING_DIR BASE "/pending"
    rmrdir(BASE);
    mkrdir(PENDING_DIR, 02775);

    time_t real_now = __real_time(NULL);
    struct tm tm;
    localtime_r(&real_now, &tm);
    tm.tm_hour = 23;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    time_t day1 = mktime(&tm);
    s_fake_now = day1;

    hrotatory_h hr = rotatory_open(PENDING_DIR "/" MASK, 0, 500, 1, 02775, 0660, FALSE);
    if(!hr) {
        s_fake_now = 0;
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    rotatory_keep_all_old_files(hr, TRUE);
    rotatory_subscribe2newfile(hr, count_newfile_cb, NULL);
    char path_day1[PATH_MAX];
    snprintf(path_day1, sizeof(path_day1), "%s", rotatory_path(hr));
    rotatory_write(hr, LOG_AUDIT, "day 1", strlen("day 1"));

    char path_day2[PATH_MAX];
    file_of(day1 + 2*3600, PENDING_DIR, MASK, path_day2, sizeof(path_day2));
    mkdir(path_day2, 0775);

    s_count_newfile = 0;
    s_count_newfile_same = 0;
    step_clocks(2*3600, 2*3600);                // the next day, 01:00
    capture_stdout_begin();
    rotatory_write(hr, LOG_AUDIT, "day 2 a", strlen("day 2 a"));   // the open fails
    char *printed = capture_stdout_end();
    int calls_failed = s_count_newfile;
    rmdir(path_day2);
    rotatory_write(hr, LOG_AUDIT, "day 2 b", strlen("day 2 b"));
    rotatory_write(hr, LOG_AUDIT, "day 2 c", strlen("day 2 c"));
    rotatory_flush(hr);

    check(count_of(printed, "Cannot open") == 1 && calls_failed == 0,
        "the open of a new day fails: printed, no newfile callback yet");
    check(s_count_newfile == 1 && s_count_newfile_same == 0 &&
        strcmp(s_newfile_old, path_day1) == 0 && strcmp(s_newfile_new, path_day2) == 0,
        "the open of a new day fails: the newfile callback runs once, at the next open that works");
    check(file_holds(path_day2, "day 2 b") && file_holds(path_day2, "day 2 c"),
        "the open of a new day fails: the next records are written");
    if(s_count_newfile != 1) {
        printf("     newfile callback %d times (old %s, new %s)\n",
            s_count_newfile, s_newfile_old, s_newfile_new);
    }
    GBMEM_FREE(printed);
    rotatory_close(hr);

    /*
     *  The same on a full disk: the callback is where the retention frees
     *  the space, so it is tried again (with the free space check, every
     *  MAX_COUNTER_STATVFS records) while the disk is full
     */
    rmrdir(BASE);
    mkrdir(PENDING_DIR, 02775);
    s_fake_now = day1;
    snprintf(s_fake_dir, sizeof(s_fake_dir), "%s", PENDING_DIR);
    s_fake_free_percent = 50;
    s_pending_newfile_calls = 0;

    hr = rotatory_open(PENDING_DIR "/" MASK, 0, 500, 20, 02775, 0660, FALSE);
    if(!hr) {
        s_fake_now = 0;
        s_fake_free_percent = -1;
        s_fake_dir[0] = 0;
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    rotatory_keep_all_old_files(hr, TRUE);
    rotatory_subscribe2newfile(hr, newfile_pending_frees_space_cb, NULL);

    s_fake_free_percent = 10;                   // below 20%: seen within 100 records
    for(int i=0; i<150; i++) {
        rotatory_write(hr, LOG_AUDIT, "x", 1);
    }
    mkdir(path_day2, 0775);
    step_clocks(2*3600, 2*3600);
    capture_stdout_begin();
    rotatory_write(hr, LOG_AUDIT, "day 2 a", strlen("day 2 a"));   // the open fails
    printed = capture_stdout_end();
    GBMEM_FREE(printed);
    int calls_full_failed = s_pending_newfile_calls;
    rmdir(path_day2);
    for(int i=0; i<150; i++) {
        rotatory_write(hr, LOG_AUDIT, "day 2 b", strlen("day 2 b"));
    }
    rotatory_flush(hr);

    check(calls_full_failed == 0 && s_pending_newfile_calls == 1,
        "full disk, the open of a new day fails: the newfile callback runs once, later");
    check(file_holds(path_day2, "day 2 b"),
        "full disk, the open of a new day fails: the retention frees the space, the records are written");
    if(s_pending_newfile_calls != 1) {
        printf("     newfile callback %d times\n", s_pending_newfile_calls);
    }

    rotatory_close(hr);
    s_fake_now = 0;
    s_fake_free_percent = -1;
    s_fake_dir[0] = 0;
    rmrdir(BASE);
}

/***************************************************************************
 *  A new day while the disk is full: the file of the new day is opened
 *  and the newfile callback runs, because the retention it applies is
 *  what frees the space. Before, nothing was done while the disk was
 *  full: the retention never ran, and the handle never wrote again.
 ***************************************************************************/
PRIVATE int s_full_newfile_calls = 0;

PRIVATE int newfile_frees_space_cb(void *user_data, const char *old_filename, const char *new_filename)
{
    s_full_newfile_calls++;
    if(s_full_newfile_calls == 2) {
        s_fake_free_percent = 50;   // the retention of the second day frees the space
    }
    return 0;
}

PRIVATE void test_disk_full_new_day(void)
{
    #define FULL_DIR BASE "/full"
    rmrdir(BASE);
    mkrdir(FULL_DIR, 02775);

    time_t real_now = __real_time(NULL);
    struct tm tm;
    localtime_r(&real_now, &tm);
    tm.tm_hour = 23;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    time_t day1 = mktime(&tm);

    s_fake_now = day1;
    snprintf(s_fake_dir, sizeof(s_fake_dir), "%s", FULL_DIR);
    s_fake_free_percent = 50;
    s_full_newfile_calls = 0;

    hrotatory_h hr = rotatory_open(FULL_DIR "/" MASK, 0, 500, 20, 02775, 0660, FALSE);
    if(!hr) {
        s_fake_now = 0;
        s_fake_free_percent = -1;
        printf("FAIL rotatory_open()\n");
        global_result += -1;
        return;
    }
    rotatory_keep_all_old_files(hr, TRUE);
    rotatory_subscribe2newfile(hr, newfile_frees_space_cb, hr);

    /*
     *  The disk goes below 20%: seen within 100 records
     */
    s_fake_free_percent = 10;
    for(int i=0; i<150; i++) {
        rotatory_write(hr, LOG_AUDIT, "x", 1);
    }

    /*
     *  Day 2 and day 3, still full until the retention of day 3 frees it
     */
    s_fake_now = day1 + 2*3600;     // next day, 01:00
    rotatory_write(hr, LOG_AUDIT, "day 2", strlen("day 2"));
    int calls_day2 = s_full_newfile_calls;
    char path_day2[PATH_MAX];
    snprintf(path_day2, sizeof(path_day2), "%s", rotatory_path(hr));

    s_fake_now += 86400;
    rotatory_write(hr, LOG_AUDIT, "day 3", strlen("day 3"));
    rotatory_flush(hr);
    int calls_day3 = s_full_newfile_calls;
    char path_day3[PATH_MAX];
    snprintf(path_day3, sizeof(path_day3), "%s", rotatory_path(hr));

    check(calls_day2 == 1 && calls_day3 == 2,
        "disk full: a new day still calls the newfile callback (the retention)");
    check(!file_holds(path_day2, "day 2"),
        "disk full: the record is dropped while the disk stays full");
    check(file_holds(path_day3, "day 3"),
        "disk full: the retention frees the space, the record of that moment is written");

    rotatory_close(hr);
    s_fake_now = 0;
    s_fake_free_percent = -1;
    s_fake_dir[0] = 0;
    rmrdir(BASE);
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;

    gbmem_get_allocators(
        &malloc_func,
        &realloc_func,
        &calloc_func,
        &free_func
    );

    json_set_alloc_funcs(
        malloc_func,
        free_func
    );

    unsigned long memory_check_list[] = {0}; // WARNING: list ended with 0
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

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
    rotatory_start_up();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("count_errors", 0, count_errors, 0);
    gobj_log_add_handler("count_errors", "count_errors", LOG_OPT_UP_ERROR, 0);

    test_retention();
    test_write_path();
    test_keep_all_old();
    test_disk_full_per_handle();
    test_clock_set_back();
    test_zero_length_piece();
    test_write_failure_reopens();
    test_open_applies_the_day();
    test_disk_full_new_day();
    test_keep_all_rename_fails();
    test_new_name_no_size_rotation();
    test_newfile_after_failed_open();
    test_write_after_end();     // LAST: it ends the rotatory

    rotatory_end();
    gobj_end();

    int result = global_result;
    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }
    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    } else {
        printf("\n%s: PASS\n", APP);
    }
    return result<0?-1:0;
}
