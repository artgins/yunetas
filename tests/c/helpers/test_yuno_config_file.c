/****************************************************************************
 *          test_yuno_config_file.c
 *
 *          The configuration files the agent writes for a yuno
 *          (bin/<n>-<role>^<name>.json): yunos/c/yuno_agent/src/
 *          yuno_config_file.c, compiled into this test.
 *
 *          The merged configuration of a yuno holds its secrets
 *          (client_secret, passwords). Up to 7.25.20 the agent wrote it
 *          with the regular-file permission of the environment, 0664:
 *          readable by every user of the node. Now it is 0640 (the yuno
 *          runs as the agent's user; the group is the one of the realm
 *          directory, as for the agent's own store, 0660), and a file
 *          that existed with a wider mode is narrowed too: open() does not
 *          change the mode of a file that exists.
 *
 *          Cases:
 *          1. a new file: 0640, with its content;
 *          2. a file that existed as 0664: 0640, with the new content;
 *          3. the files of an earlier launch that wrote more of them
 *             (<n>-<role>^<name>.json with n over the ones written now)
 *             are narrowed to 0640, never widened, never removed; the
 *             files of another yuno, a symbolic link and its target, and
 *             the files written now are not touched.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <yunetas.h>
#include "yuno_config_file.h"

#define APP     "test_yuno_config_file"
#define BASE    "/tmp/test_yuno_config_file"

PRIVATE int global_result = 0;

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void check(BOOL ok, const char *name)
{
    if(ok) {
        printf("ok   %s\n", name);
    } else {
        printf("FAIL %s\n", name);
        global_result += -1;
    }
}

/***************************************************************************
 *  The mode of a file, -1 if it cannot be read
 ***************************************************************************/
PRIVATE int mode_of(const char *path)
{
    struct stat st;
    if(stat(path, &st)<0) {
        return -1;
    }
    return (int)(st.st_mode & 07777);
}

/***************************************************************************
 *  Whether the file holds `text`
 ***************************************************************************/
PRIVATE BOOL file_is(const char *path, const char *text)
{
    char bf[256] = "";
    FILE *f = fopen(path, "r");
    if(!f) {
        return FALSE;
    }
    size_t n = fread(bf, 1, sizeof(bf)-1, f);
    fclose(f);
    bf[n] = 0;
    return strcmp(bf, text)==0? TRUE : FALSE;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_write(void)
{
    const char *path = BASE "/3-role^name.json";
    unlink(path);

    gbuffer_t *gbuf = gbuffer_create(256, 256);
    gbuffer_printf(gbuf, "{\"client_secret\": \"S3CR3T\"}");
    int ret = write_yuno_config_file(0, gbuf, path);
    check(ret == 0 && file_is(path, "{\"client_secret\": \"S3CR3T\"}"),
        "(new) written with its content");
    check(mode_of(path) == 0640, "(new) mode 0640, not readable by others");

    chmod(path, 0664);
    gbuf = gbuffer_create(256, 256);
    gbuffer_printf(gbuf, "{\"password\": \"hunter2\"}");
    ret = write_yuno_config_file(0, gbuf, path);
    check(ret == 0 && file_is(path, "{\"password\": \"hunter2\"}"),
        "(existing 0664) rewritten with the new content");
    check(mode_of(path) == 0640, "(existing 0664) narrowed to 0640");

    unlink(path);
}

/***************************************************************************
 *  Create `name` in BASE with `mode` and `text`
 ***************************************************************************/
PRIVATE void make_file(const char *name, int mode, const char *text)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), BASE, name, NULL);
    unlink(path);
    FILE *f = fopen(path, "w");
    if(f) {
        fputs(text, f);
        fclose(f);
    }
    chmod(path, (mode_t)mode);
}

/***************************************************************************
 *  The mode of `name` in BASE, not following a link
 ***************************************************************************/
PRIVATE int lmode_of(const char *name)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), BASE, name, NULL);
    struct stat st;
    if(lstat(path, &st)<0) {
        return -1;
    }
    return (int)(st.st_mode & 07777);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void test_stale(void)
{
    const char *files[] = {
        "1-role^name.json", "2-role^name.json", "3-role^name.json",
        "4-role^name.json", "5-role^name.json", "12-role^name.json",
        "7-role^name.json",                         // 0600
        "4-role^name2.json", "4-role.json", "4-other^name.json",
        "x-role^name.json", "-role^name.json", "4-role^name.json.bak",
        "role^name.sh",
        "outside.json",                             // target of the link
        0
    };
    for(int i=0; files[i]; i++) {
        make_file(files[i], strcmp(files[i], "7-role^name.json")==0? 0600 : 0664, "{}");
    }
    char link_path[PATH_MAX];
    build_path(link_path, sizeof(link_path), BASE, "6-role^name.json", NULL);
    unlink(link_path);
    if(symlink(BASE "/outside.json", link_path)<0) {
        check(FALSE, "(stale) cannot create the link of the test");
    }

    int ret = narrow_stale_yuno_config_files(0, BASE, "role^name", 3);
    check(ret == 0, "(stale) answers 0");

    check(lmode_of("4-role^name.json") == 0640, "(stale) 4 of 3 written: narrowed to 0640");
    check(lmode_of("5-role^name.json") == 0640, "(stale) 5 of 3 written: narrowed to 0640");
    check(lmode_of("12-role^name.json") == 0640, "(stale) 12 of 3 written: narrowed to 0640");
    check(lmode_of("7-role^name.json") == 0600, "(stale) a 0600 one is not widened");
    check(lmode_of("1-role^name.json") == 0664 &&
          lmode_of("2-role^name.json") == 0664 &&
          lmode_of("3-role^name.json") == 0664,
        "(stale) the ones written now (1..3) are not touched");
    check(lmode_of("4-role^name2.json") == 0664 &&
          lmode_of("4-role.json") == 0664 &&
          lmode_of("4-other^name.json") == 0664,
        "(stale) the files of another yuno are not touched");
    check(lmode_of("x-role^name.json") == 0664 &&
          lmode_of("-role^name.json") == 0664 &&
          lmode_of("4-role^name.json.bak") == 0664 &&
          lmode_of("role^name.sh") == 0664,
        "(stale) a name that is not <n>-<role>^<name>.json is not touched");
    check(lmode_of("outside.json") == 0664, "(stale) a link is not followed: its target is not touched");
    check(lmode_of("4-role^name.json") != -1 && lmode_of("12-role^name.json") != -1,
        "(stale) nothing is removed");

    unlink(link_path);
    for(int i=0; files[i]; i++) {
        char path[PATH_MAX];
        build_path(path, sizeof(path), BASE, files[i], NULL);
        unlink(path);
    }
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

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);

    /*
     *  The environment of a yuno as entry_point.c registers it by default
     */
    register_yuneta_environment(BASE, "", 02775, 0664);
    mkrdir(BASE, 02775);

    test_write();
    test_stale();

    rmdir(BASE);

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
