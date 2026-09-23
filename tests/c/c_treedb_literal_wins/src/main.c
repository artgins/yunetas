/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_treedb_literal_wins
 *          A schema from C newer than the schema file in use wins WHOLE:
 *          the file, what runs and __system__ agree, and what it withdraws
 *          of the operator's work is said
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_test_literal_wins.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_treedb_literal_wins"
#define APP_DOC         "Test that a newer literal wins whole in C_TREEDB"

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
            'name': 'test_literal_wins',                            \n\
            'gclass': 'C_TEST_LITERAL_WINS',                        \n\
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
 *  the test supplies its own: everyone may.
 ***************************************************************************/
PRIVATE BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return TRUE;
}

/*
 *  What an open says when the literal wins whole
 */
#define M_REMOVED       "Topic not declared by the schema from C: removed from __system__"
#define M_WITHDREW      "Schema from C withdrew work on the schema at open"
#define M_NOT_RAISED    "Topic from C declares other columns than the store runs, without raising its topic_version past it: the store keeps running its own"
#define M_SNAP_HOLDS    "cannot delete node, a snapshot still holds it"
#define M_IN_PART       "Schema projected into __system__ only in part: its version is not recorded, and every open of the treedb retries it"
#define M_COMPLETING    "Completing the projection into __system__, left unfinished by an earlier open"
#define M_TIE           "Schema from C has the schema_version of the dynamic schema in use but another content: NOT applied, raise its schema_version to publish it"
#define M_BEHIND        "TreeDB schema from C is behind the schema in use, not applied"
#define M_ENUM          "Value not in enum"
#define M_REPLICA       "The store of the treedb is not written here: it opens as a replica and runs its schema file, __system__ is not reconciled"
#define M_NO_TOPICS_SAVE "Draft of a treedb schema with no topics: not saved, a treedb without topics does not open"
#define M_NO_TOPICS_APPLY "Saved treedb schema with no topics: not applied, a treedb without topics does not open"
#define M_UNREADABLE    "Record of an unfinished projection cannot be read: the projection is unfinished, what it left is unknown; every open retries it, save-schema refuses, and what __system__ holds over the file is taken for drafts"

/*
 *  Every log line from INFO up, in order: one per emission (strict FIFO).
 */
