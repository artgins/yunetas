/****************************************************************************
 *          test_build_path.c
 *
 *          Unit tests for build_path() path assembly + clamped '..'
 *          normalization (path-traversal confinement).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP "test_build_path"

/***************************************************************************
 *      Data
 ***************************************************************************/
PRIVATE int global_result = 0;

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

    // NULL / zero-size buffers are no-ops, not crashes.
    char *r = build_path(NULL, 16, "a", "b", NULL);
    if(r != NULL) {
        printf("FAIL null buffer guard\n");
        global_result += -1;
    } else {
        printf("ok   null buffer guard\n");
    }

    build_path(bf, sizeof(bf), ".", "..", NULL);
    check(bf, ".", "first segment is the locked root (not normalized)");
}

/***************************************************************************
 *      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    test_basic();
    test_dot_normalization();
    test_traversal_confinement();
    test_edge_cases();

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result;
}
