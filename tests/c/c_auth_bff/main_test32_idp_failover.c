/****************************************************************************
 *          MAIN_TEST32_IDP_FAILOVER.C
 *
 *          Self-contained auth_bff test of the failover between IdPs
 *          (`issuers`). Two BFFs and three IdPs of the same realm:
 *
 *            bff1 (BFF1_PORT)  issuers [A, B]
 *            bff2 (BFF2_PORT)  issuers [C, B]
 *            A    (IDP_A_PORT) a mock Keycloak whose tokens say iss A
 *            B    (IDP_B_PORT) a mock Keycloak whose tokens say iss B
 *            C    (IDP_C_PORT) an IdP that cannot be reached: a listener
 *                              whose accept queue is full (as in test22)
 *
 *            D    (IDP_D_PORT) a mock Keycloak that takes the discovery
 *                              and never answers it (silent_discovery)
 *            bff3 (BFF3_PORT)  issuers [D, B]
 *
 *          bff2 waits idp_connect_timeout_ms 3000 and idp_failover_connect_ms
 *          500: the discovery of C at its start is for a login that B can
 *          take, so it waits 500 (up to 7.31.29 it waited 3000, and so did
 *          the login that came in the meantime). bff3 waits idp_timeout_ms
 *          4000 and idp_failover_response_ms 1500: connected, the discovery
 *          of D at its start waits 1500 for the answer (up to 7.31.29 4000).
 *
 *          A login goes to the first IdP that is up and passes to the next
 *          one when its IdP cannot be reached, fails its discovery or
 *          answers 502/503/504; a refresh goes to the IdP of its session,
 *          and never to another. The driver (c_test32_idp_failover.c) says
 *          what each step must get.
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

#include "c_mock_keycloak.h"
#include "c_test32_idp_failover.h"

/***************************************************************************
 *                      Names
 ***************************************************************************/
#define APP_NAME        "test_auth_bff_test32_idp_failover"
#define APP_DOC         "auth_bff: failover between the IdPs of `issuers`"

#define APP_VERSION     "1.0.0"
#define APP_SUPPORT     "<support@artgins.com>"
#define APP_DATETIME    __DATE__ " " __TIME__

#define USE_OWN_SYSTEM_MEMORY   FALSE
#define MEM_MIN_BLOCK           0
#define MEM_MAX_BLOCK           0
#define MEM_SUPERBLOCK          0
#define MEM_MAX_SYSTEM_MEMORY   0

#define BFF1_PORT   "18943"
#define IDP_A_PORT  "18944"
#define IDP_B_PORT  "18945"
#define IDP_C_PORT  "18946"
#define BFF2_PORT   "18947"
#define IDP_D_PORT  "18954"
#define BFF3_PORT   "18955"

