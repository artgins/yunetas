/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_node_authz
 *          Tests the permission every C_NODE command asks for
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>
#include "c_test_node_authz.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_node_authz"
#define APP_DOC         "Test the permission of every C_NODE command"

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
            'name': 'test_node_authz',                              \n\
            'gclass': 'C_TEST_NODE_AUTHZ',                          \n\
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
 *  The users of this test and what each one may do. What C_AUTHZ does with
 *  roles, reduced to a table: the name of the permission, or nothing.
 *
 *      nobody      nothing
 *      reader      read
 *      editor      read, update
 *      creator     read, update, create
 *      deleter     read, delete
 *      keeper      read, delete, create
 *      owner       every permission
 ***************************************************************************/
static BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    const char *username = kw_get_str(gobj, kw, "__username__", "", 0);
    BOOL allow = FALSE;

    if(strcmp(username, "reader")==0) {
        allow = (strcmp(authz, "read")==0)? TRUE: FALSE;
    } else if(strcmp(username, "editor")==0) {
        allow = (strcmp(authz, "read")==0 || strcmp(authz, "update")==0)? TRUE: FALSE;
    } else if(strcmp(username, "creator")==0) {
        allow = (strcmp(authz, "read")==0 || strcmp(authz, "update")==0 ||
            strcmp(authz, "create")==0)? TRUE: FALSE;
    } else if(strcmp(username, "deleter")==0) {
        allow = (strcmp(authz, "read")==0 || strcmp(authz, "delete")==0)? TRUE: FALSE;
    } else if(strcmp(username, "keeper")==0) {
        allow = (strcmp(authz, "read")==0 || strcmp(authz, "delete")==0 ||
            strcmp(authz, "create")==0)? TRUE: FALSE;
    } else if(strcmp(username, "owner")==0) {
        allow = TRUE;
    }

    KW_DECREF(kw)
    return allow;
}

/***************************************************************************
 *  HACK This function is executed on yunetas environment (mem, log, paths)
 *  BEFORE creating the yuno
 ***************************************************************************/
int result = 0;

static int register_yuno_and_more(void)
{
    int result = 0;

    /*--------------------*
     *  Register gclass
     *--------------------*/
    result += register_c_test_node_authz();

    /*------------------------------*
     *  Start test
     *------------------------------*/
    set_expected_results(
        APP_NAME,
        /*  Strict FIFO: every gobj_log_info the run emits, in order. Four
         *  "Creating topic": the schema declares `items` and the treedb adds
         *  its own three (__snaps__, __graphs__, __assets__). A refusal logs
         *  nothing: it is the -403 of the answer. The two errors are asked
         *  for: the link the nested-update check refuses, the autolink
         *  update refused on the replica, and its link, unlink and forced
         *  delete refused before anything moves. */
        json_pack("[{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "Starting yuno",
            "msg", "Creating __timeranger2__.json",
            "msg", "Creating TreeDB schema file",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Creating topic",
            "msg", "Playing yuno",
            "msg", "fkey reference: parent node not found",
            /*  the failing commands of check_comments_name_their_yuno  */
            "msg", "Node already exists",
            "msg", "hook field not found",
            "msg", "hook field not found",
            "msg", "Snap already exists: 'snap-msg'",
            "msg", "Topic name not found in treedbs",
            "msg", "Topic name not found in treedbs",
            "msg", "Topic name not found in treedbs",
            "msg", "Node not found",
            "msg", "Node not found",
            "msg", "hook not found",
            "msg", "Topic name not found in treedbs",
            "msg", "Topic name not found in treedbs",
            /*  an update-create with a bad id: the library's cause, then
             *  C_NODE's own (not the last message again)  */
            "msg", "Invalid 'id': contains path metacharacters",
            "msg", "Cannot update node: it does not exist and it cannot be created (see the previous log)",
            /*  the replica  */
            "msg", "Cannot write a node on a READ-ONLY replica",
            "msg", "Cannot link nodes on a READ-ONLY replica",
            "msg", "Cannot unlink nodes on a READ-ONLY replica",
            "msg", "Cannot delete a node on a READ-ONLY replica",
            "msg", "Cannot link nodes on a READ-ONLY replica",
            "msg", "All c_node authz tests PASSED",
            "msg", "Exit to die",
            "msg", "Exit to die",
            "msg", "Pausing yuno",
            "msg", "Yuno stopped, gobj end"
        ),
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
