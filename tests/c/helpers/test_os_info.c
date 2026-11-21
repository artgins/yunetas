/****************************************************************************
 *          test_os_info.c
 *
 *          get_os_info(): each value 64 bytes at most, cut at a whole UTF-8
 *          character. The agent puts it in the card it presents to its
 *          control center (list-agents, list-nodes), and os-release is a
 *          file anybody with root writes. Up to 7.31.29 a PRETTY_NAME went
 *          whole, whatever its length.
 *
 *          The test links with --wrap=fopen: "/etc/os-release" is a file of
 *          its own (fmemopen), every other path the real one.
 *
 *          Cases:
 *          1. a PRETTY_NAME of 70 bytes whose 64th byte is inside a 2-byte
 *             character: cut to 63 bytes, the character not split;
 *          2. a NAME of 100 ASCII bytes: cut to 64;
 *          3. an ID and a VERSION_ID that fit: whole.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP     "test_os_info"

PRIVATE int global_result = 0;

/*
 *  63 ASCII bytes, then "é" (2 bytes: bytes 64 and 65), then 5 more
 */
#define PRETTY_63   "Debian GNU/Linux 13 (trixie) with a very long name, made longer"
#define PRETTY_70   PRETTY_63 "\xc3\xa9" "ABCDE"
#define NAME_100    "NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN"

PRIVATE const char os_release[] =
    "ID=debian\n"
    "NAME=\"" NAME_100 "\"\n"
    "VERSION_ID=\"13\"\n"
    "PRETTY_NAME=\"" PRETTY_70 "\"\n";

FILE *__real_fopen(const char *path, const char *mode);
FILE *__wrap_fopen(const char *path, const char *mode);

/***************************************************************************
 *  The os-release of the test, the rest as it is
 ***************************************************************************/
FILE *__wrap_fopen(const char *path, const char *mode)
{
    if(path && strcmp(path, "/etc/os-release") == 0) {
        return fmemopen((void *)os_release, strlen(os_release), "r");
    }
    return __real_fopen(path, mode);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void check_value(json_t *jn_os, const char *key, const char *expected)
{
    const char *got = json_string_value(json_object_get(jn_os, key));
    if(got && strcmp(got, expected) == 0) {
        printf("ok   %-12s %zu bytes\n", key, strlen(got));
    } else {
        printf("FAIL %-12s '%s' (expected '%s')\n", key, got? got : "(null)", expected);
        global_result += -1;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
int main(int argc, char *argv[])
{
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

    json_t *jn_os = get_os_info();
    check_value(jn_os, "pretty_name", PRETTY_63);
    check_value(jn_os, "name", "NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN");
    check_value(jn_os, "id", "debian");
    check_value(jn_os, "version_id", "13");
    JSON_DECREF(jn_os)

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
