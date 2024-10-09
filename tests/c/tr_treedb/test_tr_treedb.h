/****************************************************************************
 *          tests
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <gobj.h>
#include <tr_treedb.h>

#ifdef __cplusplus
extern "C"{
#endif

PUBLIC int test_departments(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
);
PUBLIC int test_departments_final(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
);

PUBLIC int test_users(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
);

PUBLIC int test_compound(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
);

extern char foto_final_departments[];
extern char foto_final_users[];
extern char foto_final1[];
extern char foto_final2[];
extern char users_file[];

#ifdef __cplusplus
}
#endif
