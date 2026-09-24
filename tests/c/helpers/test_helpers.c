/****************************************************************************
 *          test_helpers.c
 *
 *          Unit tests for split2() / split_free2() (string split helper).
 *          Includes a reentrancy regression: split2() must NOT clobber a
 *          caller's in-progress strtok() parse (the strtok -> strtok_r fix).
 *          And save_json_to_file(): a failure is never silent.
 *          And rmrdir() / rmrcontentdir() / mkrdir() with symbolic links,
 *          and with an entry that disappears during the walk.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <yunetas.h>

#define APP "test_helpers"

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE yev_loop_h yev_loop;
PRIVATE int global_result = 0;

/***************************************************************************
 *  Compare a split2() result against a NULL-terminated expected array,
 *  also checking the reported size. Frees the list with split_free2().
 ***************************************************************************/
PRIVATE void check_split(
    const char *str,
    const char *delim,
    const char **expected,   // NULL-terminated
    const char *name
)
{
    int expected_size = 0;
    while(expected[expected_size]) {
        expected_size++;
    }

    int list_size = 0;
    const char **list = split2(str, delim, &list_size);

    int ok = 1;
    if(list_size != expected_size) {
        ok = 0;
    } else {
        for(int i=0; i<expected_size; i++) {
            if(strcmp(list[i], expected[i]) != 0) {
                ok = 0;
                break;
            }
        }
    }

    if(ok) {
        printf("ok   %-40s size=%d\n", name, list_size);
    } else {
        printf("FAIL %-40s got size=%d\n", name, list_size);
        global_result += -1;
    }

    split_free2(list);
}

/***************************************************************************
 *  Basic splitting
 ***************************************************************************/
PRIVATE void test_split_basic(void)
{
    check_split("a,b,c", ",", (const char*[]){"a","b","c",NULL}, "basic comma split");
    check_split("abc", ",", (const char*[]){"abc",NULL}, "single token, no delim");
    check_split("a:b;c", ":;", (const char*[]){"a","b","c",NULL}, "multi-char delim set");
}

/***************************************************************************
 *  Empty strings are dropped (documented HACK in split2)
 ***************************************************************************/
PRIVATE void test_split_empties_excluded(void)
{
    check_split("a,,b", ",", (const char*[]){"a","b",NULL}, "drop empty between delims");
    check_split(",a,b,", ",", (const char*[]){"a","b",NULL}, "drop leading/trailing empty");
    check_split("", ",", (const char*[]){NULL}, "empty input -> empty list");
    check_split(",,,", ",", (const char*[]){NULL}, "all-delims -> empty list");
}

/***************************************************************************
 *  plist_size NULL must not crash
 ***************************************************************************/
PRIVATE void test_split_null_size_arg(void)
{
    const char **list = split2("a,b", ",", NULL);
    if(list && list[0] && strcmp(list[0],"a")==0 && list[1] && strcmp(list[1],"b")==0) {
        printf("ok   %-40s\n", "NULL list_size arg");
    } else {
        printf("FAIL %-40s\n", "NULL list_size arg");
        global_result += -1;
    }
    split_free2(list);
}

/***************************************************************************
 *  Reentrancy regression (the strtok -> strtok_r fix):
 *  a caller that is mid-strtok() must keep its parse intact across a
 *  split2() call. The OLD split2() used the global-state strtok()
 *  internally, so calling it between two strtok(NULL,...) steps clobbered
 *  the caller's parse pointer (left dangling into split2's freed scratch
 *  buffer). With strtok_r inside split2(), the caller's strtok() is untouched.
 *
 *  NOTE: this test uses the global strtok() ON PURPOSE to play the role of
 *  such a caller; it is not production code.
 ***************************************************************************/
PRIVATE void test_split_reentrancy(void)
{
    char outer[] = "x1,x2,x3";

    char *t1 = strtok(outer, ",");          // caller starts a GLOBAL strtok parse
    int n = 0;
    const char **inner = split2("a;b;c", ";", &n);  // ... and calls split2 mid-parse
    split_free2(inner);
    char *t2 = strtok(NULL, ",");           // must still yield "x2"
    char *t3 = strtok(NULL, ",");           // must still yield "x3"

    if(t1 && strcmp(t1,"x1")==0 &&
       t2 && strcmp(t2,"x2")==0 &&
       t3 && strcmp(t3,"x3")==0) {
        printf("ok   %-40s\n", "outer strtok survives split2");
    } else {
        printf("FAIL %-40s t1=%s t2=%s t3=%s\n", "outer strtok survives split2",
            t1?t1:"(null)", t2?t2:"(null)", t3?t3:"(null)");
        global_result += -1;
    }
}

