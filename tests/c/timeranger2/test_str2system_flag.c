/****************************************************************************
 *          test_str2system_flag.c
 *
 *  Regression coverage for tranger2_str2system_flag(): every name maps to
 *  its own bit of system_flag2_t.
 *
 *  idx_in_list() counts from 0, and the function used to apply a count from
 *  1 (`idx > 0 -> 1 << (idx-1)`): sf_string_key mapped to nothing,
 *  sf_rowid_key to sf_string_key, sf_int_key to sf_rowid_key, and sf_t_ms /
 *  sf_tm_ms to unused bits. It survived because every caller in the tree
 *  passes "sf_string_key" (-> 0) and tranger2_create_topic() falls back to a
 *  string key when the topic has a pkey. `create-topic system_flag=sf_int_key`
 *  created a rowid-key topic.
 *
 *  An unknown name is logged, not dropped in silence.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <limits.h>

#include <gobj.h>
#include <kwid.h>
#include <timeranger2.h>
#include <helpers.h>
#include <testing.h>

#define APP "test_str2system_flag"

/***************************************************************
 *              Data
 ***************************************************************/
typedef struct {
    const char *names;
    system_flag2_t expected;
} case_t;

PRIVATE const case_t cases[] = {
    {"sf_string_key",               sf_string_key},
    {"sf_rowid_key",                sf_rowid_key},
    {"sf_int_key",                  sf_int_key},
    {"sf_zip_record",               sf_zip_record},
    {"sf_cipher_record",            sf_cipher_record},
    {"sf_t_ms",                     sf_t_ms},
    {"sf_tm_ms",                    sf_tm_ms},
    {"sf_deleted_instance",         sf_deleted_instance},
    {"sf_immutable_record",         sf_immutable_record},
    {"sf_loading_from_disk",        sf_loading_from_disk},
    {"SF_INT_KEY",                  sf_int_key},
    {"sf_string_key|sf_t_ms",       sf_string_key|sf_t_ms},
    {"sf_int_key, sf_tm_ms",        sf_int_key|sf_tm_ms},
    {"sf_rowid_key sf_t_ms|sf_tm_ms", sf_rowid_key|sf_t_ms|sf_tm_ms},
    {"",                            0},
    {NULL, 0}
};

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int check_case(const char *names, system_flag2_t expected)
{
    system_flag2_t got = tranger2_str2system_flag(names);
    if(got != expected) {
        printf("%sERROR%s --> '%s': got 0x%04X, expected 0x%04X\n",
            On_Red BWhite, Color_Off, names, (unsigned)got, (unsigned)expected);
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  do_test
 ***************************************************************************/
PRIVATE int do_test(void)
{
    int result = 0;

    /*-------------------------------------*
     *  Every name, alone and combined
     *-------------------------------------*/
    set_expected_results("names map to their own bit", NULL, NULL, NULL, 0);
    for(int i = 0; cases[i].names; i++) {
        result += check_case(cases[i].names, cases[i].expected);
    }
    result += test_json(NULL);

    /*-------------------------------------*
     *  Unknown names: logged, not counted
     *-------------------------------------*/
    set_expected_results(
        "unknown names are logged",
        json_pack("[{s:s},{s:s}]",
            "msg", "Unknown system_flag name, ignored",
            "msg", "Unknown system_flag name, ignored"
        ),
        NULL, NULL, 0
    );
    result += check_case("sf_bogus", 0);
    result += check_case("sf_int_key|sf_bogus", sf_int_key);
    result += test_json(NULL);

    return result;
}

/***************************************************************************
 *              Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    sys_malloc_fn_t malloc_func;
    sys_realloc_fn_t realloc_func;
    sys_calloc_fn_t calloc_func;
    sys_free_fn_t free_func;
    gbmem_get_allocators(&malloc_func, &realloc_func, &calloc_func, &free_func);
    json_set_alloc_funcs(malloc_func, free_func);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    init_backtrace_with_backtrace(argv[0]);
    set_show_backtrace_fn(show_backtrace_with_backtrace);

    gobj_start_up(argc, argv, NULL, NULL, NULL, NULL, NULL, NULL);

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("testing", 0, capture_log_write, 0);
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    int result = do_test();

    gobj_end();

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result < 0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP);
    } else {
        printf("<-- %sTEST OK%s: %s\n", On_Green BWhite, Color_Off, APP);
    }
    return result < 0? -1 : 0;
}
