/****************************************************************************
 *          test_command_binary_kw.c
 *
 *          Regression test: a command whose kw carries a binary field (a
 *          `gbuffer`) keeps the gbuffer's reference count right.
 *
 *          command_parser() builds a new kw for the handler (kw_cmd) and
 *          copies into it the keys of the caller's kw. Both kws are released
 *          with KW_DECREF, and KW_DECREF releases the gbuffer of each one. So
 *          the copy of a binary field must take a reference of its own. Up to
 *          7.25.4 the copy was a plain json_object_update_missing(), with no
 *          reference: the gbuffer was released once too often ("BAD
 *          gbuf_decref()", or a gbuffer freed while its owner still held it).
 *
 *          The three paths of build_cmd_kw() that copy the caller's kw:
 *            1. a command without a parameter schema
 *            2. a command with a parameter schema
 *            3. a command with a parameter schema and SDF_WILD_CMD
 *
 *          For each one, the test holds two references of its own and gives
 *          the kw a third one. The handler must see four (its copy has one),
 *          and after the command the test must hold its two again.
 *
 *          The same for build_stats() (the default stats parser): it hands
 *          the kw to the stats builder of the gobj and of each bottom gobj,
 *          and each one releases it with KW_DECREF. Up to 7.25.4 it gave them
 *          the kw with json_incref(), which does not take a reference of the
 *          gbuffer, so the gbuffer was released once per builder.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP "test_command_binary_kw"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int global_result = 0;
PRIVATE gbuffer_t *g_gbuf = 0;          /* the gbuffer under test */
PRIVATE int g_seen_refcount = -1;       /* refcount the handler saw */
PRIVATE BOOL g_seen_same = FALSE;       /* handler got the same gbuffer */

GOBJ_DEFINE_GCLASS(C_BINARYKWTEST);

/***************************************************************
 *              Command handler
 ***************************************************************/
PRIVATE json_t *cmd_probe(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    g_seen_same = (gbuf == g_gbuf)?TRUE:FALSE;
    g_seen_refcount = gbuf? (int)gbuf->refcount : -1;

    KW_DECREF(kw)
    return build_command_response(gobj, 0, json_sprintf("ok"), 0, 0);
}

/***************************************************************
 *              GClass scaffolding
 ***************************************************************/
PRIVATE sdata_desc_t pm_one[] = {
/*-PM----type-----------name------------flag--------default-----description---------- */
SDATAPM (DTP_STRING,    "name",         0,          "",         "A name"),
SDATA_END()
};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name------------flag----------alias---items---json_fn-----description--*/
SDATACM2(DTP_SCHEMA,    "noschema",     0,            0,      0,      cmd_probe,  "No parameter schema"),
SDATACM2(DTP_SCHEMA,    "withschema",   0,            0,      pm_one, cmd_probe,  "A parameter schema"),
SDATACM2(DTP_SCHEMA,    "wild",         SDF_WILD_CMD, 0,      pm_one, cmd_probe,  "A wild command"),
SDATA_END()
};

PRIVATE sdata_desc_t attrs_table[] = {
SDATA_END()
};

PRIVATE const GMETHODS gmt = {0};

PRIVATE int register_binarykwtest(void)
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
        C_BINARYKWTEST,
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
        printf("FAIL %-52s got %d expected %d\n", name, got, expected);
        global_result += -1;
    } else {
        printf("ok   %-52s (%d)\n", name, got);
    }
}

/***************************************************************************
 *  Run one command with a kw that carries a gbuffer
 ***************************************************************************/
PRIVATE void run_case(hgobj yuno, const char *command)
{
    char label[80];

    g_gbuf = gbuffer_create(16, 16);
    gbuffer_incref(g_gbuf);         /* the test holds two references */

    json_t *kw = json_object();
    gbuffer_incref(g_gbuf);         /* the kw holds the third one */
    json_object_set_new(kw, "gbuffer", json_integer((json_int_t)(uintptr_t)g_gbuf));

    g_seen_refcount = -1;
    g_seen_same = FALSE;

    json_t *r = gobj_command(yuno, command, kw, yuno);

    snprintf(label, sizeof(label), "%s: answers result=0", command);
    check_int(label, (int)kw_get_int(0, r, "result", -999, 0), 0);
    JSON_DECREF(r)

    snprintf(label, sizeof(label), "%s: the handler gets the gbuffer", command);
    check_int(label, g_seen_same?1:0, 1);

    snprintf(label, sizeof(label), "%s: the handler's copy holds a reference", command);
    check_int(label, g_seen_refcount, 4);

    snprintf(label, sizeof(label), "%s: the test holds its two references", command);
    check_int(label, (int)g_gbuf->refcount, 2);

    /*
     *  Release what the test holds; guard against a count already wrong,
     *  so a failing run reports and does not free the gbuffer twice.
     */
    int refs = (int)g_gbuf->refcount;
    while(refs > 0) {
        refs--;
        gbuffer_decref(g_gbuf);
    }
    g_gbuf = 0;
}

/***************************************************************************
 *  build_stats() with a kw that carries a gbuffer
 ***************************************************************************/
PRIVATE void run_stats_case(hgobj yuno)
{
    g_gbuf = gbuffer_create(16, 16);
    gbuffer_incref(g_gbuf);         /* the test holds two references */

    json_t *kw = json_object();
    gbuffer_incref(g_gbuf);         /* the kw holds the third one */
    json_object_set_new(kw, "gbuffer", json_integer((json_int_t)(uintptr_t)g_gbuf));

    json_t *jn_data = build_stats(yuno, "", kw, yuno);
    check_int("build_stats: answers a dict", json_is_object(jn_data)?1:0, 1);
    JSON_DECREF(jn_data)

    check_int("build_stats: the test holds its two references", (int)g_gbuf->refcount, 2);

    int refs = (int)g_gbuf->refcount;
    while(refs > 0) {
        refs--;
        gbuffer_decref(g_gbuf);
    }
    g_gbuf = 0;
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
        NULL,                   // global_authz_checker
        NULL                    // global_authentication_parser
    );
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    if(register_binarykwtest() != 0) {
        printf("%s: FAIL (gclass_create)\n", APP);
        gobj_end();
        return -1;
    }

    hgobj yuno = gobj_create_yuno("binarykwtest_yuno", C_BINARYKWTEST, 0);
    if(!yuno) {
        printf("%s: FAIL (gobj_create_yuno)\n", APP);
        gobj_end();
        return -1;
    }

    run_case(yuno, "noschema");
    run_case(yuno, "withschema");
    run_case(yuno, "wild");
    run_stats_case(yuno);

    gobj_end();

    size_t leaked = get_cur_system_memory();
    check_int("no memory leak", (int)leaked, 0);

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result;
}
