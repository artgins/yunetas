/****************************************************************************
 *          test_spawn_copy.c
 *
 *          run_process2() (root-linux, run_command.c) and copyfile()
 *          (gobj-c, helpers.c), both written again after 7.28.4 to take out
 *          code of others: the system() of TLPI (GPL-3.0) and an answer of
 *          StackOverflow (CC BY-SA).
 *
 *          run_process2(): the status of waitpid(2) for a program that
 *          exits 0 or not, the argv as given (no shell), -1 for a program
 *          that does not exist.
 *          copyfile(): the same bytes, the mode asked, an existing
 *          destination kept unless `overwrite`, -1 for a missing source.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <yunetas.h>

#define APP "test_spawn_copy"

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

    printf("     (an ERROR \"posix_spawn() FAILED\" follows, on purpose)\n");
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

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("FAIL system memory not free: %lu\n", (unsigned long)get_cur_system_memory());
        print_track_mem();
        global_result += -1;
    }

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result == 0 ? 0 : -1;
}
