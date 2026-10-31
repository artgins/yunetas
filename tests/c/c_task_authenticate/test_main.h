/****************************************************************************
 *          TEST_MAIN.H
 *
 *          Shared yuno entry point for the c_task_authenticate tests.
 *
 *          Each main_test*.c is just a few lines: a per-test JSON snippet
 *          for the C_TASK_AUTHENTICATE service (`task_kw_snippet`) and a
 *          call to run_task_authenticate_test().  All the boilerplate —
 *          variable_config template, log-capture handler, yuneta_setup —
 *          lives here.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#pragma once

#include <yunetas.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************
 *  run_task_authenticate_test
 *
 *  app_name              APP_NAME for yuneta_entry_point
 *  app_doc               APP_DOC for yuneta_entry_point
 *  task_kw_snippet       JSON object literal (single-quoted, will be
 *                        normalised) used as the `kw` of the
 *                        task-authenticate service.  Must include the
 *                        IdP attrs the test exercises (issuer or
 *                        token_endpoint+end_session_endpoint) plus
 *                        credentials (client_id/user_id/user_passw)
 *                        so the task can run.
 *  expected_result       expected EV_ON_TOKEN result field passed to
 *                        the C_TEST_DRIVER service.
 *  expected_log_msg      single expected log message string (matched as
 *                        a `msg` field, kw_match_simple subset semantics),
 *                        or, starting with "[", a json list of the `msg`
 *                        of each log expected, in order.
 *                        May be NULL — no warning/error is expected.
 *                        We accept a string (not json_t) so the json_pack
 *                        can run AFTER yuneta_entry_point has installed
 *                        the gbmem allocators in jansson; otherwise the
 *                        array would be libc-allocated and test_json's
 *                        JSON_DECREF would later free it through gbmem.
 *  capture_warnings      if TRUE, the test_capture handler is at
 *                        LOG_OPT_UP_WARNING (else LOG_OPT_UP_ERROR).
 *
 *  Returns the same int that yuneta_entry_point returns, plus any
 *  test-level errors detected by test_json().
 ***************************************************************************/
/*
 *  Before run_task_authenticate_test(): run the task `runs` times (stopped
 *  and started again after each EV_ON_TOKEN but the last), stopping it
 *  inside its EV_ON_TOKEN when `stop_in_on_token`, as ycli does. After the
 *  last run the driver logs an ERROR for any C_PROT_HTTP_CL or C_TCP left.
 */
PUBLIC void set_task_runs(int runs, BOOL stop_in_on_token);

/*
 *  Before run_task_authenticate_test(): the test goes on `ms` after
 *  EV_ON_TOKEN (200 by default), for what comes after it: the logout.
 */
PUBLIC void set_task_end_delay(int ms);

PUBLIC int run_task_authenticate_test(
    int argc, char *argv[],
    const char *app_name,
    const char *app_doc,
    const char *task_kw_snippet,
    const char *mock_idp_kw_snippet,    /* "{}" for the default mock */
    int expected_result,
    const char *expected_log_msg,
    BOOL capture_warnings
);

#ifdef __cplusplus
}
#endif