/***************************************************************************
 *  A version has to compare as a version, and the arithmetic has to fit.
 *
 *  This was an `int` accumulating up to 1000^4, so a four-segment release
 *  with its revision -- "1.9.0.0-2" -- overflowed and came out NEGATIVE. The
 *  agent compares releases with this to decide which binary is the newest,
 *  and on a client node it read 1.9.0.0-2 as SMALLER than 1.7.1.0-2 and
 *  promoted the OLD one, re-appending it on every restart for eleven days.
 *  The store showed it plainly: 1.9.0.0 written at 13:46:25, 1.7.1.0 written
 *  back two seconds later.
 *
 *  The cases below are that node's real version chain.
 ***************************************************************************/
PRIVATE void test_version_cmp(void)
{
    /*
     *  Ordered pairs: the right one must compare NEWER than the left one.
     */
    struct { const char *older; const char *newer; const char *why; } pairs[] = {
        /*  The pair that cost eleven days on a client node  */
        {"1.7.1.0-2",   "1.9.0.0-2",    "four segments plus a revision"},
        /*  The rest of that same chain, in order  */
        {"1.5.3.0-2",   "1.5.4.0-2",    "chain"},
        {"1.5.4.0-2",   "1.6.5.0-2",    "chain"},
        {"1.6.8.0-2",   "1.7.0.0-2",    "chain"},
        {"1.7.0.0-2",   "1.7.1.0-2",    "chain"},
        {"1.8.0.0-2",   "1.8.1.0-2",    "chain"},
        {"1.8.1.0-2",   "1.9.0.0-2",    "chain"},
        /*  The revision orders within one release  */
        {"1.9.0.0-1",   "1.9.0.0-2",    "revision"},
        /*  Numbers, not strings: 10 comes after 9  */
        {"7.9.11",      "7.10.0",       "numeric, not lexicographic"},
        {"7.10.0-1",    "7.11.0-1",     "SDK chain"},
        /*  A segment over 999 -- ANY packing that weighs segments by 1000
            gets this backwards, whatever the width of the accumulator  */
        {"2.0.0",       "2.1000.0",     "segment above the packing base"},
        {"1.2000.0",    "2.0.0",        "a big minor is still a minor"},
        /*  Segments far past what an int64 packing could hold  */
        {"1.2.3.4.5.6.7.8",  "1.2.3.4.5.6.7.9",  "eight segments"},
        {0, 0, 0}
    };

    int failed = 0;
    for(int i = 0; pairs[i].older; i++) {
        if(!(version_cmp(pairs[i].newer, pairs[i].older) > 0)) {
            printf("FAIL %-40s %s should be NEWER than %s (%s)\n",
                "version_cmp ordering",
                pairs[i].newer, pairs[i].older, pairs[i].why
            );
            failed++;
        }
        /*  and the other way round, so it is a comparison and not a coin  */
        if(!(version_cmp(pairs[i].older, pairs[i].newer) < 0)) {
            printf("FAIL %-40s %s should be OLDER than %s (%s)\n",
                "version_cmp is symmetric",
                pairs[i].older, pairs[i].newer, pairs[i].why
            );
            failed++;
        }
    }

    /*
     *  Equal is equal, and a missing segment is a zero.
     */
    struct { const char *a; const char *b; const char *why; } equals[] = {
        {"1.9.0.0-2",   "1.9.0.0-2",    "identical"},
        {"7.11",        "7.11.0",       "missing segment counts as zero"},
        {"7.11.0",      "7.11.0.0.0",   "trailing zeros do not matter"},
        {"",            "",             "two empty versions"},
        {0, 0, 0}
    };
    for(int i = 0; equals[i].a; i++) {
        if(version_cmp(equals[i].a, equals[i].b) != 0) {
            printf("FAIL %-40s '%s' vs '%s' (%s)\n",
                "version_cmp equality", equals[i].a, equals[i].b, equals[i].why
            );
            failed++;
        }
    }

    /*
     *  An absent version is the oldest thing there is, and NULL must not
     *  crash: the agent reads these straight out of a treedb record.
     */
    if(!(version_cmp("1.0.0", "") > 0) || !(version_cmp("", "1.0.0") < 0)) {
        printf("FAIL %-40s\n", "version_cmp against an empty version");
        failed++;
    }
    if(!(version_cmp("1.0.0", 0) > 0) || !(version_cmp(0, "1.0.0") < 0) ||
            version_cmp(0, 0) != 0) {
        printf("FAIL %-40s\n", "version_cmp against NULL");
        failed++;
    }

    if(!failed) {
        printf("ok   %-40s\n", "version_cmp orders releases");
    } else {
        global_result += -1;
    }
}

