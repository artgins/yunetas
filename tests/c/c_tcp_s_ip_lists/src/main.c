/****************************************************************************
 *          MAIN.C
 *
 *          Main of test_c_tcp_s_ip_lists
 *          Tests the yuno's ip lists (denied_ips, allowed_ips) at C_TCP_S accept
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <yunetas.h>
#include "c_test_ip_lists.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_c_tcp_s_ip_lists"
#define APP_DOC         "Test the ip lists at C_TCP_S accept"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
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
        'allowed_ips': {                                            \n\
            'fe80::7': true,                                        \n\
            'fe80::8%1': true,                                      \n\
            '::FFFF:10.9.9.12': true                                \n\
        },                                                          \n\
        'denied_ips': {                                             \n\
            '2001:DB8::5': true,                                    \n\
            '2001:db8:0:0:0:0:0:5': false,                          \n\
            '::ffff:10.9.9.9': true,                                \n\
            '10.9.9.10:443': true,                                  \n\
            '[2001:db8::6]': true,                                  \n\
            'not-an-ip': true,                                      \n\
            '10.9.9.11': true                                       \n\
        },                                                          \n\
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
            'name': 'test_ip_lists',                                \n\
            'gclass': 'C_TEST_IP_LISTS',                            \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'children': [                                           \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__input_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': false,                                     \n\
            'autoplay': false,                                      \n\
            'kw': {                                                 \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'server_port',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://127.0.0.1:7793',              \n\
                        'child_tree_filter': {                      \n\
                            'kw': {                                 \n\
                                '__gclass_name__': 'C_CHANNEL',     \n\
                                '__disabled__': false,              \n\
                                'connected': false                  \n\
                            }                                       \n\
                        }                                           \n\
                    }                                               \n\
                }                                                   \n\
            ],                                                      \n\
            '[^^children^^]': {                                     \n\
                '__range__': [1,3],                                 \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'input-(^^__range__^^)',                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'input-(^^__range__^^)',        \n\
                            'gclass': 'C_PROT_TCP4H',               \n\
                            'kw': {                                 \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'gclass': 'C_TCP'               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

time_measure_t time_measure;

/***************************************************************************
 *  Persistent attrs: nothing is kept (the test has no realm), each save of
 *  an ip list is counted
 ***************************************************************************/
int ip_list_saves_allowed = 0;
int ip_list_saves_denied = 0;

static int test_persist_startup(void)
{
    return 0;
}
static void test_persist_end(void)
{
}
static int test_persist_load(hgobj gobj, json_t *keys)
{
    JSON_DECREF(keys)
    return 0;
}
static int test_persist_save(hgobj gobj, json_t *keys)
{
    const char *attr = json_string_value(keys);
    if(attr && strcmp(attr, "allowed_ips")==0) {
        ip_list_saves_allowed++;
    } else if(attr && strcmp(attr, "denied_ips")==0) {
        ip_list_saves_denied++;
    }
    JSON_DECREF(keys)
    return 0;
}
static int test_persist_remove(hgobj gobj, json_t *keys)
{
    JSON_DECREF(keys)
    return 0;
}
static json_t *test_persist_list(hgobj gobj, json_t *keys)
{
    JSON_DECREF(keys)
    return json_object();
}
static const persistent_attrs_t test_persistent_attrs = {
    test_persist_startup,
    test_persist_end,
    test_persist_load,
    test_persist_save,
    test_persist_remove,
    test_persist_list
};

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
    result += register_c_test_ip_lists();

    /*------------------------------*
     *  Start test
     *------------------------------*/
    set_expected_results(
        APP_NAME,
        /*  Strict FIFO: every gobj_log_info the run emits, in order.
         *  After the start, the entries of the ip lists of the config, stored as
         *  typed by 7.25.4 and normalised at load: allowed_ips drops a
         *  link-local address without its interface and renames a mapped
         *  ipv4; denied_ips renames three (two of them one ip) and drops
         *  three that are no ip. Then one refusal in phase 1, two in phase
         *  2; a wrong verdict is an error, which is not in this list.  */
        json_pack("[{s:s}, {s:s,s:s}, {s:s,s:s}, "
                  "{s:s,s:s}, {s:s,s:s}, {s:s,s:s}, {s:s,s:s}, {s:s,s:s}, {s:s,s:s}, "
                  "{s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}, {s:s}]",
            "msg", "Starting yuno",
            "msg", "ip list entry dropped, it never matched a peer", "entry", "fe80::7",
            "msg", "ip list entry renamed to the form a peer is looked up by", "entry", "::FFFF:10.9.9.12",
            "msg", "ip list entry renamed to the form a peer is looked up by", "entry", "2001:DB8::5",
            "msg", "ip list entry renamed to the form a peer is looked up by", "entry", "2001:db8:0:0:0:0:0:5",
            "msg", "ip list entry renamed to the form a peer is looked up by", "entry", "::ffff:10.9.9.9",
            "msg", "ip list entry dropped, it never matched a peer", "entry", "10.9.9.10:443",
            "msg", "ip list entry dropped, it never matched a peer", "entry", "[2001:db8::6]",
            "msg", "ip list entry dropped, it never matched a peer", "entry", "not-an-ip",
            "msg", "Playing yuno",
            "msg", "TCP_S: Ip denied",
            "msg", "TCP_S: Ip denied",
            "msg", "TCP_S: Ip not allowed",
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
        &test_persistent_attrs,
        NULL,       // command_parser
        NULL,       // stats_parser
        NULL,       // authz_checker
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
