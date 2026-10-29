/****************************************************************************
 *          test_build_path.c
 *
 *          Unit tests for build_path() path assembly + clamped '..'
 *          normalization (path-traversal confinement), and its answer to
 *          a path that does not fit: NULL, errno ENAMETOOLONG and one
 *          ERROR, the buffer holding the segments that fitted (up to
 *          7.29.2 it answered the buffer, so every `!build_path()` guard of
 *          the tree was dead and the cut path was used).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP "test_build_path"

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE int global_result = 0;
PRIVATE int s_errors = 0;
PRIVATE char s_last_log[1024];

/***************************************************************************
 *  The log, captured: the overflow must say so, once
 ***************************************************************************/
PRIVATE int capture_write(void *h, int priority, const char *bf, size_t len)
{
    if(priority <= LOG_ERR) {
        s_errors++;
    }
    if(len >= sizeof(s_last_log)) {
        len = sizeof(s_last_log) - 1;
    }
    memcpy(s_last_log, bf, len);
    s_last_log[len] = 0;
    return 0;
}

PRIVATE void check_true(BOOL ok, const char *name)
{
    if(!ok) {
        printf("FAIL %s\n", name);
        global_result += -1;
    } else {
        printf("ok   %s\n", name);
    }
}

/***************************************************************************
 *  Assert helper
 ***************************************************************************/
PRIVATE void check(const char *got, const char *expected, const char *name)
{
    if(strcmp(got, expected) != 0) {
        printf("FAIL %-34s got '%s'  expected '%s'\n", name, got, expected);
        global_result += -1;
    } else {
        printf("ok   %-34s '%s'\n", name, got);
    }
}

/***************************************************************************
 *  Tests
 ***************************************************************************/
PRIVATE void test_basic(void)
{
    char bf[PATH_MAX];

    build_path(bf, sizeof(bf), "/var", "log", "app", NULL);
    check(bf, "/var/log/app", "basic absolute join");

    build_path(bf, sizeof(bf), "realms", "owner", "id", NULL);
    check(bf, "realms/owner/id", "basic relative join");

    build_path(bf, sizeof(bf), "/only", NULL);
    check(bf, "/only", "single absolute segment");

    build_path(bf, sizeof(bf), "/a/", "b/", NULL);
    check(bf, "/a/b", "strip stray slashes");

    build_path(bf, sizeof(bf), "/r", "a//b", NULL);
    check(bf, "/r/a/b", "collapse duplicate slashes");

    build_path(bf, sizeof(bf), "", "a", "", "b", NULL);
    check(bf, "a/b", "skip empty segments");

    build_path(bf, sizeof(bf), "base", "outputs/yunos", "src", NULL);
    check(bf, "base/outputs/yunos/src", "multi-component segment");
}

PRIVATE void test_dot_normalization(void)
{
    char bf[PATH_MAX];

    build_path(bf, sizeof(bf), "/var", "log", ".", "app", NULL);
    check(bf, "/var/log/app", "drop single dot");

    build_path(bf, sizeof(bf), "a", "b/../c", NULL);
    check(bf, "a/c", "resolve .. inside tail");

    build_path(bf, sizeof(bf), "/root", "a", "..", "b", NULL);
    check(bf, "/root/b", ".. pops previous tail segment");

    build_path(bf, sizeof(bf), "/a/b", "c", "..", NULL);
    check(bf, "/a/b", "trailing .. lands back at root");
}

PRIVATE void test_traversal_confinement(void)
{
    char bf[PATH_MAX];

    // The security cases: a '..'-laden tail must never escape the first
    // (trusted root) segment. Leftover components stay confined under it.
    build_path(bf, sizeof(bf), "/yuneta/store", "../../etc/passwd", NULL);
    check(bf, "/yuneta/store/etc/passwd", "clamp absolute root escape");

    build_path(bf, sizeof(bf), "realms", "../../x", NULL);
    check(bf, "realms/x", "clamp relative root escape");

    build_path(bf, sizeof(bf), "/a", "../../../../etc", NULL);
    check(bf, "/a/etc", "clamp deep escape");

    build_path(bf, sizeof(bf), "/store", "..", "..", NULL);
    check(bf, "/store", "clamp .. with no tail left");

    build_path(bf, sizeof(bf), "/data", "a/../../../b", NULL);
    check(bf, "/data/b", "clamp mixed traversal");
}