/***************************************************************************
 *              Test
 *  HACK: return -1 to fail, 0 to ok
 ***************************************************************************/
/***************************************************************************
 *  save_json_to_file(): every failure leaves a trace. With create=FALSE a
 *  missing directory answered -1 and logged nothing.
 ***************************************************************************/
PRIVATE int s_errors = 0;

PRIVATE char s_last_error[4096];

PRIVATE int count_errors(void *h, int priority, const char *bf, size_t len)
{
    snprintf(s_last_error, sizeof(s_last_error), "%.*s", (int)len, bf);
    s_errors++;
    return 0;
}

PRIVATE void test_save_json_to_file(void)
{
    char dir[PATH_MAX];
    build_path(dir, sizeof(dir), "/tmp", "test_helpers_no_such_dir", "sub", NULL);
    rmrdir("/tmp/test_helpers_no_such_dir");

    int errors_before = s_errors;
    int ret = save_json_to_file(
        0, dir, "x.json", 02770, 0660, 0,
        FALSE,  // do not create
        FALSE,
        json_pack("{s:i}", "a", 1)  // owned
    );
    if(ret == -1 && s_errors - errors_before == 1 && !is_directory(dir)) {
        printf("ok   %-40s\n", "save_json_to_file: missing dir is logged");
    } else {
        printf("FAIL %-40s ret=%d errors=%d\n", "save_json_to_file: missing dir is logged",
            ret, s_errors - errors_before);
        global_result += -1;
    }

    /*  ...and the ordinary save still works and logs nothing  */
    errors_before = s_errors;
    ret = save_json_to_file(
        0, dir, "x.json", 02770, 0660, 0,
        TRUE,   // create
        FALSE,
        json_pack("{s:i}", "a", 1)  // owned
    );
    json_t *back = load_json_from_file(0, dir, "x.json", 0);
    if(ret == 0 && s_errors == errors_before && json_integer_value(json_object_get(back, "a")) == 1) {
        printf("ok   %-40s\n", "save_json_to_file: create and read back");
    } else {
        printf("FAIL %-40s ret=%d\n", "save_json_to_file: create and read back", ret);
        global_result += -1;
    }
    JSON_DECREF(back)
    rmrdir("/tmp/test_helpers_no_such_dir");
}

/***************************************************************************
 *  rmrdir() / rmrcontentdir() never walk through a symbolic link.
 *
 *  Up to 7.25.4 both used stat(), which follows the link: a link to a
 *  directory was walked into and the TARGET's files were deleted (outside
 *  the tree being removed), and a dangling link made the walk fail.
 *  A link is removed as a link, never descended.
 ***************************************************************************/
#define RMR_BASE "/tmp/test_helpers_rmrdir"

PRIVATE void write_small_file(const char *path)
{
    FILE *f = fopen(path, "w");
    if(f) {
        fputs("keep me\n", f);
        fclose(f);
    }
}

PRIVATE int build_tree_with_links(const char *tree)
{
    char path[PATH_MAX];

    mkrdir(RMR_BASE "/outside", 02775);
    write_small_file(RMR_BASE "/outside/keep.txt");

    build_path(path, sizeof(path), tree, "sub", NULL);
    mkrdir(path, 02775);
    build_path(path, sizeof(path), tree, "sub", "file.txt", NULL);
    write_small_file(path);

    int ret = 0;
    build_path(path, sizeof(path), tree, "link_to_outside_dir", NULL);
    ret += symlink(RMR_BASE "/outside", path);
    build_path(path, sizeof(path), tree, "sub", "link_to_outside_file", NULL);
    ret += symlink(RMR_BASE "/outside/keep.txt", path);
    build_path(path, sizeof(path), tree, "dangling", NULL);
    ret += symlink(RMR_BASE "/no/such/target", path);
    return ret;
}

PRIVATE void check_outside_intact(const char *name)
{
    if(is_regular_file(RMR_BASE "/outside/keep.txt")) {
        printf("ok   %-40s\n", name);
    } else {
        printf("FAIL %-40s the file OUTSIDE the tree was deleted\n", name);
        global_result += -1;
    }
}

