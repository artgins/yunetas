/****************************************************************************
 *          tests
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <ghelpers.h>

#ifdef __cplusplus
extern "C"{
#endif

PUBLIC void set_expected_results(
    const char *name,
    json_t *errors_list,
    int verbose
);

PUBLIC BOOL match_record(
    json_t *record, // NOT owned
    json_t *expected, // NOT owned
    int verbose,
    GBUFFER *gbuf_path
);
PUBLIC BOOL match_list(
    json_t *list, // NOT owned
    json_t *expected, // NOT owned
    int verbose,
    GBUFFER *gbuf_path
);
PUBLIC BOOL check_log_result(const char *test, int verbose);

PUBLIC BOOL test_departments(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
);

PUBLIC BOOL test_users(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
);

PUBLIC BOOL test_final_foto(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
);

PUBLIC BOOL test_compound(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
);

PUBLIC BOOL test_final_foto2(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
);

#ifdef __cplusplus
}
#endif

