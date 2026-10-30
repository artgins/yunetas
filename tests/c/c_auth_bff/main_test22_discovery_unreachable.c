/****************************************************************************
 *          MAIN_TEST22_DISCOVERY_UNREACHABLE.C
 *
 *          Self-contained auth_bff test: BFF configured with `issuer` on a
 *          port where the IdP cannot be reached: a listener of the test
 *          whose accept queue is full (backlog 0, one connection never
 *          accepted), so the kernel drops the SYN of the BFF and its
 *          connect waits. BFF `idp_connect_timeout_ms` = 500: the connect
 *          watchdog ends the discovery task. A login is queued behind the
 *          discovery (the driver, from a plain socket).
 *
 *          Asserts that the discovery failure is ONE ERROR ("OIDC
 *          discovery: IdP did not answer in time", with the cause of the
 *          connect watchdog), plus the ERROR of the 502 answered to the
 *          login queued, and that it counts in `idp_timeouts`. In the
 *          fixes after 7.29.3 the watchdog said it too ("IdP unreachable,
 *          task watchdog fired") and the drain of the queue a third time
 *          ("OIDC discovery failed, draining queue").
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <yunetas.h>

#include "c_test22_discovery_unreachable.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_auth_bff_test22_discovery_unreachable"
#define APP_DOC         "auth_bff OIDC discovery, IdP unreachable"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define BFF_PORT    "18882"
#define KC_PORT     "18883"

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
        'work_dir': '/tmp',                                         \n\
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
        'realm_owner': 'test',                                      \n\
        'realm_id':    'test',                                      \n\
        'trace_levels': {                                           \n\
        }                                                           \n\
    },                                                              \n\
    'global': {                                                     \n\
        'Authz.allow_anonymous_in_localhost': true                  \n\
    },                                                              \n\
    'services': [                                                   \n\
        {                                                           \n\
            'name': 'c_test22_discovery_unreachable',               \n\
            'gclass': 'C_TEST22_DISCOVERY_UNREACHABLE',             \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'check_delay_ms': 3000,                             \n\
                'bff_port': '"BFF_PORT"'                            \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__bff_side__',                                 \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'bff_server',                           \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:"BFF_PORT"',          \n\
                        'child_tree_filter': {                      \n\
                            'kw': {                                 \n\
                                '__gclass_name__': 'C_CHANNEL',     \n\
                                '__disabled__': false,              \n\
                                'connected': false                  \n\
                            }                                       \n\
                        }                                           \n\
                    }                                               \n\
                },                                                  \n\
                {                                                   \n\
                    'name': 'bff-1',                                \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'bff-1',                        \n\
                            'gclass': 'C_AUTH_BFF',                 \n\
                            'kw': {                                 \n\
                                'issuer': 'http://127.0.0.1:"KC_PORT"/realms/test/', \n\
                                'client_id':    'test-client',      \n\
                                'client_secret': '',                \n\
                                'cookie_domain': '',                \n\
                                'allowed_origin': '',               \n\
                                'allowed_redirect_uri': '',         \n\
                                'crypto': {},                       \n\
                                'pending_queue_size': 16,           \n\
                                'idp_timeout_ms': 500,              \n\
                                'idp_connect_timeout_ms': 500       \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'bff-1',                \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'bff-1',        \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

/***************************************************************************
 *  Authz checker: allow everything in the self-contained test
 ***************************************************************************/
PRIVATE BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return TRUE;
}

time_measure_t time_measure;

int result = 0;

/*
 *  The IdP that cannot be reached: a listener whose accept queue is full
 */
PRIVATE int fd_idp_listen = -1;
PRIVATE int fd_idp_filler = -1;

PRIVATE int open_the_unreachable_idp(void)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)atoi(KC_PORT));

    int one = 1;
    fd_idp_listen = socket(AF_INET, SOCK_STREAM, 0);
    fd_idp_filler = socket(AF_INET, SOCK_STREAM, 0);
    if(fd_idp_listen < 0 || fd_idp_filler < 0 ||
            setsockopt(fd_idp_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0 ||
            bind(fd_idp_listen, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
            listen(fd_idp_listen, 0) < 0 ||
            connect(fd_idp_filler, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM,
            "msg",          "%s", "TEST: cannot make the IdP that cannot be reached",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
        return -1;
    }
    return 0;
}

PRIVATE void close_the_unreachable_idp(void)
{
    if(fd_idp_filler >= 0) {
        close(fd_idp_filler);
        fd_idp_filler = -1;
    }
    if(fd_idp_listen >= 0) {
        close(fd_idp_listen);
        fd_idp_listen = -1;
    }
}

static int register_yuno_and_more(void)
{
    int res = 0;

    res += register_c_test22_discovery_unreachable();

    if(open_the_unreachable_idp() < 0) {
        res += -1;  // Error already logged
    }

    /* Suppress noisy traces */
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER),  "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    set_auto_kill_time(10);

    /*
     *  ONE ERROR for the failure of the discovery, with the cause of the
     *  connect watchdog, and the ERROR of the 502 answered to the login
     *  that was queued behind it.
     *  kw_match_simple is a subset match.
     */
    json_t *expected_errors = json_pack("[{s:s, s:s}, {s:s, s:s}]",
        "msg",          "👤BFF OIDC discovery: IdP did not answer in time",
        "cause",        "idp_connect_timeout_ms: the IdP could not be reached",
        "msg",          "👤BFF server error",
        "error_code",   "auth_service_unavailable"
    );
    set_expected_results(
        APP_NAME,
        expected_errors,
        NULL,           // no JSON comparison
        NULL,           // no ignore_keys
        TRUE            // verbose
    );

    MT_START_TIME(time_measure)

    return res;
}

static void cleaning(void)
{
    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, APP_NAME)

    result += test_json(NULL);
    close_the_unreachable_idp();
}

int main(int argc, char *argv[])
{
    glog_init();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    gobj_log_register_handler(
        "testing",
        0,
        capture_log_write,
        0
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_ERROR, 0);

    unsigned long memory_check_list[] = {0, 0};
    set_memory_check_list(memory_check_list);

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

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    return result;
}