PRIVATE void test_rmrdir_symlinks(void)
{
    char tree[PATH_MAX];
    struct stat st;

    /*
     *  rmrdir() of a tree holding links
     */
    rmrdir(RMR_BASE);
    build_path(tree, sizeof(tree), RMR_BASE, "store", NULL);
    if(build_tree_with_links(tree) != 0) {
        printf("FAIL %-40s cannot build the tree\n", "rmrdir: tree with links");
        global_result += -1;
        return;
    }
    int errors_before = s_errors;
    int ret = rmrdir(tree);
    if(ret == 0 && lstat(tree, &st) != 0 && s_errors == errors_before) {
        printf("ok   %-40s\n", "rmrdir: tree with links removed");
    } else {
        printf("FAIL %-40s ret=%d errors=%d\n", "rmrdir: tree with links removed",
            ret, s_errors - errors_before);
        global_result += -1;
    }
    check_outside_intact("rmrdir: link target untouched");

    /*
     *  rmrdir() of a path that IS a link to a directory: the link goes
     */
    char link_path[PATH_MAX];
    build_path(link_path, sizeof(link_path), RMR_BASE, "link_itself", NULL);
    if(symlink(RMR_BASE "/outside", link_path) == 0) {
        ret = rmrdir(link_path);
        if(ret == 0 && lstat(link_path, &st) != 0 && is_directory(RMR_BASE "/outside")) {
            printf("ok   %-40s\n", "rmrdir: a link path removes the link");
        } else {
            printf("FAIL %-40s ret=%d\n", "rmrdir: a link path removes the link", ret);
            global_result += -1;
        }
        check_outside_intact("rmrdir: link path target untouched");
    }

    /*
     *  rmrcontentdir() of a tree holding links
     */
    build_tree_with_links(tree);
    errors_before = s_errors;
    ret = rmrcontentdir(tree);
    if(ret == 0 && is_directory(tree) && s_errors == errors_before) {
        char path[PATH_MAX];
        build_path(path, sizeof(path), tree, "dangling", NULL);
        if(lstat(path, &st) != 0) {
            printf("ok   %-40s\n", "rmrcontentdir: content with links removed");
        } else {
            printf("FAIL %-40s dangling link left\n", "rmrcontentdir: content with links removed");
            global_result += -1;
        }
    } else {
        printf("FAIL %-40s ret=%d errors=%d\n", "rmrcontentdir: content with links removed",
            ret, s_errors - errors_before);
        global_result += -1;
    }
    check_outside_intact("rmrcontentdir: link target untouched");

    rmrdir(RMR_BASE);
}

/***************************************************************************
 *  An entry that disappears during the walk (another process removed it
 *  between readdir() and lstat()) is already gone: that is what the walk
 *  wants, so the walk goes on, and nothing fails without a log.
 *  Up to 7.25.5-dev the lstat() of the walk returned -1 on ENOENT with no
 *  log, and every level above answered -1 as "Error already logged".
 *
 *  This test binary is linked with -Wl,--wrap=lstat (see CMakeLists.txt):
 *  the lstat() of s_vanish_path removes that path first, once.
 ***************************************************************************/
PRIVATE char s_vanish_path[PATH_MAX] = "";

int __real_lstat(const char *path, struct stat *st);
int __wrap_lstat(const char *path, struct stat *st);

int __wrap_lstat(const char *path, struct stat *st)
{
    if(*s_vanish_path && strcmp(path, s_vanish_path) == 0) {
        s_vanish_path[0] = 0;
        char cmd[PATH_MAX + 16];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
        if(system(cmd) != 0) {
            printf("FAIL cannot remove %s\n", path);
            global_result += -1;
        }
    }
    return __real_lstat(path, st);
}

PRIVATE void make_vanish_tree(const char *tree)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), tree, "sub", NULL);
    mkrdir(path, 02775);
    build_path(path, sizeof(path), tree, "sub", "file.txt", NULL);
    write_small_file(path);
    build_path(path, sizeof(path), tree, "file1.txt", NULL);
    write_small_file(path);
    build_path(path, sizeof(path), tree, "file2.txt", NULL);
    write_small_file(path);
}

PRIVATE void check_vanish(const char *name, int ret, int errors, BOOL gone_ok)
{
    if(ret == 0 && errors == 0 && gone_ok) {
        printf("ok   %-40s\n", name);
    } else {
        printf("FAIL %-40s ret=%d errors=%d\n", name, ret, errors);
        global_result += -1;
    }
}

