/****************************************************************************
 *          MAIN_PERF_AUTH_BFF.C
 *
 *          Self-contained throughput benchmark for c_auth_bff.
 *          5 concurrent HTTP client slots × 36 000 iterations =
 *          180 000 full /auth/login round-trips against 5 parallel
 *          BFF channels with latency_ms=0 on the mock Keycloak so
 *          the bottleneck is the BFF + task + parser hot path.
 *          Each slot reuses one TCP connection (HTTP/1.1 keep-alive)
 *          for all its iterations, matching the persistent-connection
 *          model used by perf_c_tcps.
 *
 *          Prints a standard #TIME line at shutdown, same format as
 *          perf_c_tcp / perf_c_tcps, so results sit next to the
 *          other perf numbers in performance/c/README.md.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include <yunetas.h>

#include "c_mock_keycloak.h"
#include "c_perf_auth_bff.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "perf_auth_bff"
#define APP_DOC         "Concurrent throughput benchmark for c_auth_bff"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define BFF_PORT    "18801"
#define KC_PORT     "18802"

/***************************************************************************
 *                      Default config
 ***************************************************************************/
PRIVATE char fixed_config[]= "\
{                                                                   \n\
    'yuno': {                                                       \n\
        'yuno_role': '"APP_NAME"',                                  \n\
        'tags': ['perf', 'yunetas']                                 \n\
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
            'name': 'c_perf_auth_bff',                              \n\
            'gclass': 'C_PERF_AUTH_BFF',                            \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'bff_url': 'http://127.0.0.1:"BFF_PORT"/'           \n\
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
                }                                                   \n\
            ],                                                      \n\
            '[^^children^^]': {                                     \n\
                '__range__': [1, 6],                                \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'bff-(^^__range__^^)',                  \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'bff-(^^__range__^^)',          \n\
                            'gclass': 'C_AUTH_BFF',                 \n\
                            'kw': {                                 \n\
                                'keycloak_url': 'http://127.0.0.1:"KC_PORT"/', \n\
                                'realm':        'test',             \n\
                                'client_id':    'test-client',      \n\
                                'client_secret': '',                \n\
                                'cookie_domain': '',                \n\
                                'allowed_origin': '',               \n\
                                'allowed_redirect_uri': '',         \n\
                                'crypto': {},                       \n\
                                'pending_queue_size': 16            \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'bff-(^^__range__^^)',  \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'bff-(^^__range__^^)', \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__kc_side__',                                  \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'kc_server',                            \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:"KC_PORT"',           \n\
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
                '__range__': [1, 6],                                \n\
                '__vars__': {                                       \n\
                },                                                  \n\
                '__content__': {                                    \n\
                    'name': 'kc-(^^__range__^^)',                   \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'kc-(^^__range__^^)',           \n\
                            'gclass': 'C_MOCK_KEYCLOAK',            \n\
                            'kw': {                                 \n\
                                'latency_ms': 0                     \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'kc-(^^__range__^^)',   \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'kc-(^^__range__^^)', \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
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

PRIVATE BOOL test_authz_checker(hgobj gobj, const char *authz, json_t *kw, hgobj src)
{
    KW_DECREF(kw)
    return TRUE;
}

int result = 0;

static int register_yuno_and_more(void)
{
    int res = 0;

    if(mock_keycloak_init_jwk() < 0) {
        return -1;  // Error already logged
    }

    res += register_c_mock_keycloak();
    res += register_c_perf_auth_bff();

    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER),  "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    set_auto_kill_time(300);  /* headroom for 180 000 round-trips */

    set_expected_results(
        APP_NAME,
        json_array(),
        NULL,
        NULL,
        TRUE
    );

    /*
     *  MT_START_TIME is called inside the orchestrator's
     *  launch_all_slots() so the timing brackets the actual
     *  load only, not the register/warm-up phase.
     */

    return res;
}

static void cleaning(void)
{
    /*
     *  perf_time_measure was started by launch_all_slots() and the
     *  count was set in maybe_finish_and_die().  Print the TIME
     *  line now, same format as perf_c_tcp / perf_c_tcps so the
     *  result is grep-friendly and fits the existing README tables.
     */
    MT_PRINT_TIME(perf_time_measure, APP_NAME)

    result += test_json(NULL);
    mock_keycloak_end_jwk();
}

int main(int argc, char *argv[])
{
    glog_init();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_UP_WARNING, 0);

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
        NULL,
        NULL,
        NULL,
        test_authz_checker,
        NULL,
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