#define IDP_A       "http://127.0.0.1:"IDP_A_PORT"/realms/test"
#define IDP_B       "http://127.0.0.1:"IDP_B_PORT"/realms/test"
#define IDP_C       "http://127.0.0.1:"IDP_C_PORT"/realms/test"
#define IDP_D       "http://127.0.0.1:"IDP_D_PORT"/realms/test"

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
            'name': 'c_test32_idp_failover',                        \n\
            'gclass': 'C_TEST32_IDP_FAILOVER',                      \n\
            'default_service': true,                                \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'bff1_port': '"BFF1_PORT"',                         \n\
                'bff2_port': '"BFF2_PORT"',                         \n\
                'bff3_port': '"BFF3_PORT"'                          \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__idp_a_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'idpa_server',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:"IDP_A_PORT"',        \n\
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
                    'name': 'idpa-1',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'idpa-1',                       \n\
                            'gclass': 'C_MOCK_KEYCLOAK',            \n\
                            'kw': {                                 \n\
                                'issuer_claim': '"IDP_A"'           \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'idpa-1',               \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'idpa-1',       \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                },                                                  \n\
                {                                                   \n\
                    'name': 'idpa-2',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'idpa-2',                       \n\
                            'gclass': 'C_MOCK_KEYCLOAK',            \n\
                            'kw': {                                 \n\
                                'issuer_claim': '"IDP_A"'           \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'idpa-2',               \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'idpa-2',       \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__idp_b_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'idpb_server',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:"IDP_B_PORT"',        \n\
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
                    'name': 'idpb-1',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'idpb-1',                       \n\
                            'gclass': 'C_MOCK_KEYCLOAK',            \n\
                            'kw': {                                 \n\
                                'issuer_claim': '"IDP_B"'           \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'idpb-1',               \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'idpb-1',       \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                },                                                  \n\
                {                                                   \n\
                    'name': 'idpb-2',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'idpb-2',                       \n\
                            'gclass': 'C_MOCK_KEYCLOAK',            \n\
                            'kw': {                                 \n\
                                'issuer_claim': '"IDP_B"'           \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'idpb-2',               \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'idpb-2',       \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                },                                                  \n\
                {                                                   \n\
                    'name': 'idpb-3',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'idpb-3',                       \n\
                            'gclass': 'C_MOCK_KEYCLOAK',            \n\
                            'kw': {                                 \n\
                                'issuer_claim': '"IDP_B"'           \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'idpb-3',               \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'idpb-3',       \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                },                                                  \n\
                {                                                   \n\
                    'name': 'idpb-4',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'idpb-4',                       \n\
                            'gclass': 'C_MOCK_KEYCLOAK',            \n\
                            'kw': {                                 \n\
                                'issuer_claim': '"IDP_B"'           \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'idpb-4',               \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'idpb-4',       \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__idp_d_side__',                               \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'idpd_server',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:"IDP_D_PORT"',        \n\
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
                    'name': 'idpd-1',                               \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'idpd-1',                       \n\
                            'gclass': 'C_MOCK_KEYCLOAK',            \n\
                            'kw': {                                 \n\
                                'issuer_claim': '"IDP_D"',          \n\
                                'silent_discovery': true            \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'idpd-1',               \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'idpd-1',       \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__bff1_side__',                                \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'bff1_server',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:"BFF1_PORT"',         \n\
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
                    'name': 'bff1',                                 \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'bff1',                         \n\
                            'gclass': 'C_AUTH_BFF',                 \n\
                            'kw': {                                 \n\
                                'issuers': ['"IDP_A"/', '"IDP_B"/'],\n\
                                'client_id':    'test-client',      \n\
                                'client_secret': '',                \n\
                                'cookie_domain': '',                \n\
                                'allowed_origin': 'http://localhost',\n\
                                'allowed_redirect_uri': '',         \n\
                                'crypto': {},                       \n\
                                'idp_timeout_ms': 2000,             \n\
                                'idp_connect_timeout_ms': 500,      \n\
                                'idp_failover_connect_ms': 500,     \n\
                                'idp_down_retry_ms': 1000           \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'bff1',                 \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'bff1',         \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__bff2_side__',                                \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'bff2_server',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:"BFF2_PORT"',         \n\
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
                    'name': 'bff2',                                 \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'bff2',                         \n\
                            'gclass': 'C_AUTH_BFF',                 \n\
                            'kw': {                                 \n\
                                'issuers': ['"IDP_C"/', '"IDP_B"/'],\n\
                                'client_id':    'test-client',      \n\
                                'client_secret': '',                \n\
                                'cookie_domain': '',                \n\
                                'allowed_origin': 'http://localhost',\n\
                                'allowed_redirect_uri': '',         \n\
                                'crypto': {},                       \n\
                                'idp_timeout_ms': 2000,             \n\
                                'idp_connect_timeout_ms': 3000,     \n\
                                'idp_failover_connect_ms': 500,     \n\
                                'idp_down_retry_ms': 1000           \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'bff2',                 \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'bff2',         \n\
                                            'gclass': 'C_TCP'       \n\
                                        }                           \n\
                                    ]                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                           \n\
                    ]                                               \n\
                }                                                   \n\
            ]                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'name': '__bff3_side__',                                \n\
            'gclass': 'C_IOGATE',                                   \n\
            'autostart': true,                                      \n\
            'autoplay': true,                                       \n\
            'kw': {                                                 \n\
                'persistent_channels': false                        \n\
            },                                                      \n\
            'children': [                                           \n\
                {                                                   \n\
                    'name': 'bff3_server',                          \n\
                    'gclass': 'C_TCP_S',                            \n\
                    'kw': {                                         \n\
                        'url': 'tcp://0.0.0.0:"BFF3_PORT"',         \n\
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
                    'name': 'bff3',                                 \n\
                    'gclass': 'C_CHANNEL',                          \n\
                    'kw': {                                         \n\
                    },                                              \n\
                    'children': [                                   \n\
                        {                                           \n\
                            'name': 'bff3',                         \n\
                            'gclass': 'C_AUTH_BFF',                 \n\
                            'kw': {                                 \n\
                                'issuers': ['"IDP_D"/', '"IDP_B"/'],\n\
                                'client_id':    'test-client',      \n\
                                'client_secret': '',                \n\
                                'cookie_domain': '',                \n\
                                'allowed_origin': 'http://localhost',\n\
                                'allowed_redirect_uri': '',         \n\
                                'crypto': {},                       \n\
                                'idp_timeout_ms': 4000,             \n\
                                'idp_connect_timeout_ms': 3000,     \n\
                                'idp_failover_connect_ms': 500,     \n\
                                'idp_failover_response_ms': 1500,   \n\
                                'idp_down_retry_ms': 60000          \n\
                            },                                      \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'bff3',                 \n\
                                    'gclass': 'C_PROT_HTTP_SR',     \n\
                                    'kw': {                         \n\
                                    },                              \n\
                                    'children': [                   \n\
                                        {                           \n\
                                            'name': 'bff3',         \n\
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
    addr.sin_port = htons((uint16_t)atoi(IDP_C_PORT));

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

/***************************************************************************
 *  HACK: runs on yunetas environment BEFORE creating the yuno
 ***************************************************************************/
static int register_yuno_and_more(void)
{
    int res = 0;

    /*--------------------*
     *  Init JWT key first — any GClass that signs tokens depends on it
     *--------------------*/
    if(mock_keycloak_init_jwk() < 0) {
        return -1;  // Error already logged
    }

    /*--------------------*
     *  Register gclasses
     *--------------------*/
    res += register_c_mock_keycloak();
    res += register_c_test32_idp_failover();

    if(open_the_unreachable_idp() < 0) {
        res += -1;  // Error already logged
    }

    /*------------------------------------------------*
     *  Suppress noisy traces
     *------------------------------------------------*/
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER0), "machine", TRUE);
    gobj_set_gclass_no_trace(gclass_find_by_name(C_TIMER),  "machine", TRUE);
    gobj_set_global_no_trace("timer_periodic", TRUE);

    // Samples of traces: DON'T REMOVE
    // gobj_set_gclass_trace(gclass_find_by_name(C_AUTH_BFF), "messages", TRUE);
    // gobj_set_global_trace("machine", TRUE);

    set_auto_kill_time(60);

    /*
     *  The logs of the run, in order, with the priority of each
     */
    json_t *jn_expected = json_array();
    #define EXPECT_WARNING(text) \
        json_array_append_new(jn_expected, json_pack("{s:s, s:s}", \
            "msg", (text), "__priority__", "WARNING"))
    #define EXPECT_ERROR(text) \
        json_array_append_new(jn_expected, json_pack("{s:s, s:s}", \
            "msg", (text), "__priority__", "ERROR"))
    #define EXPECT_CAUSE(prio, text, cause) \
        json_array_append_new(jn_expected, json_pack("{s:s, s:s, s:s}", \
            "msg", (text), "__priority__", (prio), "cause", (cause)))
    /*  1: the discovery of C at the start of bff2 cannot connect, C is down  */
    EXPECT_CAUSE("ERROR", "👤BFF OIDC discovery: IdP did not answer in time",
        "idp_failover_connect_ms: the IdP could not be reached");
    EXPECT_CAUSE("WARNING", "👤BFF IdP down, the logins go to the next IdP",
        "the OIDC discovery failed");
    /*  2: the discovery of D at the start of bff3 has no answer, D is down  */
    EXPECT_CAUSE("ERROR", "👤BFF OIDC discovery: IdP did not answer in time",
        "idp_failover_response_ms: the IdP did not answer the discovery");
    EXPECT_CAUSE("WARNING", "👤BFF IdP down, the logins go to the next IdP",
        "the OIDC discovery failed");
    /*  6: A answers 502, the login passes to B  */
    EXPECT_WARNING("👤BFF IdP failed a login, it goes to the next IdP");
    EXPECT_CAUSE("WARNING", "👤BFF IdP down, the logins go to the next IdP",
        "a login answered 502/503/504");
    /*  8: a refresh of a session of A while A answers 502: as with one IdP  */
    EXPECT_ERROR("👤BFF IdP rejected the request");
    EXPECT_ERROR("👤BFF server error");
    /*  10: a refresh_token that is not a JWT goes to the first (said once)  */
    EXPECT_WARNING("👤BFF a refresh_token of no issuer of 'issuers' (or not a JWT): "
        "it goes to the first; said once");
    /*  11: a login A took and dropped is not sent to B  */
    EXPECT_ERROR("👤BFF server error");
    /*  12: A cannot be reached, the login passes to B  */
    EXPECT_CAUSE("ERROR", "👤BFF IdP outbound watchdog fired",
        "idp_failover_connect_ms: the IdP could not be reached");
    EXPECT_CAUSE("WARNING", "👤BFF IdP down, the logins go to the next IdP",
        "idp_failover_connect_ms: the IdP could not be reached");
    /*  13: A is back and answers 401: its answer, B not asked  */
    EXPECT_WARNING("👤BFF request rejected");
    /*  14: C tried again (bff2), its discovery cannot connect: no second "down"  */
    EXPECT_CAUSE("ERROR", "👤BFF OIDC discovery: IdP did not answer in time",
        "idp_failover_connect_ms: the IdP could not be reached");
    set_expected_results(
        APP_NAME,
        jn_expected,
        NULL,           // no JSON comparison
        NULL,           // no ignore_keys
        TRUE            // verbose
    );

    MT_START_TIME(time_measure)

    return res;
}

/***************************************************************************
 *  HACK: runs on yunetas environment BEFORE destroying the yuno
 ***************************************************************************/
static void cleaning(void)
{
    MT_INCREMENT_COUNT(time_measure, 1)
    MT_PRINT_TIME(time_measure, APP_NAME)

    result += test_json(NULL);  // check captured error log
    mock_keycloak_end_jwk();
    close_the_unreachable_idp();
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*------------------------------*
     *  Init logger
     *------------------------------*/
    glog_init();

    gobj_log_add_handler("stdout", "stdout", LOG_OPT_ALL, 0);

    /*
     *  Capture WARNING+ logs for test verification.
     *  Any unexpected log will fail the test.
     */
    gobj_log_register_handler(
        "testing",          // handler_name
        0,                  // close_fn
        capture_log_write,  // write_fn
        0                   // fwrite_fn
    );
    gobj_log_add_handler("test_capture", "testing", LOG_OPT_UP_WARNING, 0);

    /*------------------------------------------------*
     *      Memory leak check
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

    if(get_cur_system_memory() != 0) {
        printf("%sERROR --> %s%s\n", On_Red BWhite, "system memory not free", Color_Off);
        print_track_mem();
        result += -1;
    }

    return result;
}
