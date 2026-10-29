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
 *  An entry that starts with AS_WARNING must be logged as a WARNING, one
 *  that starts with AS_ERROR as an ERROR.
 */
#define AS_WARNING  "<WARNING>"
#define AS_ERROR    "<ERROR>"

PRIVATE const char *expected_log_msgs[] = {
    /*  start up  */
    "Starting yuno",
    /*  __system__ tranger + schema + its seven topics  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Playing yuno",
    "Creating __timeranger2__.json",
    /*  client tranger + schema + __snaps__ __graphs__ users departments fidelity __assets__ __icons__  */
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    /*  Test 2: schema file re-created from the literal on re-open  */
    "Creating TreeDB schema file",
    /*  Test 3: a literal ahead: projection (whole: `fidelity`, which it
     *  no longer declares, goes), schema file, the one topic that moved  */
    "Updating TreeDB schema in __system__",
    "Topic not declared by the schema from C: removed from __system__",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  Test 4: a literal newer than the FILE, not than __system__ (10),
     *  wins whole; then one ahead; then one that changes `users` without
     *  its topic_version: the store keeps its own, said  */
    "Updating TreeDB schema in __system__",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Updating TreeDB schema in __system__",
    "Re-Creating TreeDB schema file",
    "Updating TreeDB schema in __system__",
    "Topic from C declares other columns than the store runs, without raising its topic_version past it: the store keeps running its own",
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
    /*  Test 8: legacy ids move; the treedb opens (+ __assets__ __icons__); its
     *  tranger is created before the schema is decided  */
    "Creating __timeranger2__.json",
    "TreeDB schema ids moved to qualified names",
    "Creating TreeDB schema file",
    "Creating topic",
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
    "TreeDB schema from C is imposed, but it is behind __system__: the projection is kept",
    "Imposing TreeDB schema from C over a newer one",
    "Re-Creating TreeDB schema file",
    "Imposing topic_version from C over a newer one",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  Test 9b: impose on, the treedb in dynamic_schema_treedbs opens from
     *  its file, which is the literal Test 9 imposed: nothing said; Test 10
     *  opens it first, the same  */
    /*  Test 10: the disk taken ahead (save + apply), then the code imposes  */
    "Schema saved",
    "Schema applied",
    "TreeDB schema from C is behind the schema in use, not applied",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "TreeDB schema from C is imposed, but it is behind __system__: the projection is kept",
    /*  ...the apply RAN at the open before: imposing replaces a running
     *  dynamic schema, and says it ("in_use")  */
    "Schema from C withdrew work on the schema at open",
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
    "Creating topic",
    /*  Test 13: imposing seeds, re-makes one behind, leaves one ahead  */
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "Updating TreeDB schema in __system__",
    /*  ...`alfa` changed without its topic_version: the store keeps its own  */
    "Topic from C declares other columns than the store runs, without raising its topic_version past it: the store keeps running its own",
    "Re-Creating TreeDB schema file",
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "TreeDB schema from C is imposed, but it is behind __system__: the projection is kept",
    "Imposing TreeDB schema from C over a newer one",
    "Re-Creating TreeDB schema file",
    /*  Test 13b: save-schema (twice, then once over a dict file), apply
     *  refused while imposed, applied, the file decides. The opens with the
     *  literal Test 13 imposed on the file say nothing  */
    "Schema saved",
    "Schema saved",
    "Schema saved",
    "impose_c_schema forced by the code of the yuno, over the attribute",
    "Opening TreeDB with the schema from C, __system__ not read",
    "TreeDB schema from C is imposed, but it is behind __system__: the projection is kept",
    "Schema applied",
    "TreeDB schema from C is behind the schema in use, not applied",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  Test 13b2: a saved draft taken back is withdrawn by the next save  */
    "TreeDB schema from C is behind the schema in use, not applied",
    "Schema saved",
    "Saved schema withdrawn, the draft is the schema in use",
    /*  ...and reopened: the literal is the file in use, nothing is said  */
    /*  Test 13b2a: a column moved in the draft, saved, moved back  */
    "Schema saved",
    "Saved schema withdrawn, the draft is the schema in use",
    /*  Test 13b2b: two required draft columns saved, applied and opened;
     *  the one with no default refuses a record without it; then removed,
     *  saved and applied again  */
    "Schema saved",
    "Schema applied",
    "TreeDB schema from C is behind the schema in use, not applied",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Field required: 'required_list'",
    "Schema saved",
    "Schema applied",
    "TreeDB schema from C is behind the schema in use, not applied",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  Test 13b2b: the same schema with its cols listed is not "another
     *  content": nothing is said  */
    /*  Test 13b3a: an operator save of `users`, then a literal newer than
     *  __system__ raising `users` past the file: projected over the draft  */
    "TreeDB schema from C is behind the schema in use, not applied",
    "Schema saved",
    "Updating TreeDB schema in __system__",
    "Schema from C withdrew work on the schema at open",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  Test 13b3: a literal newer than the file, not than __system__, wins
     *  whole: the saved draft of `departments` is withdrawn, said; then,
     *  with NO file in use, the same  */
    "TreeDB schema from C is behind the schema in use, not applied",
    "Schema saved",
    "Updating TreeDB schema in __system__",
    "Schema from C withdrew work on the schema at open",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Schema saved",
    "No schema file in use: the treedb opens with the schema from C, projected whole over __system__",
    "Updating TreeDB schema in __system__",
    "Schema from C withdrew work on the schema at open",
    "Creating TreeDB schema file",
    /*  Test 13b4: an apply not opened yet is withdrawn by the literal of
     *  the next open, said. C: the literal gives `users` the apply's number
     *  (the literal runs). D: the literal raises only `departments` and
     *  carries `users` as it runs  */
    "TreeDB schema from C is behind the schema in use, not applied",
    "Schema saved",
    "Schema applied",
    "Updating TreeDB schema in __system__",
    "Schema from C withdrew work on the schema at open",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Schema saved",
    "Schema applied",
    "Updating TreeDB schema in __system__",
    "Schema from C withdrew work on the schema at open",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  ...and what an open withdraws is said: an unsaved draft of `users`;
     *  a save taken back withdraws nothing and says nothing; an apply that
     *  never ran. Each literal carries `departments` below the version the
     *  store runs (D raised it) with other columns: the store keeps its own,
     *  said at each open  */
    "TreeDB schema from C is behind the schema in use, not applied",
    "Updating TreeDB schema in __system__",
    "Topic from C declares other columns than the store runs, without raising its topic_version past it: the store keeps running its own",
    "Schema from C withdrew work on the schema at open",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Schema saved",
    "Saved schema withdrawn, the draft is the schema in use",
    "Updating TreeDB schema in __system__",
    "Topic from C declares other columns than the store runs, without raising its topic_version past it: the store keeps running its own",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Schema saved",
    "Schema applied",
    "Updating TreeDB schema in __system__",
    "Topic from C declares other columns than the store runs, without raising its topic_version past it: the store keeps running its own",
    "Schema from C withdrew work on the schema at open",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  Test 13c: apply of all is all or none: A saved, B opened (+ its
     *  three), B's saved schema does not parse, then A alone applied. The
     *  file runs, and `departments` runs AHEAD of it (13b4 left the store
     *  with its own): said at every open that runs the file  */
    "TreeDB schema from C is behind the schema in use, not applied",
    "Schema file in use declares other columns than the store runs, at a topic_version behind the store's (a schema written whole over a topic the store had raised): the store runs its own, which only its topic_cols.json says. __system__ holds the file's columns, so save-schema has nothing to save until the topic is edited there: to keep what runs, edit the topic in __system__ to those columns, then save-schema and apply-schema; to run the file's, raise its topic_version above running_version (and the schema_version) in the schema from C",
    "Schema saved",
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "wrong type for list",
    "Data must be an {} or [{}]",
    "wrong type for list",
    "topic without cols",
    "Schema applied",
    /*  ...and B's saved file cannot be READ: saved-schema says broken, the
     *  apply of all leaves B out and applies A  */
    "TreeDB schema from C is behind the schema in use, not applied",
    "Schema file in use declares other columns than the store runs, at a topic_version behind the store's (a schema written whole over a topic the store had raised): the store runs its own, which only its topic_cols.json says. __system__ holds the file's columns, so save-schema has nothing to save until the topic is edited there: to keep what runs, edit the topic in __system__ to those columns, then save-schema and apply-schema; to run the file's, raise its topic_version above running_version (and the schema_version) in the schema from C",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    "Schema saved",
    "Cannot load json file, bad json",
    "Cannot load json file, bad json",
    "Schema applied",
    /*  Test 13d: every command refuses `denied`, which logs nothing  */
    /*  Test 13d2: the data of a peer is refused, a WARNING each (V20.4)  */
    AS_WARNING "Command parameter of the wrong type",       // save-schema treedb_name 5
    AS_WARNING "Command parameter of the wrong type",       // apply-schema treedb_name 5
    AS_WARNING "treedb not open here, refused",
    AS_WARNING "treedb not open here, refused",
    AS_WARNING "treedb not open here, refused",
    AS_WARNING "treedb not open here, refused",
    AS_WARNING "topic not found, refused",
    AS_WARNING "system topic, delete refused",
    AS_WARNING "Topic refused: bad name",                   // a/b, the treedb's own check
    AS_WARNING "Topic refused: bad columns",                // no columns
    AS_WARNING "Topic refused: bad name",                   // longer than NAME_MAX
    AS_WARNING "treedb name breaks the name rule, or is too long, refused",
    AS_WARNING "open-treedb without a treedb_schema json object, refused",
    AS_WARNING "Command parameter of the wrong type",       // files_max_size "big": the parser's
    AS_WARNING "Command parameter of the wrong type",       // 1.5: the parser's too, a real with decimals (V21.3.2)
    /*  Test 13d3: more of a peer's data refused, a WARNING each (V20)  */
    AS_WARNING "topic of a wrong shape, refused",           // a file: __timeranger2__.json
    AS_WARNING "topic of a wrong shape, refused",           // a file: the schema file
    AS_WARNING "Topic refused: bad pkey2s",                 // pkey2s twice
    AS_WARNING "Topic refused: bad pkey2s",                 // pkey2 "id"
    AS_WARNING "Topic refused: bad pkey2s",                 // pkey2 with '`'
    AS_WARNING "Topic refused: bad pkey2s",                 // pkey2 the tkey
    AS_WARNING "topic of a wrong shape, refused",           // too long for its lists
    AS_WARNING "open-treedb with a filename_mask that is not one file name, refused",
    AS_WARNING "open-treedb with a filename_mask that is not one file name, refused",
    AS_WARNING "open-treedb with a filename_mask that is not one file name, refused",
    AS_WARNING "treedb name breaks the name rule, or is too long, refused",  // __system__
    AS_WARNING "treedb name breaks the name rule, or is too long, refused",  // __other
    AS_WARNING "treedb name breaks the name rule, or is too long, refused",  // a^b
    AS_WARNING "treedb name breaks the name rule, or is too long, refused",  // 109 bytes
    AS_WARNING "treedb name breaks the name rule, or is too long, refused",  // .x
    AS_WARNING "treedb name breaks the name rule, or is too long, refused",  // x.json
    AS_WARNING "Topic refused: bad pkey2s",                 // a topic of the schema
    AS_WARNING "open-treedb with the schema of another treedb, refused",
    AS_WARNING "open-treedb with an attribute of a wrong type, refused",
    /*  a value of a wrong type from a user with no permission: the parser's, before authz  */
    AS_WARNING "Command parameter of the wrong type",
    AS_WARNING "Command parameter of the wrong type",
    AS_WARNING "Command parameter of the wrong type",
    AS_WARNING "Command parameter of the wrong type",
    /*  the seeds of an EXTERNAL open-treedb, applied again at its next start, then of an internal one  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    AS_WARNING "initial_load: topic not found in treedb",
    "initial_load: seed record created",
    "initial_load: seed record created",
    "initial_load: seed record created",
    AS_WARNING "initial_load: bad parent ref",              // a ref to a topic the treedb has not
    AS_WARNING "initial_load: bad parent ref",              // a ref through a hook that is not one
    AS_WARNING "initial_load: topic not found in treedb",   // the start after a stop: a peer's still
    AS_WARNING "initial_load: bad parent ref",
    AS_WARNING "initial_load: bad parent ref",
    AS_ERROR "initial_load: topic not found in treedb",
    AS_ERROR "initial_load: bad parent ref",
    AS_ERROR "initial_load: bad parent ref",
    /*  Test 13d4: a peer's schema refused before the open, and its store  */
    AS_WARNING "Topic refused: bad columns",                // no columns
    AS_WARNING "Topic refused: bad columns",                // no 'id' column
    AS_WARNING "a 'file' column must be flagged 'fkey' too",
    AS_WARNING "Topic refused: bad 'file' column",
    /*  the same from C: the open goes as it always went, the topic logged
     *  and left out, and the treedb opens (REV-CLOUD V20 #1)  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    AS_ERROR "Topic refused: bad columns",
    "Cannot create topic of the schema",                    // a CRITICAL, exit_on_error 0
    "Creating topic",
    "Creating topic",
    /*  a name of the system from C opens (#1), and a refusal of C_TREEDB
     *  for a C caller is an ERROR (create-topic of a file of the database)  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    /*  ...and its create-topic of a file of the database goes as it went:
     *  the tranger refuses it (exit_on_error 0)  */
    AS_ERROR "Not a directory: the path exists and is not a directory",
    "Cannot create TimeRanger subdir. mkrdir() FAILED",     // a CRITICAL
    "Cannot create topic: it is not whole, what was made is removed",   // a CRITICAL
    AS_WARNING "link topic not found",                      // parse_schema(), in the window
    AS_WARNING "open-treedb from outside with a setting of the code, refused",  // exit_on_error
    AS_WARNING "open-treedb from outside with a setting of the code, refused",  // import_root
    AS_WARNING "treedb name breaks the name rule, or is too long, refused",     // .treedb_schema.json
    AS_WARNING "open-treedb with an attribute of a wrong type, refused",        // a seed id 5
    AS_WARNING "topic of a wrong shape, refused",           // a column id too long for __system__
    AS_WARNING "open-treedb of a store that is not that treedb's, or held, refused",
    "Creating __timeranger2__.json",                        // the store a master holds
    AS_WARNING "open-treedb of a store that is not that treedb's, or held, refused",
    "Creating __timeranger2__.json",                        // the store a replica tranger has open
    AS_WARNING "open-treedb of a store that is not that treedb's, or held, refused",
    AS_WARNING "topic of a wrong shape, refused",           // .blobs
    AS_WARNING "topic of a wrong shape, create refused",    // C_TRANGER: a topic of a treedb
    AS_WARNING "Topic refused: bad pkey2s",                 // save-schema of a draft with a pkey2 "id" (#18)
    AS_WARNING "treedb name breaks the name rule, or is too long, refused",     // delete-treedb x/../
    AS_WARNING "Topic refused: bad pkey2s",                 // apply-schema of a pkey2 "id"
    "Creating __timeranger2__.json",                        // a dict of topics: opens
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    /*  Test 13d5 #1: a C caller's open goes as it did: the store made...  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    /*  ...a peer's open over a leftover topic directory, and with a seed
     *  without a string id, refused before anything is made...  */
    AS_WARNING "topic of a wrong shape, refused",
    AS_WARNING "open-treedb with an attribute of a wrong type, refused",
    /*  ...the C caller's opens, the leftover topic and the seed logged and
     *  left out, as tr_treedb, timeranger2 and C_NODE always did  */
    "Updating TreeDB schema in __system__",
    "Re-Creating TreeDB schema file",
    AS_ERROR "kw must be list or dict",                     // tranger2_create_topic() over a directory
    AS_ERROR "kw must be list or dict",                     //   with no topic_var.json nor topic_desc.json
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    AS_ERROR "Not a topic: topic_desc.json not found",
    "Cannot create topic of the schema",                    // a CRITICAL, exit_on_error 0
    AS_ERROR "initial_load: record without id",
    "initial_load: seed record created",
    /*  #0: a peer forces the close of a treedb the C code opened; #15 its own  */
    AS_WARNING "close-treedb from outside of a treedb the code of the yuno opened, refused",
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    AS_WARNING "close-treedb force of a treedb this peer did not open, refused",   // another user
    /*  #3: C_TRANGER writes of a peer into a treedb's topics, a new topic in its store  */
    AS_WARNING "write of a topic of a treedb or a msg2db, refused",     // add-record
    AS_WARNING "write of a topic of a treedb or a msg2db, refused",     // delete-key
    AS_WARNING "write of a topic of a treedb or a msg2db, refused",     // delete-topic
    AS_WARNING "topic of a wrong shape, create refused",                // create-topic
    /*  the shapes of a peer's schema  */
    AS_WARNING "topic of a wrong shape, refused",           // __timeranger2__.json
    AS_WARNING "topic of a wrong shape, refused",           // <treedb>.treedb_schema.json
    AS_WARNING "topic of a wrong shape, refused",           // .blobs
    AS_WARNING "topic of a wrong shape, refused",           // __snaps__
    AS_WARNING "topic of a wrong shape, refused",           // old.bak
    AS_WARNING "open-treedb from outside with a setting of the code, refused",  // impose_c_schema
    AS_WARNING "topic of a wrong shape, refused",           // topic_name is not the id
    AS_WARNING "topic of a wrong shape, refused",           // no id
    AS_WARNING "topic of a wrong shape, refused",           // id 5
    AS_WARNING "topic of a wrong shape, refused",           // a dict entry whose id is not its key
    AS_WARNING "A hook column must be of type dict or list",
    AS_WARNING "Topic refused: a link column of a wrong type",
    AS_WARNING "topic of a wrong shape, refused",           // topic_version {}
    AS_WARNING "topic of a wrong shape, refused",           // system_topic 1
    AS_WARNING "topic of a wrong shape, refused",           // main_topic "yes"
    AS_WARNING "topic of a wrong shape, refused",           // topic_version "017"
    AS_WARNING "topic of a wrong shape, refused",           // topic_version 2147483648
    AS_WARNING "schema of a wrong shape, refused",          // schema_version []
    AS_WARNING "schema of a wrong shape, refused",          // schema_version 2147483648
    AS_WARNING "Schema refused: two elements have the same qualified id in __system__ (a name with a dot), rename one of them",
    AS_WARNING "Topic refused: its 'file' column has no __assets__ hook of its own",
    AS_WARNING "topic of a wrong shape, refused",           // create-topic .<treedb>.treedb_schema.json.tmp
    AS_WARNING "topic of a wrong shape, refused",           // create-topic x.json
    AS_WARNING "topic of a wrong shape, refused",           // create-topic topic_version 2147483648
    /*  #7: create-topic of a file column of a hook the treedb has  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    AS_WARNING "Topic refused: its 'file' column has no __assets__ hook of its own",
    /*  #12: versions past an int, from C: the literal is installed  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  Test 13d6 c4 A: a peer's treedb and a msg2db `alarms` on its tranger...  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating Msg2DB schema file",
    "Creating topic",                                       // alarms
    /*  ...create-topic of the msg2db's topic, refused; a C caller adopts
     *  `alarms` (no log), a peer's delete of it refused, the msg2db open and
     *  closed (its schema file); the open of the store whose schema names
     *  `alarms`, refused; create-topic of it, the msg2db closed, refused  */
    AS_WARNING "topic of the store that is not this treedb's, refused",
    AS_WARNING "delete of a topic that a msg2db or another treedb holds too, refused",
    AS_WARNING "delete of a topic that a msg2db or another treedb holds too, refused",
    AS_WARNING "topic of the store that is not this treedb's, refused",
    AS_WARNING "topic of the store that is not this treedb's, refused",
    /*  c4 #2: a C caller's store with `items` and a topic of 196 bytes...  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    /*  ...opened again by a literal that gives them bad pkey2s: kept, as they
     *  were, each fault an ERROR (and the instance the pkey2 `id` cannot index,
     *  as it always was)  */
    "Updating TreeDB schema in __system__",
    "Re-Creating TreeDB schema file",
    AS_ERROR "Topic on disk opened as it was, with bad pkey2s: fix its schema, a new topic with them is refused",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    AS_ERROR "cannot add an instance to the pkey2 index",
    AS_ERROR "Topic on disk opened as it was, the id of a list of its indexes cut to NAME_MAX: shorten its names, a new topic with them is refused",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  c4 #3: a hook column of type string, a peer's refused, the C caller's created  */
    AS_WARNING "A hook column must be of type dict or list",
    AS_WARNING "Topic refused: a link column of a wrong type",
    "Creating topic",
    "Deleting topic",
    /*  c4 C: two main_topic, a main_topic not hooked to itself; one, opened  */
    AS_WARNING "schema with a wrong main_topic, refused",
    AS_WARNING "schema with a wrong main_topic, refused",
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    /*  c4 D: a peer's save past the largest version, of the schema and of a topic  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    AS_WARNING "save-schema would raise a version past 2147483647, refused",
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    AS_WARNING "save-schema would raise a version past 2147483647, refused",
    /*  c4 Residual: a peer's open of a treedb the code opens, closed  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    AS_WARNING "open-treedb from outside of a treedb the code of the yuno opens, refused",
    AS_WARNING "open-treedb from outside of a treedb the code of the yuno opens, refused",  // c6r: another case
    /*  c7: a peer's open of `tranger_treedb_code_own` (the code's tranger),
     *  refused; a peer's delete-treedb (force) of the code's closed treedb, by
     *  its name and in another case, refused; the code opens it again, and a
     *  peer's open of its tranger's name is answered by that service (no log)  */
    AS_WARNING "open-treedb from outside with the service names of a treedb the code of the yuno opens, refused",
    AS_WARNING "delete-treedb from outside of a treedb the code of the yuno opened, refused",
    AS_WARNING "delete-treedb from outside of a treedb the code of the yuno opened, refused",  // another case
    /*  c7: the code's `tranger_code_side`, and a peer's `Code_Side` (whose
     *  tranger it is) refused while it is closed  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    AS_WARNING "open-treedb from outside with the service names of a treedb the code of the yuno opens, refused",
    /*  Test 13d7 c5 #7: a peer's close of a treedb the code opened, playing and
     *  paused (C_TREEDB kept, as mqtt_broker keeps it), with force and without,
     *  by its name and (c6r) in another case; a peer's treedb named in another
     *  case: another user's force refused, the opener's closes done; the
     *  code's own close, paused, logs nothing  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating __timeranger2__.json",                        // treedb_peer_case, a peer's
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    AS_WARNING "close-treedb from outside of a treedb the code of the yuno opened, refused",
    AS_WARNING "close-treedb from outside of a treedb the code of the yuno opened, refused",
    AS_WARNING "close-treedb from outside of a treedb the code of the yuno opened, refused",  // another case
    AS_WARNING "close-treedb from outside of a treedb the code of the yuno opened, refused",  // another case, force
    AS_WARNING "close-treedb force of a treedb this peer did not open, refused",   // another user, another case
    "Pausing yuno",
    AS_WARNING "close-treedb from outside of a treedb the code of the yuno opened, refused",
    AS_WARNING "close-treedb from outside of a treedb the code of the yuno opened, refused",
    AS_WARNING "close-treedb from outside of a treedb the code of the yuno opened, refused",  // another case
    AS_WARNING "close-treedb from outside of a treedb the code of the yuno opened, refused",  // another case, force
    "Playing yuno",
    /*  c5 #1: a peer's treedb with {items, x}, then {items} (x withdrawn from
     *  __system__), then {items, x} again: taken, x at its new version  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Updating TreeDB schema in __system__",
    "Topic not declared by the schema from C: removed from __system__",
    "Re-Creating TreeDB schema file",
    "Updating TreeDB schema in __system__",
    "Re-Creating TreeDB schema file",
    "Re-Creating topic_var.json",
    "Re-Creating topic_cols.json",
    /*  c5 #4: a peer's topic `events` with the string tkey `when`; a date past
     *  44 bits refused in create-node and update-node of a peer, and in a C
     *  caller's create; a C caller's append to the tranger kept, and said  */
    "Creating __timeranger2__.json",
    "Creating TreeDB schema file",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",
    "Creating topic",                                       // events
    AS_WARNING "Node refused: the time of its tkey does not fit a record (negative, or past 44 bits)",
    AS_WARNING "Node refused: the time of its tkey does not fit a record (negative, or past 44 bits)",
    AS_ERROR "Node refused: the time of its tkey does not fit a record (negative, or past 44 bits)",
    AS_WARNING "Node refused: its tkey is a text that is no date",   // V46.3, a peer
    AS_ERROR "Node refused: its tkey is a text that is no date",     // and a C caller
    "Creating topic",                                       // raw_t
    AS_ERROR "Record time past what an md2 row keeps (44 bits): appended CUT",
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
    size_t ln_mark = strlen(AS_WARNING);
    size_t ln_error_mark = strlen(AS_ERROR);
    for(int i = 0; expected_log_msgs[i]; i++) {
        const char *msg = expected_log_msgs[i];
        if(strncmp(msg, AS_WARNING, ln_mark)==0) {
            json_array_append_new(list, json_pack("{s:s, s:s}",
                "msg", msg + ln_mark, "__priority__", "WARNING"));
        } else if(strncmp(msg, AS_ERROR, ln_error_mark)==0) {
            json_array_append_new(list, json_pack("{s:s, s:s}",
                "msg", msg + ln_error_mark, "__priority__", "ERROR"));
        } else {
            json_array_append_new(list, json_pack("{s:s}", "msg", msg));
        }
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