PRIVATE const char *expected_log_msgs[] = {
    /*  start up: __system__ tranger + schema + its six topics  */
    "Starting yuno",
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Playing yuno",

    /*  RM: v1 (users, departments), then v2 without departments, twice  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    M_REMOVED,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",

    /*  TIE+HOOK: v1, users applied, v3 raises users with an fkey and
     *  adds `groups`: the apply is withdrawn, said once  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Schema saved",
    "Schema applied",
    "Updating TreeDB schema in __system__",
    M_WITHDREW,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Creating topic",

    /*  EQ: v1, users applied, v3 twice: the second open says nothing  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Schema saved",
    "Schema applied",
    "Updating TreeDB schema in __system__",
    M_WITHDREW,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",

    /*  SAVED: a saved draft and an unsaved one, withdrawn by v2  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Schema saved",
    "Updating TreeDB schema in __system__",
    M_WITHDREW,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",

    /*  OLD: users applied; a literal of the file's version with another
     *  content (the apply runs), then one behind the file  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Schema saved",
    "Schema applied",
    M_TIE,
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    M_BEHIND,

    /*  REN: departments renamed to sections, the hook and fkey with it  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    M_REMOVED,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Creating topic",

    /*  NR: users changed without its topic_version  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    M_NOT_RAISED,
    "Re-Creating TreeDB schema file",

    /*  IMP: imposed from the code, v2 removes departments (the tranger of
     *  the treedb is created before the schema is decided)  */
    "Creating __timeranger2__.json",
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "Updating TreeDB schema in __system__",
    M_REMOVED,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",

    /*  L-2: departments' store directory removed, v2 changes it: nothing
     *  withdrawn, nothing said; the topic is created again  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    "Re-Creating TreeDB schema file",
    "Creating topic",

    /*  SNAP: a snapshot of __system__ holds departments; v2 removes it.
     *  Twice the delete is refused (the fkey column and the topic), the
     *  projection says it is partial, the second open retries it; once
     *  the snapshot is deleted the third open completes it  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    M_SNAP_HOLDS,
    M_SNAP_HOLDS,
    M_IN_PART,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    M_COMPLETING,
    M_SNAP_HOLDS,
    M_SNAP_HOLDS,
    M_IN_PART,
    M_COMPLETING,
    M_REMOVED,

    /*  TWICE: users saved; the second open is refused, and says nothing  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Schema saved",

    /*  RAN: users applied, run by the next open (v1, behind), then v3
     *  withdraws the running dynamic schema, said once  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Schema saved",
    "Schema applied",
    M_BEHIND,
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Updating TreeDB schema in __system__",
    M_WITHDREW,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",

    /*  SEED: delete-treedb, then a literal of the dynamic file's version
     *  with another content: the tie is said at both opens  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Schema saved",
    "Schema applied",
    M_TIE,
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    M_TIE,

    /*  LOCK: the store is held: a replica, __system__ not reconciled;
     *  then free: the literal is installed and projected  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Cannot open an exclusive json file",
    "Open as not master, __timeranger2__.json locked",
    M_REPLICA,
    "Updating TreeDB schema in __system__",
    M_REMOVED,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",

    /*  REC: the record of the apply cannot be written: refused; then
     *  it can  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Schema saved",
    "Cannot create json file",
    "Schema applied",

    /*  M1: a snapshot holds departments; v2 leaves it (unfinished), the
     *  operator adds users.email; the retry replaces it and SAYS so; a
     *  column added to departments; once the snapshot is gone the
     *  projection completes and says departments  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    M_SNAP_HOLDS,
    M_IN_PART,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    M_COMPLETING,
    M_SNAP_HOLDS,
    M_IN_PART,
    M_WITHDREW,
    M_COMPLETING,
    M_SNAP_HOLDS,
    M_IN_PART,
    M_COMPLETING,
    M_REMOVED,
    M_WITHDREW,

    /*  M2 imposed: v1 with groups, a snapshot, v2 without (unfinished),
     *  snapshot gone, v2 again: completed, groups not reported  */
    "Creating __timeranger2__.json",
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "Updating TreeDB schema in __system__",
    M_SNAP_HOLDS,
    M_IN_PART,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "Updating TreeDB schema in __system__",
    M_REMOVED,

    /*  M2 dynamic: the same, then a NEWER literal v3 once the snapshot is
     *  gone: groups goes, not reported  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    M_SNAP_HOLDS,
    M_IN_PART,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Updating TreeDB schema in __system__",
    M_REMOVED,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",

    /*  L1: users applied, delete-treedb, seeded from the file (behind),
     *  drafts, then the file itself as the literal: nothing projected  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Schema saved",
    "Schema applied",
    M_BEHIND,
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",

    /*  L3: users applied and run, a crashed record, departments applied,
     *  v4: users "in_use", departments "applied"  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Schema saved",
    "Schema applied",
    M_BEHIND,
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    M_BEHIND,
    "Schema saved",
    "Schema applied",
    "Updating TreeDB schema in __system__",
    M_WITHDREW,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",

    /*  L4: delete-treedb, a file whose column has a flag the meta-schema
     *  refuses (the store refuses the topic too): the seed fails, is
     *  recorded, retried at the next open; the file fixed, completed  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    M_ENUM,
    M_IN_PART,
    M_BEHIND,
    "Wrong enum type",
    "Topic refused: bad columns",
    "Cannot create topic of the schema",
    M_COMPLETING,
    M_ENUM,
    M_IN_PART,
    M_BEHIND,
    "Wrong enum type",
    "Topic refused: bad columns",
    "Cannot create topic of the schema",
    M_COMPLETING,
    M_BEHIND,
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",

    /*  L6: a literal refused by C_TREEDB (hook and fkey), then a schema
     *  file with no topics: treedb_open_db() fails, the open answers -1
     *  (and nothing else: C_NODE neither sets the callback of a treedb
     *  that never opened nor closes it)  */
    "Creating __timeranger2__.json",
    "A column cannot be both 'hook' and 'fkey'",
    "Input Schema fails",
    "A column cannot be both 'hook' and 'fkey'",
    "Schema fails",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "No topics found",

    /*  M1b: a snapshot holds departments, v2 leaves it (unfinished), the
     *  operator adds departments.budget; a retry that cannot finish keeps
     *  it a draft and says nothing; the open that removes it says it  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    M_SNAP_HOLDS,
    M_IN_PART,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    M_COMPLETING,
    M_SNAP_HOLDS,
    M_IN_PART,
    M_COMPLETING,
    M_REMOVED,
    M_WITHDREW,

    /*  M2b on a removed topic (tw_m2r): the draft on departments stays a
     *  draft through two refused deletes, and is said once, by the open
     *  that removes it  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    M_SNAP_HOLDS,
    M_IN_PART,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    M_COMPLETING,
    M_SNAP_HOLDS,
    M_IN_PART,
    M_COMPLETING,
    M_REMOVED,
    M_WITHDREW,

    /*  M2b on a kept topic (tw_m2c): users.budget and departments are
     *  both refused; users is said once, by the open that removes it  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    M_SNAP_HOLDS,
    M_SNAP_HOLDS,
    M_IN_PART,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    M_COMPLETING,
    M_SNAP_HOLDS,
    M_SNAP_HOLDS,
    M_IN_PART,
    M_COMPLETING,
    M_REMOVED,
    M_WITHDREW,

    /*  L1b: the record is torn: every read of it says so (saved-schema
     *  reads it twice, save-schema and the next open once); the retry
     *  writes it again; with no leftovers known, the open that completes
     *  it reports departments  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    M_SNAP_HOLDS,
    M_IN_PART,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    M_UNREADABLE,
    M_UNREADABLE,
    M_UNREADABLE,
    M_UNREADABLE,
    M_COMPLETING,
    M_SNAP_HOLDS,
    M_IN_PART,
    M_COMPLETING,
    M_REMOVED,
    M_WITHDREW,

    /*  L2b: first open, projected whole  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",

    /*  L3b + L4b: a schema file with no topics: the open fails, the
     *  second one is refused up front, delete-treedb is refused, and
     *  close-treedb logs nothing  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "No topics found",

    /*  N7 unsaved (tw_n7u): the operator deletes departments; a newer
     *  literal re-creates it and says it  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    M_WITHDREW,
    "Re-Creating TreeDB schema file",

    /*  N7 saved (tw_n7s): the same, with the deletion saved  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Schema saved",
    "Updating TreeDB schema in __system__",
    M_WITHDREW,
    "Re-Creating TreeDB schema file",

    /*  N1: delete-treedb, a seed that died (node 0/0): the next open
     *  completes it; save-schema and apply-schema refuse no topics  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    M_COMPLETING,
    M_NO_TOPICS_SAVE,
    M_NO_TOPICS_APPLY,

    /*  N4: a saved draft on departments, the delete refused: the first
     *  open says the saved schema; the one that completes says the
     *  draft, "saved"  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Schema saved",
    "Updating TreeDB schema in __system__",
    M_SNAP_HOLDS,
    M_IN_PART,
    M_WITHDREW,
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    M_COMPLETING,
    M_REMOVED,
    M_WITHDREW,

    /*  end  */
    "All treedb literal wins tests PASSED",
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
    register_yuneta_environment(root_dir, "c_treedb_literal_wins", 02770, 0660);

    /*--------------------*
     *  Register gclass
     *--------------------*/
    result += register_c_test_literal_wins();

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
