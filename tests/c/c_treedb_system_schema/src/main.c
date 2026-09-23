/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_treedb_system_schema
 *          Tests the __system__ meta-treedb of C_TREEDB: a treedb schema
 *          projected as data, and rebuilt back from it
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_test_system_schema.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_treedb_system_schema"
#define APP_DOC         "Test the __system__ meta-treedb of C_TREEDB"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define DEBUG_MEMORY            FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '"APP_NAME"',                                  \n\
        'tags': ['test', 'yunetas']                                 \n\
    }                                                               \n\
}                                                                   \n\
";
PRIVATE char variable_config[]= "\
{                                                                   \n\
    'environment': {                                                \n\
        'console_log_handlers': {                                   \n\
        },                                                          \n\
        'daemon_log_handlers': {                                    \n\
        }                                                           \n\
    },                                                              \n\
    'yuno': {                                                       \n\
        'autoplay': true,                                           \n\
        'required_services': [],                                    \n\
        'public_services': [],                                      \n\
        'service_descriptor': {                                     \n\
        },                                                          \n\
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'test_system_schema',                           \n\
            'gclass': 'C_TEST_SYSTEM_SCHEMA',                       \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'children': [                                           \n\
            ]                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

time_measure_t time_measure;

/***************************************************************************
 *  The test drives C_TREEDB through its commands, and those are guarded by
 *  authzs. Without an authz service the default checker denies them, so
 *  the test supplies its own: everyone may, except the one principal the
 *  refusal of `mt_treedbs` is asked about.
 ***************************************************************************/
#define DENIED_USER     "denied@test"
#define WRITER_USER     "writer@test"   /*  everything but create-delete  */

PRIVATE BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    const char *username = kw_get_str(gobj, kw, "__username__", "", 0);
    BOOL allowed = (strcmp(username, DENIED_USER) != 0)? TRUE: FALSE;
    if(strcmp(username, WRITER_USER)==0 && strcmp(authz, "create-delete")==0) {
        allowed = FALSE;
    }
    KW_DECREF(kw)
    return allowed;
}

/*
 *  Every log line from INFO up, in order: one per emission (strict FIFO).
 */
PRIVATE const char *expected_log_msgs[] = {
    /*  start up  */
    "Starting yuno",
    /*  __system__ tranger + schema + its six topics  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Playing yuno",
    "Creating __timeranger2__.json",
    /*  client tranger + schema + __snaps__ __graphs__ users departments fidelity __assets__  */
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    /*  Test 2: schema file re-created from the literal on re-open  */
    "Creating TreeDB schema file",
    /*  Test 3: a literal ahead: projection, schema file, the one topic that moved  */
    "Updating TreeDB schema in __system__",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  Test 4: a literal newer than the FILE takes over a save never
     *  applied (projected over it, said), then one ahead  */
    "Schema from C is newer than the file in use but not than __system__: it takes over the file, and replaces in __system__ the drafts of the topics it raises past the file",
    "Updating TreeDB schema in __system__",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Updating TreeDB schema in __system__",
    "Re-Creating TreeDB schema file",
    "Updating TreeDB schema in __system__",
    "Topic from C differs from __system__ but does not raise its topic_version past the file in use: not applied, the topic runs from the file",
    "Re-Creating TreeDB schema file",
    /*  Test 5: the refused writes  */
    "Value not in enum",
    "Value not in enum",
    "a 'file' column must be of type 'string': one file per column",
    "Column definition refused",
    "A column cannot be both 'hook' and 'fkey'",
    "Column definition refused",
    "A hook column must be of type dict or list",
    "Column definition refused",
    "A hook column must be of type dict or list",
    "Column definition refused",
    "Schema topic pkey must be 'id'",
    "Node already exists",
    "Topic already has a column with this name",
    /*  Test 8: legacy ids move; the treedb opens (+ __assets__)  */
    "TreeDB schema ids moved to qualified names",
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    /*  ... its draft column saved, applied, and in the treedb  */
    "Schema saved",
    "Schema applied",
    "TreeDB schema from C is behind the schema in use, not applied",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  Test 9: impose_c_schema on: imposed over a newer file  */
    "Opening TreeDB with the schema from C, __system__ not read",
    "TreeDB schema from C is behind the schema in use, not applied",
    "Imposing TreeDB schema from C over a newer one",
    "Re-Creating TreeDB schema file",
    "Imposing topic_version from C over a newer one",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  Test 9b: impose on, the treedb in dynamic_schema_treedbs opens from its file  */
    "TreeDB schema from C is behind the schema in use, not applied",
    /*  Test 10 opens it first  */
    "TreeDB schema from C is behind the schema in use, not applied",
    /*  Test 10: the disk taken ahead (save + apply), then the code imposes  */
    "Schema saved",
    "Schema applied",
    "TreeDB schema from C is behind the schema in use, not applied",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "TreeDB schema from C is behind the schema in use, not applied",
    "Imposing TreeDB schema from C over a newer one",
    "Re-Creating TreeDB schema file",
    "Imposing topic_version from C over a newer one",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  Test 11: gobj_treedbs refused  */
    "No permission to list the treedbs",
    /*  Test 12: the treedb opened to be deleted  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    /*  Test 13: imposing seeds, re-makes one behind, leaves one ahead  */
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "Updating TreeDB schema in __system__",
    "Re-Creating TreeDB schema file",
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "TreeDB schema from C is behind the schema in use, not applied",
    "Imposing TreeDB schema from C over a newer one",
    "Re-Creating TreeDB schema file",
    /*  Test 13b: save-schema (twice, then once over a dict file), apply
     *  refused while imposed, applied, the file decides  */
    "TreeDB schema from C is behind the schema in use, not applied",
    "Schema saved",
    "Schema saved",
    "Schema saved",
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "TreeDB schema from C is behind the schema in use, not applied",
    "TreeDB schema from C is behind the schema in use, not applied",
    "Schema applied",
    "TreeDB schema from C is behind the schema in use, not applied",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  Test 13b2: a saved draft taken back is withdrawn by the next save  */
    "TreeDB schema from C is behind the schema in use, not applied",
    "Schema saved",
    "Saved schema withdrawn, the draft is the schema in use",
    /*  Test 13b2b: the same schema with its cols listed is not "another
     *  content" (only the ordinary "behind" of __system__)  */
    "TreeDB schema from C is behind the schema in use, not applied",
    /*  Test 13b3a: an operator save of `users`, then a literal newer than
     *  __system__ raising `users` past the file: projected over the draft  */
    "TreeDB schema from C is behind the schema in use, not applied",
    "Schema saved",
    "Updating TreeDB schema in __system__",
    "Topic from C raised past the file in use replaces its saved draft in __system__",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  Test 13b3: a literal taking over the file re-projects only the topic
     *  it raised; then, with NO file in use, the whole literal  */
    "TreeDB schema from C is behind the schema in use, not applied",
    "Schema saved",
    "Schema from C is newer than the file in use but not than __system__: it takes over the file, and replaces in __system__ the drafts of the topics it raises past the file",
    "Updating TreeDB schema in __system__",
    "Topic from C differs from __system__ but does not raise its topic_version past the file in use: not applied, the topic runs from the file",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Schema saved",
    "No schema file in use: the treedb opens with the schema from C, projected whole over __system__",
    "Updating TreeDB schema in __system__",
    "Creating TreeDB schema file",
    /*  Test 13c: apply of all is all or none: A saved, B opened (+ its
     *  three), B's saved schema does not parse, then A alone applied  */
    "TreeDB schema from C is behind the schema in use, not applied",
    "Schema saved",
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "wrong type for list",
    "Data must be an {} or [{}]",
    "wrong type for list",
    "topic without cols",
    "Schema applied",
    /*  Test 13d: every command refuses `denied`, which logs nothing  */
    /*  Test 14: the replica, and its C_TREEDB writes READ-ONLY  */
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    /*  end  */
    "All treedb system schema tests PASSED",
    "Exit to die",
    "Exit to die",
    "Pausing yuno",
    "Yuno stopped, gobj end",
    NULL
};