PRIVATE void test_edge_cases(void)
{
    char bf[PATH_MAX];

    // Overflow must not crash and must stay terminated within the buffer.
    build_path(bf, 4, "/aa", "bb", NULL);
    check(bf, "/aa", "no overflow on tiny buffer");

    // NULL / zero-size buffers answer NULL, logged, errno EINVAL.
    // Up to 7.29.3 both were silent, and a size 0 answered the buffer.
    int errors_before = s_errors;
    errno = 0;
    char *r = build_path(NULL, 16, "a", "b", NULL);
    check_true(r == NULL, "null buffer answers NULL");
    check_true(errno == EINVAL, "null buffer sets errno EINVAL");
    check_true(s_errors == errors_before + 1, "null buffer logs one ERROR");

    char zero[4] = {'x', 'x', 'x', 'x'};
    errors_before = s_errors;
    errno = 0;
    r = build_path(zero, 0, "a", "b", NULL);
    check_true(r == NULL, "size 0 answers NULL");
    check_true(errno == EINVAL, "size 0 sets errno EINVAL");
    check_true(s_errors == errors_before + 1, "size 0 logs one ERROR");
    check_true(zero[0] == 'x', "size 0 writes nothing");

    // A root made only of '/' stays: up to 7.29.3 it was lost ("tmp").
    build_path(bf, 8, "/", "tmp", NULL);
    check(bf, "/tmp", "root / is kept");

    build_path(bf, sizeof(bf), "//", "/tmp/", "x", NULL);
    check(bf, "/tmp/x", "root // is one /");

    build_path(bf, sizeof(bf), "/", NULL);
    check(bf, "/", "root / alone");

    build_path(bf, sizeof(bf), "/", "..", "etc", NULL);
    check(bf, "/etc", "root / clamps ..");

    errors_before = s_errors;
    r = build_path(bf, 5, "/", "tmp", NULL);           // "/tmp" needs 5
    check_true(r == bf && s_errors == errors_before, "root / and tmp just fit");
    check(bf, "/tmp", "root / and tmp whole");

    build_path(bf, sizeof(bf), ".", "..", NULL);
    check(bf, ".", "first segment is the locked root (not normalized)");
}

PRIVATE void test_overflow_answers_null(void)
{
    char bf[PATH_MAX];

    int errors_before = s_errors;
    errno = 0;
    char *r = build_path(bf, 6, "/aa", "bb", NULL);    // "/aa/bb" needs 7
    check_true(r == NULL, "overflow answers NULL");
    check(bf, "/aa", "overflow keeps the segments that fit");
    check_true(errno == ENAMETOOLONG, "overflow sets errno ENAMETOOLONG");
    check_true(s_errors == errors_before + 1 && strstr(s_last_log, "Path too long") != NULL,
        "overflow logs one ERROR \"Path too long\"");

    errors_before = s_errors;
    r = build_path(bf, 7, "/aa", "bb", NULL);           // exactly fits
    check_true(r == bf, "a path that just fits answers the buffer");
    check(bf, "/aa/bb", "a path that just fits is whole");
    check_true(s_errors == errors_before, "a path that fits logs nothing");

    char first[PATH_MAX + 1];
    memset(first, 'a', sizeof(first) - 1);
    first[0] = '/';
    first[sizeof(first) - 1] = 0;
    errors_before = s_errors;
    r = build_path(bf, sizeof(bf), first, NULL);        // the first segment alone
    check_true(r == NULL && bf[0] == 0, "a first segment that does not fit answers NULL, empty");
    check_true(s_errors == errors_before + 1, "and logs one ERROR");
}

/***************************************************************************
 *      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    glog_init();
    gobj_log_register_handler("capture", NULL, capture_write, NULL);
    gobj_log_add_handler("capture", "capture", LOG_OPT_ALL, 0);

    test_basic();
    test_dot_normalization();
    test_traversal_confinement();
    test_edge_cases();
    test_overflow_answers_null();

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result;
}