PRIVATE void test_rmrdir_entry_vanishes(void)
{
    char tree[PATH_MAX];
    struct stat st;
    const char *vanishing[] = {"file1.txt", "sub", 0};

    rmrdir(RMR_BASE);
    build_path(tree, sizeof(tree), RMR_BASE, "vanish", NULL);

    for(int i=0; vanishing[i]; i++) {
        char name[128];

        make_vanish_tree(tree);
        build_path(s_vanish_path, sizeof(s_vanish_path), tree, vanishing[i], NULL);
        int errors_before = s_errors;
        int ret = rmrdir(tree);
        snprintf(name, sizeof(name), "rmrdir: %s disappears in the walk", vanishing[i]);
        check_vanish(name, ret, s_errors - errors_before, lstat(tree, &st) != 0);

        make_vanish_tree(tree);
        build_path(s_vanish_path, sizeof(s_vanish_path), tree, vanishing[i], NULL);
        errors_before = s_errors;
        ret = rmrcontentdir(tree);
        snprintf(name, sizeof(name), "rmrcontentdir: %s disappears", vanishing[i]);
        char path[PATH_MAX];
        build_path(path, sizeof(path), tree, "file2.txt", NULL);
        check_vanish(name, ret, s_errors - errors_before,
            is_directory(tree) && lstat(path, &st) != 0);
        rmrdir(tree);
    }
    s_vanish_path[0] = 0;

    rmrdir(RMR_BASE);
}

/***************************************************************************
 *  mkrdir() over a path that exists and is not a directory must fail,
 *  and say so. Up to 7.25.4 the check was `stat() != 0 && !S_ISDIR()`,
 *  which is never true, so it answered 0 with no directory there.
 ***************************************************************************/
PRIVATE void test_mkrdir_not_a_directory(void)
{
    rmrdir(RMR_BASE);
    mkrdir(RMR_BASE, 02775);
    write_small_file(RMR_BASE "/a_file");

    int errors_before = s_errors;
    int ret = mkrdir(RMR_BASE "/a_file", 02775);
    if(ret == -1 && s_errors - errors_before == 1) {
        printf("ok   %-40s\n", "mkrdir: a file in place of the dir");
    } else {
        printf("FAIL %-40s ret=%d errors=%d\n", "mkrdir: a file in place of the dir",
            ret, s_errors - errors_before);
        global_result += -1;
    }

    /*  the log names the real cause: ENOTDIR, not a stale errno  */
    char enotdir[64];
    snprintf(enotdir, sizeof(enotdir), "\"errno\": %d,", ENOTDIR);
    if(strstr(s_last_error, enotdir)) {
        printf("ok   %-40s\n", "mkrdir: the log says ENOTDIR");
    } else {
        printf("FAIL %-40s %s\n", "mkrdir: the log says ENOTDIR", s_last_error);
        global_result += -1;
    }

    /*  ...also for a file in the MIDDLE of the path  */
    errno = ENOENT;
    errors_before = s_errors;
    ret = mkrdir(RMR_BASE "/a_file/sub", 02775);
    if(ret == -1 && s_errors - errors_before == 1 && strstr(s_last_error, enotdir)) {
        printf("ok   %-40s\n", "mkrdir: a file in the middle, ENOTDIR");
    } else {
        printf("FAIL %-40s ret=%d errors=%d %s\n", "mkrdir: a file in the middle, ENOTDIR",
            ret, s_errors - errors_before, s_last_error);
        global_result += -1;
    }

    /*  a dangling link where the directory goes: -1 and logged, never 0  */
    if(symlink(RMR_BASE "/no/such/target", RMR_BASE "/dangling") != 0) {
        printf("FAIL %-40s cannot create the link\n", "mkrdir: a dangling link");
        global_result += -1;
    }
    errors_before = s_errors;
    ret = mkrdir(RMR_BASE "/dangling", 02775);
    if(ret == -1 && s_errors - errors_before == 1) {
        printf("ok   %-40s\n", "mkrdir: a dangling link is an error");
    } else {
        printf("FAIL %-40s ret=%d errors=%d\n", "mkrdir: a dangling link is an error",
            ret, s_errors - errors_before);
        global_result += -1;
    }

    /*  a link to a directory is a directory for mkdir -p  */
    if(symlink(RMR_BASE, RMR_BASE "/link_to_base") != 0) {
        printf("FAIL %-40s cannot create the link\n", "mkrdir: through a link to a dir");
        global_result += -1;
    }
    errors_before = s_errors;
    ret = mkrdir(RMR_BASE "/link_to_base/sub", 02775);
    if(ret == 0 && is_directory(RMR_BASE "/sub") && s_errors == errors_before) {
        printf("ok   %-40s\n", "mkrdir: through a link to a dir");
    } else {
        printf("FAIL %-40s ret=%d\n", "mkrdir: through a link to a dir", ret);
        global_result += -1;
    }
    rmrdir(RMR_BASE);
}