PRIVATE json_t *expected_log_list(void)
{
    json_t *list = json_array();
    for(int i = 0; expected_log_msgs[i]; i++) {
        json_array_append_new(list, json_pack("{s:s}", "msg", expected_log_msgs[i]));
    }
    return list;
}

/***************************************************************************
 *  HACK This function is executed on yunetas environment (mem, log, paths)
 *  BEFORE creating the yuno
 ***************************************************************************/
int result = 0;

static int register_yuno_and_more(void)
{
    int result = 0;

    /*--------------------------------------*
     *  A realm for the persistent attrs,
     *  inside the directory the test wipes
     *  when it starts (see its mt_create)
     *--------------------------------------*/
    char root_dir[PATH_MAX];
    build_path(root_dir, sizeof(root_dir), getenv("HOME"), "tests_yuneta", NULL);
    register_yuneta_environment(root_dir, "c_treedb_system_schema", 02770, 0660);

    /*--------------------*
     *  Register gclass
     *--------------------*/
    result += register_c_test_system_schema();

    /*------------------------------*
     *  Start test
     *------------------------------*/
    set_expected_results(
        APP_NAME,
        expected_log_list(),
        NULL,   // expected
        NULL,   // ignore_keys
        1       // verbose
    );

    MT_START_TIME(time_measure)

    return result;
}

/***************************************************************************
 *  HACK This function is executed on yunetas environment
 *  AFTER the yuno has stopped
 ***************************************************************************/
static void cleaning(void)
{
    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, APP_NAME)

    result += test_json(NULL);
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------*
     *  Captura salida logger
     *------------------------------*/
    glog_init();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_INFO, 0);

    /*------------------------------------------------*
     *      To check memory loss
     *------------------------------------------------*/
    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

    /*------------------------------------------------*
     *          Start yuneta
     *------------------------------------------------*/
    helper_quote2doublequote(fixed_config);
    helper_quote2doublequote(variable_config);
    yuneta_setup(
        NULL,       // persistent_attrs
        NULL,       // command_parser
        NULL,       // stats_parser
        test_authz_checker,
        NULL,       // authentication_parser
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY,
        USE_OWN_SYSTEM_MEMORY,
        MEM_MIN_BLOCK,
        MEM_SUPERBLOCK
    );

    result += yuneta_entry_point(
        argc, argv,
        APP_NAME, APP_VERSION, APP_SUPPORT, APP_DOC, APP_DATETIME,
        fixed_config,
        variable_config,
        register_yuno_and_more,
        cleaning
    );

    if(get_cur_system_memory()!=0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    if(result<0) {
        printf("<-- %sTEST FAILED%s: %s\n", On_Red BWhite, Color_Off, APP_NAME);
    }
    return result<0?-1:0;
}
