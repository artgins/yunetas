/****************************************************************************
 *          test_command_secret_positional.c
 *
 *          A required SDF_SECRET parameter can be given without its key
 *          (positionally), and a command line is split by blanks: a secret
 *          written with blanks spills into the parameters after it. When it
 *          is the LAST required parameter, what spills is extra text, which
 *          the parser takes as the rest of the secret and never shows. When
 *          another required parameter follows it, a piece of the secret
 *          becomes that parameter's value (shown in the command traces and
 *          in its errors) and the rest is echoed in the "extra parameters"
 *          answer. So gclass_create() refuses a command table where a
 *          required secret -- SDF_SECRET or a secret's name, as the parser
 *          tells them -- is followed by another required parameter, with an
 *          ERROR. Up to 7.25.21 such a table was accepted.
 *
 *          Cases:
 *            1. a required secret followed by a required parameter: refused
 *            2. the same with a secret by its name (`password`, no flag):
 *               refused
 *            3. a required secret as the last required parameter, optional
 *               ones after it: accepted
 *            4. a required parameter, then a required secret: accepted
 *            5. a secret that is not required (key=value only) before a
 *               required parameter: accepted
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <yunetas.h>

#define APP "test_command_secret_positional"

/***************************************************************
 *              Data
 ***************************************************************/
PRIVATE int global_result = 0;
PRIVATE int errors_refused = 0;     // the ERROR of a refused table

GOBJ_DEFINE_GCLASS(C_SECRET_POS_1);
GOBJ_DEFINE_GCLASS(C_SECRET_POS_2);
GOBJ_DEFINE_GCLASS(C_SECRET_POS_3);
GOBJ_DEFINE_GCLASS(C_SECRET_POS_4);
GOBJ_DEFINE_GCLASS(C_SECRET_POS_5);

PRIVATE json_t *cmd_probe(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return build_command_response(gobj, 0, json_sprintf("ok"), 0, 0);
}

/*-PM----type-----------name------------flag------------------------default-description--*/
PRIVATE sdata_desc_t pm_secret_then_required[] = {
SDATAPM (DTP_STRING,    "token",        SDF_REQUIRED|SDF_SECRET,    0,      "A secret"),
SDATAPM (DTP_STRING,    "user",         SDF_REQUIRED,               0,      "A user"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_named_secret_then_required[] = {
SDATAPM (DTP_STRING,    "password",     SDF_REQUIRED,               0,      "A secret by its name"),
SDATAPM (DTP_STRING,    "user",         SDF_REQUIRED,               0,      "A user"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_secret_last_required[] = {
SDATAPM (DTP_STRING,    "user",         SDF_REQUIRED,               0,      "A user"),
SDATAPM (DTP_STRING,    "token",        SDF_REQUIRED|SDF_SECRET,    0,      "A secret"),
SDATAPM (DTP_STRING,    "note",         0,                          "",     "Optional"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_secret_only_required[] = {
SDATAPM (DTP_STRING,    "token",        SDF_REQUIRED|SDF_SECRET,    0,      "A secret"),
SDATAPM (DTP_STRING,    "note",         0,                          "",     "Optional"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_optional_secret_first[] = {
SDATAPM (DTP_STRING,    "token",        SDF_SECRET,                 0,      "A secret, key=value only"),
SDATAPM (DTP_STRING,    "note",         0,                          "",     "Optional"),
SDATA_END()
};

/*-CMD---type-----------name------------flag----alias---items-------------------------json_fn-----description--*/
PRIVATE sdata_desc_t cmds_1[] = {
SDATACM2(DTP_SCHEMA,    "ok",           0,      0,      pm_secret_last_required,        cmd_probe,  "Fine"),
SDATACM2(DTP_SCHEMA,    "login",        0,      0,      pm_secret_then_required,        cmd_probe,  "Refused"),
SDATA_END()
};
PRIVATE sdata_desc_t cmds_2[] = {
SDATACM2(DTP_SCHEMA,    "login",        0,      0,      pm_named_secret_then_required,  cmd_probe,  "Refused"),
SDATA_END()
};
PRIVATE sdata_desc_t cmds_3[] = {
SDATACM2(DTP_SCHEMA,    "login",        0,      0,      pm_secret_only_required,        cmd_probe,  "Fine"),
SDATA_END()
};
PRIVATE sdata_desc_t cmds_4[] = {
SDATACM2(DTP_SCHEMA,    "login",        0,      0,      pm_secret_last_required,        cmd_probe,  "Fine"),
SDATA_END()
};
PRIVATE sdata_desc_t cmds_5[] = {
SDATACM2(DTP_SCHEMA,    "login",        0,      0,      pm_optional_secret_first,       cmd_probe,  "Fine"),
SDATA_END()
};

PRIVATE sdata_desc_t attrs_table[] = {
SDATA_END()
};

PRIVATE const GMETHODS gmt = {0};

PRIVATE hgclass create(gclass_name_t name, const sdata_desc_t *cmds)
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
    return gclass_create(
        name,
        event_types,
        states,
        &gmt,
        0,              // lmt
        attrs_table,
        0,              // priv_size
        0,              // authz_table
        cmds,
        0,              // trace_level
        0               // gclass_flag
    );
}

/***************************************************************
 *              Helpers
 ***************************************************************/
PRIVATE int capture_write(void *h, int priority, const char *bf, size_t len)
{
    if(priority <= LOG_ERR && strstr(bf, "A required secret parameter must be the last required one")) {
        errors_refused++;
    }
    return 0;
}

PRIVATE void check(BOOL ok, const char *name)
{
    printf("%s %s\n", ok? "ok  " : "FAIL", name);
    if(!ok) {
        global_result += -1;
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
        command_parser,         // global_command_parser
        NULL,                   // global_stats_parser
        NULL,                   // global_authz_checker
        NULL                    // global_authentication_parser
    );
    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);
    gobj_log_register_handler("capture", 0, capture_write, 0);
    gobj_log_add_handler("capture", "capture", LOG_OPT_ALL, 0);

    errors_refused = 0;
    check(create(C_SECRET_POS_1, cmds_1) == NULL, "1. a required secret followed by a required parameter: refused");
    check(errors_refused == 1, "1. ... with one ERROR");

    errors_refused = 0;
    check(create(C_SECRET_POS_2, cmds_2) == NULL, "2. the same, a secret by its name: refused");
    check(errors_refused == 1, "2. ... with one ERROR");

    errors_refused = 0;
    check(create(C_SECRET_POS_3, cmds_3) != NULL, "3. a required secret, the only required one: accepted");
    check(create(C_SECRET_POS_4, cmds_4) != NULL, "4. a required parameter, then a required secret: accepted");
    check(create(C_SECRET_POS_5, cmds_5) != NULL, "5. a secret not required, first: accepted");
    check(errors_refused == 0, "3-5. ... with no ERROR");

    gobj_log_del_handler("capture");
    gobj_end();

    size_t leaked = get_cur_system_memory();
    check(leaked == 0, "no memory leak");

    printf("\n%s: %s\n", APP, global_result == 0 ? "PASS" : "FAIL");
    return global_result;
}