/***************************************************************************
 *  find_files_with_suffix_array() lists regular files only: never a
 *  symbolic link, never a directory. Up to 7.25.4 the DT_UNKNOWN path
 *  (filesystems that give no d_type) used stat() and listed a link to a
 *  file, while the d_type path did not. On this test's filesystem the
 *  d_type path runs; the test guards the contract both paths now share.
 ***************************************************************************/
PRIVATE void test_find_files_with_suffix(void)
{
    rmrdir(RMR_BASE);
    mkrdir(RMR_BASE "/dir.md2", 02775);
    write_small_file(RMR_BASE "/a.md2");
    write_small_file(RMR_BASE "/b.json");
    int ret = symlink(RMR_BASE "/a.md2", RMR_BASE "/link.md2");
    ret += symlink(RMR_BASE "/none", RMR_BASE "/dangling.md2");

    dir_array_t da;
    find_files_with_suffix_array(0, RMR_BASE, ".md2", &da);
    if(ret == 0 && da.count == 1 && strcmp(da.items[0], "a.md2") == 0) {
        printf("ok   %-40s\n", "find_files_with_suffix: regular only");
    } else {
        printf("FAIL %-40s count=%d\n", "find_files_with_suffix: regular only", (int)da.count);
        global_result += -1;
    }
    dir_array_free(&da);
    rmrdir(RMR_BASE);
}

/***************************************************************************
 *  gbuffer_base64_to_binary() decodes base64_len chars, not up to a '\0'.
 *  Up to 7.25.4 the length was ignored: a slice of a longer text (the
 *  value of content64='...' inside a command line) failed on the quote.
 ***************************************************************************/
PRIVATE void test_base64_slice(void)
{
    const char *text = "content64='QUJD' id=x";
    const char *slice = text + strlen("content64='");
    gbuffer_t *gbuf = gbuffer_base64_to_binary(slice, 4);
    if(gbuf && gbuffer_leftbytes(gbuf) == 3 &&
            memcmp(gbuffer_cur_rd_pointer(gbuf), "ABC", 3) == 0) {
        printf("ok   %-40s\n", "base64: a slice of a text is decoded");
    } else {
        printf("FAIL %-40s\n", "base64: a slice of a text is decoded");
        global_result += -1;
    }
    GBUFFER_DECREF(gbuf)

    gbuf = gbuffer_base64_to_binary("QUI=", 4);
    if(gbuf && gbuffer_leftbytes(gbuf) == 2 &&
            memcmp(gbuffer_cur_rd_pointer(gbuf), "AB", 2) == 0) {
        printf("ok   %-40s\n", "base64: padding at the end of the slice");
    } else {
        printf("FAIL %-40s\n", "base64: padding at the end of the slice");
        global_result += -1;
    }
    GBUFFER_DECREF(gbuf)
}

PRIVATE int do_test(void)
{
    test_save_json_to_file();
    test_rmrdir_symlinks();
    test_rmrdir_entry_vanishes();
    test_mkrdir_not_a_directory();
    test_find_files_with_suffix();
    test_base64_slice();
    test_split_basic();
    test_split_empties_excluded();
    test_split_null_size_arg();
    test_split_reentrancy();
    test_version_cmp();

    return global_result;
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*----------------------------------*
     *      Startup gobj system
     *----------------------------------*/
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

    /*--------------------------------*
     *      Log handlers
     *--------------------------------*/
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("count_errors", 0, count_errors, 0);
    gobj_log_add_handler("count_errors", "count_errors", LOG_OPT_UP_ERROR, 0);

    /*--------------------------------*
     *  Create the event loop
     *--------------------------------*/
    yev_loop_create(
        0,
        2024,
        10,
        NULL,
        &yev_loop
    );

    /*--------------------------------*
     *      Test
     *--------------------------------*/
    int result = do_test();

    /*--------------------------------*
     *  Stop the event loop
     *--------------------------------*/
    yev_loop_stop(yev_loop);
    yev_loop_destroy(yev_loop);

    gobj_end();

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
