/****************************************************************************
 *          test_dir_listing.c
 *
 *          The answer of the agent's dir-* commands (dir-yuneta,
 *          dir-realms, dir-repos, dir-store, dir-logs, dir-local-data):
 *          yunos/c/yuno_agent/src/dir_listing.c, compiled into this test.
 *
 *          A tree that cannot be listed answers -1, with a comment that
 *          names the directory, and no data. Up to 7.25.4 each command
 *          ignored the return of get_ordered_filename_array() and answered
 *          the list it got: a directory that could not be opened (mode 0,
 *          no such directory) answered an EMPTY list with result 0, which
 *          reads as "the directory is empty".
 *
 *          Cases:
 *          1. a tree that can be listed: 0, its entries sorted;
 *          2. a directory of mode 0 (SKIPPED as root, which opens it): -1;
 *          3. a directory that does not exist: -1;
 *          4. a `match` that is not a regular expression: -1.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <yunetas.h>
#include "dir_listing.h"

#define APP     "test_dir_listing"
#define BASE    "/tmp/test_dir_listing"

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

/***************************************************************************
 *  Ask for the listing, check the answer
 ***************************************************************************/
PRIVATE void check_listing(
    const char *what,
    const char *directory,
    const char *match,
    int expected_result,
    const char *expected_data   // the data as ugly json, NULL: none expected
)
{
    json_t *response = build_dir_listing_response(0, directory, match, json_object());
    int result = (int)kw_get_int(0, response, "result", -999, 0);
    const char *comment = kw_get_str(0, response, "comment", "", 0);
    json_t *data = kw_get_dict_value(0, response, "data", 0, 0);

    char name[256];
    snprintf(name, sizeof(name), "%s: result %d", what, expected_result);
    ok_or_fail(result == expected_result, name);

    if(expected_result < 0) {
        snprintf(name, sizeof(name), "%s: the comment names the directory", what);
        ok_or_fail(strstr(comment, "cannot list") != NULL && strstr(comment, directory) != NULL, name);
        snprintf(name, sizeof(name), "%s: no list, not an empty one", what);
        ok_or_fail(!json_is_array(data), name);
    } else {
        char *s = json2uglystr(data);
        snprintf(name, sizeof(name), "%s: the entries (%s)", what, s? s: "");
        ok_or_fail(s && expected_data && strcmp(s, expected_data) == 0, name);
        GBMEM_FREE(s)
    }
    if(result != expected_result) {
        printf("     got result %d, comment '%s'\n", result, comment);
    }
    JSON_DECREF(response)
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_listings(void)
{
    rmrdir(BASE);
    mkrdir(BASE "/sub", 02770);
    int fd = newfile(BASE "/sub/b.log", 0660, FALSE);
    if(fd >= 0) {
        close(fd);
    }
    fd = newfile(BASE "/a.log", 0660, FALSE);
    if(fd >= 0) {
        close(fd);
    }

    check_listing("1. a tree that can be listed", BASE, ".*", 0,
        "[\"" BASE "/a.log\",\"" BASE "/sub\",\"" BASE "/sub/b.log\"]");

    if(geteuid() != 0) {
        chmod(BASE "/sub", 0);
        check_listing("2. a directory of mode 0", BASE "/sub", ".*", -1, NULL);
        chmod(BASE "/sub", 02770);
    } else {
        printf("2. SKIPPED, running as root: a directory of mode 0 is still listed\n");
    }

    check_listing("3. a directory that does not exist", BASE "/no-such-dir", ".*", -1, NULL);
    check_listing("4. a match that is not a regular expression", BASE, "([", -1, NULL);

    rmrdir(BASE);
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
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

    test_listings();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
