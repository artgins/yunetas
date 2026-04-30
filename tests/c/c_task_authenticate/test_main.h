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
 *                        task-authenticate service.  Must include
 *                        whatever IdP attrs the test exercises (issuer
 *                        / token_endpoint+end_session_endpoint /
 *                        auth_url) plus credentials (azp/user_id/
 *                        user_passw) so the task can run.
 *  expected_result       expected EV_ON_TOKEN result field passed to
 *                        the C_TEST_DRIVER service.
 *  expected_log_messages JSON array of expected log messages (consumed
 *                        FIFO by capture_log_write).  May be NULL — an
 *                        empty array is used in that case.  Owned.
 *  capture_warnings      if TRUE, the test_capture handler is at
 *                        LOG_OPT_UP_WARNING (else LOG_OPT_UP_ERROR).
 *
 *  Returns the same int that yuneta_entry_point returns, plus any
 *  test-level errors detected by test_json().
 ***************************************************************************/
PUBLIC int run_task_authenticate_test(
    int argc, char *argv[],
    const char *app_name,
    const char *app_doc,
    const char *task_kw_snippet,
    const char *mock_idp_kw_snippet,    /* "{}" for the default mock */
    int expected_result,
    json_t *expected_log_messages,
    BOOL capture_warnings
);

#ifdef __cplusplus
}
#endif
