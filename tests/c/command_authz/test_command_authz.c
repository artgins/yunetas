/****************************************************************************
 *          test_command_authz.c
 *
 *          Regression test for the re-armed per-command authorization gate
 *          in command_parser() (SDF_AUTHZ_X).
 *
 *          Verifies the gated-opt-in posture:
 *            1. attr off (default)         -> SDF_AUTHZ_X command runs (no break)
 *            2. attr on, no permission     -> command DENIED (-403), not run
 *            3. attr on, self src (==gobj) -> bypass, command runs
 *            4. attr on, permission granted-> command runs
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP "test_command_authz"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int  global_result = 0;
PRIVATE BOOL g_allow = FALSE;   /* verdict returned by the test authz checker */
PRIVATE int  g_executed = 0;    /* incremented by the guarded command */

GOBJ_DEFINE_GCLASS(C_AUTHZTEST);

/***************************************************************
 *              Guarded command + test authz checker
 ***************************************************************/
PRIVATE json_t *cmd_secret(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    g_executed++;
    KW_DECREF(kw)
    return build_command_response(gobj, 0, json_sprintf("ok"), 0, 0);
}

/* Owns kw (contract: the checker frees it). Returns the controllable verdict. */
PRIVATE BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return g_allow;
}

/***************************************************************
 *              GClass scaffolding
 ***************************************************************/
PRIVATE sdata_desc_t command_table[] = {
SDATACM2(DTP_SCHEMA, "secret", SDF_AUTHZ_X, 0, 0, cmd_secret, "guarded command"),
SDATA_END()
};

PRIVATE sdata_desc_t attrs_table[] = {
SDATA(DTP_BOOLEAN, "enable_command_authz", SDF_RD, "0", "per-command authz gate"),
SDATA_END()
};

PRIVATE const GMETHODS gmt = {0};

PRIVATE int register_authztest(void)
{
    ev_action_t st_idle[] = {
        {0, 0, 0}
    };
    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };
    event_type_t event_types[] = {
        {0, 0}
    };
    hgclass gc = gclass_create(
        C_AUTHZTEST,
        event_types,
        states,
        &gmt,
        0,              // lmt
        attrs_table,
        0,              // priv_size
        0,              // authz_table
        command_table,
        0,              // trace_level
        0               // gclass_flag
    );
    return gc ? 0 : -1;
}

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE void check_int(const char *name, int got, int expected)
{
    if(got != expected) {
        printf("FAIL %-32s got %d expected %d\n", name, got, expected);
        global_result += -1;
    } else {
        printf("ok   %-32s (%d)\n", name, got);
    }
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

    unsigned long memory_check_list[] = {0};
    set_memory_check_list(memory_check_list);

    gobj_start_up(
        argc, argv,
        NULL,                   // jn_global_settings
        NULL,                   // persistent_attrs
        command_parser,         // global_command_parser  <-- function under test
        NULL,                   // global_stats_parser
        test_authz_checker,     // global_authz_checker
        NULL                    // global_authentication_parser
    );
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    if(register_authztest() != 0) {
        printf("%s: FAIL (gclass_create)\n", APP);
        gobj_end();
        return -1;
    }

    hgobj yuno = gobj_create_yuno("authztest_yuno", C_AUTHZTEST, 0);
    hgobj src  = gobj_create("ext_src", C_AUTHZTEST, 0, yuno);   /* external principal */
    if(!yuno || !src) {
        printf("%s: FAIL (gobj_create)\n", APP);
        gobj_end();
        return -1;
    }

    json_t *r;

    /* Case 1: attr OFF (default) -> command runs even with g_allow=FALSE */
    g_allow = FALSE; g_executed = 0;
    r = gobj_command(yuno, "secret", json_object(), src);
    check_int("default-off runs", g_executed, 1);
    JSON_DECREF(r)

    /* Turn the gate ON */
    gobj_write_bool_attr(yuno, "enable_command_authz", TRUE);

    /* Case 2: ON + no permission + external src -> denied */
    g_allow = FALSE; g_executed = 0;
    r = gobj_command(yuno, "secret", json_object(), src);
    check_int("deny not executed", g_executed, 0);
    check_int("deny result -403", (int)kw_get_int(0, r, "result", 0, 0), -403);
    JSON_DECREF(r)

    /* Case 3: ON + no permission + self src (src==gobj) -> bypass */
    g_allow = FALSE; g_executed = 0;
    r = gobj_command(yuno, "secret", json_object(), yuno);
    check_int("self-bypass runs", g_executed, 1);
    JSON_DECREF(r)

    /* Case 4: ON + permission granted -> runs */
    g_allow = TRUE; g_executed = 0;
    r = gobj_command(yuno, "secret", json_object(), src);
    check_int("allow runs", g_executed, 1);
    JSON_DECREF(r)

    gobj_destroy(src);

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    gobj_end();
    return global_result;
}
