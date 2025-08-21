#pragma once

/*

    {}  dict hook   (N unique childs)
    []  list hook   (n not-unique childs)
    (↖) 1 fkey      (1 parent)
    [↖] n fkeys     (n parents)
    {↖} N fkeys     (N parents) ???

    (2) pkey2 - secondary key
    *   field required
    =   field inherited



                        realms
            ┌───────────────────────────┐
            │* id (url)                 │
            │                           │
            │                 realms {} │ ◀─┐N
            │                           │   │
            │       parent_realm_id (↖) │ ──┘ 1
            │                           │
            │* realm_owner              │
            │* realm_role               │
            │* realm_name               │
            │* realm_env                │
            │  realm_disabled           │
            │* range_ports WARNING deprecated
            │* bind_ip                  │
            │  last_port   WARNING deprecated
            │                           │
            │                  yunos {} │ ◀─┐N
            │                           │   │
            │  _geometry                │   │
            └───────────────────────────┘   │
                                            │
                        yunos               │
            ┌───────────────────────────┐   │
            │* id                       │   │
            │                           │   │
            │              realm_id (↖) │ ──┘1
            │                           │
            │* yuno_role                │
            │* yuno_name                │
            │* yuno_release (2)         │
            │  yuno_tag                 │
            │  yuno_running             │
            │  yuno_playing             │
            │  yuno_pid                 │
            │  watcher_pid              │
            │  yuno_disabled            │
            │  must_play                │
            │* role_version             │
            │* name_version             │
            │  traced                   │
            │  yuno_multiple            │
            │  global                   │
            │* date                     │
            │  yuno_startdate           │
            │  _channel_gobj            │
            │  requester                │
            │  launch_id                │
            │                           │
            │         configurations {} │ ◀─────────┐N
            │                           │           │
            │                 binary () │ ◀─┐1      │
            │                           │   │       │
            │  _geometry                │   │       │
            └───────────────────────────┘   │       │
                                            │       │
                    binaries                │       │
            ┌───────────────────────────┐   │       │
            │* id (role)                │   │       │
            │                           │   │       │
            │                 yunos [↖] │ ──┘n      │
            │                           │           │
            │* version  (2)             │           │
            │  size                     │           │
            │* date                     │           │
            │  description              │           │
            │  tags                     │           │
            │  required_services        │           │
            │  public_services          │           │
            │  service_descriptor       │           │
            │* binary                   │           │
            │                           │           │
            │  _geometry                │           │
            └───────────────────────────┘           │
                                                    │
                    configurations                  │
            ┌───────────────────────────┐           │
            │* id (name)                │           │
            │                           │           │
            │                 yunos [↖] │ ──────────┘n
            │                           │
            │* version  (2)             │
            │* date                     │
            │  description              │
            │  zcontent                 │
            │                           │
            │  _geometry                │
            └───────────────────────────┘

                    public_services
            ┌───────────────────────────┐
            │* id    (service)          │
            │                           │
            │* service                  │
            │* realm_id                 │
            │  description              │
            │* yuno_role                │
            │* yuno_name                │
            │  yuno_id                  │
            │  ip                       │
            │  port                     │
            │* schema                   │
            │  url                      │
            │  version                  │
            │  conector                 │
            │                           │
            │  _geometry                │
            └───────────────────────────┘


                        ┌───────────────┐
                        │     realms    │
                        └───────────────┘
                                ▲ n (hook 'yunos')
                                ┃
                                ┃
                                ▼ 1 (fkey 'realm_id')
                ┌───────────────────────────────────────┐
                │               yunos                   │
                └───────────────────────────────────────┘
                        ▲ 1 (hook 'binary')     ▲ n (hook 'configurations')
                        ┃                       ┃
                        ┃                       ┃
                        ▼ n ('fkey yunos')      ▼ n (fkey 'yunos')
                ┌────────────────┐      ┌────────────────┐
                │   binaries     │      │ configurations │
                └────────────────┘      └────────────────┘

*/

static char treedb_schema_yuneta_agent[]= "\
{                                                                   \n\
    'id': 'treedb_yuneta_agent',                                    \n\
    'schema_version': '22',                                         \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'realms',                                         \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '7',                                   \n\
            'pkey2s': '',                                           \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'id',                                 \n\
                    'fillspace': 30,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'realms': {                                         \n\
                    'header': 'Realms',                             \n\
                    'fillspace': 10,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'realms': 'parent_realm_id'                 \n\
                    }                                               \n\
                },                                                  \n\
                'parent_realm_id': {                                \n\
                    'header': 'Realm Parent',                       \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'realm_owner': {                                    \n\
                    'header': 'realm_owner',                        \n\
                    'fillspace': 15,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'realm_role': {                                     \n\
                    'header': 'realm_role',                         \n\
                    'fillspace': 15,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'realm_name': {                                     \n\
                    'header': 'realm_name',                         \n\
                    'fillspace': 15,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'realm_env': {                                      \n\
                    'header': 'realm_env',                          \n\
                    'fillspace': 9,                                 \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'realm_disabled': {                                 \n\
                    'header': 'realm_disabled',                     \n\
                    'fillspace': 8,                                 \n\
                    'type': 'boolean',                              \n\
                    'flag': [                                       \n\
                        'inherit',                                  \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'range_ports': {                                    \n\
                    'header': 'range_ports',                        \n\
                    'fillspace': 0,                                 \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                'bind_ip': {                                        \n\
                    'header': 'bind_ip',                            \n\
                    'fillspace': 15,                                \n\
                    'type': 'string',                               \n\
                    'default': '127.0.0.1',                         \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'last_port': {                                      \n\
                    'header': 'last_port',                          \n\
                    'fillspace': 0,                                 \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                'yunos': {                                          \n\
                    'header': 'yunos',                              \n\
                    'fillspace': 22,                                \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'yunos': 'realm_id'                         \n\
                    }                                               \n\
                },                                                  \n\
                '_geometry': {                                      \n\
                    'header': 'Geometry',                           \n\
                    'type': 'blob',                                 \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
                                                                    \n\
        {                                                           \n\
            'id': 'yunos',                                          \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '19',                                  \n\
            'tkey': '',                                             \n\
            'pkey2s': 'yuno_release',                               \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'id',                                 \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required',                                 \n\
                        'rowid'                                     \n\
                    ]                                               \n\
                },                                                  \n\
                'realm_id': {                                       \n\
                    'header': 'realm_id',                           \n\
                    'fillspace': 30,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno_role': {                                      \n\
                    'header': 'yuno_role',                          \n\
                    'fillspace': 16,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno_name': {                                      \n\
                    'header': 'yuno_name',                          \n\
                    'fillspace': 22,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno_release': {                                   \n\
                    'header': 'yuno_release',                       \n\
                    'fillspace': 12,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno_tag': {                                       \n\
                    'header': 'yuno_tag',                           \n\
                    'fillspace': 12,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno_running': {                                   \n\
                    'header': 'running',                            \n\
                    'fillspace': 5,                                 \n\
                    'type': 'boolean',                              \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno_playing': {                                   \n\
                    'header': 'playing',                            \n\
                    'fillspace': 5,                                 \n\
                    'type': 'boolean',                              \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno_pid': {                                       \n\
                    'header': 'pid',                                \n\
                    'fillspace': 7,                                 \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                'watcher_pid': {                                    \n\
                    'header': 'w_pid',                              \n\
                    'fillspace': 7,                                 \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno_disabled': {                                  \n\
                    'header': 'disabled',                           \n\
                    'fillspace': 6,                                 \n\
                    'type': 'boolean',                              \n\
                    'flag': [                                       \n\
                        'inherit',                                  \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'must_play': {                                      \n\
                    'header': 'must_play',                          \n\
                    'fillspace': 8,                                 \n\
                    'type': 'boolean',                              \n\
                    'flag': [                                       \n\
                        'inherit',                                  \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'role_version': {                                   \n\
                    'header': 'role_version',                       \n\
                    'fillspace': 16,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'name_version': {                                   \n\
                    'header': 'name_version',                       \n\
                    'fillspace': 16,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'traced': {                                         \n\
                    'header': 'traced',                             \n\
                    'fillspace': 6,                                 \n\
                    'type': 'boolean',                              \n\
                    'flag': [                                       \n\
                        'inherit',                                  \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno_multiple': {                                  \n\
                    'header': 'yuno_multiple',                      \n\
                    'fillspace': 6,                                 \n\
                    'type': 'boolean',                              \n\
                    'flag': [                                       \n\
                        'inherit',                                  \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'global': {                                         \n\
                    'header': 'global',                             \n\
                    'fillspace': 6,                                 \n\
                    'type': 'boolean',                              \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'date': {                                           \n\
                    'header': 'date',                               \n\
                    'fillspace': 22,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno_startdate': {                                 \n\
                    'header': 'yuno_startdate',                     \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                '_channel_gobj': {                                  \n\
                    'header': '_channel_gobj',                      \n\
                    'fillspace': 6,                                 \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                '_requester': {                                     \n\
                    'header': '_requester',                         \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                '_requester_md_iev': {                              \n\
                    'header': '_requester_md_iev',                  \n\
                    'fillspace': 10,                                \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                'launch_id': {                                      \n\
                    'header': 'launch_id',                          \n\
                    'fillspace': 10,                                \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                'configurations': {                                 \n\
                    'header': 'configurations',                     \n\
                    'fillspace': 15,                                \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'configurations': 'yunos'                   \n\
                    }                                               \n\
                },                                                  \n\
                'binary': {                                         \n\
                    'header': 'binary',                             \n\
                    'fillspace': 8,                                 \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'binaries': 'yunos'                         \n\
                    }                                               \n\
                },                                                  \n\
                '_geometry': {                                      \n\
                    'header': 'Geometry',                           \n\
                    'type': 'blob',                                 \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
                                                                    \n\
        {                                                           \n\
            'id': 'binaries',                                       \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '3',                                   \n\
            'tkey': '',                                             \n\
            'pkey2s': 'version',                                    \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'id',                                 \n\
                    'fillspace': 18,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'version': {                                        \n\
                    'header': 'version',                            \n\
                    'fillspace': 14,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'size': {                                           \n\
                    'header': 'size',                               \n\
                    'fillspace': 10,                                \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'date': {                                           \n\
                    'header': 'date',                               \n\
                    'fillspace': 22,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'description': {                                    \n\
                    'header': 'description',                        \n\
                    'fillspace': 22,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'tags': {                                           \n\
                    'header': 'tags',                               \n\
                    'fillspace': 22,                                \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'required_services': {                              \n\
                    'header': 'required_services',                  \n\
                    'fillspace': 22,                                \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'public_services': {                                \n\
                    'header': 'public_services',                    \n\
                    'fillspace': 22,                                \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'service_descriptor': {                             \n\
                    'header': 'service_descriptor',                 \n\
                    'fillspace': 22,                                \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'binary': {                                         \n\
                    'header': 'binary',                             \n\
                    'fillspace': 22,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'yunos': {                                          \n\
                    'header': 'yunos',                              \n\
                    'fillspace': 22,                                \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                '_geometry': {                                      \n\
                    'header': 'Geometry',                           \n\
                    'type': 'blob',                                 \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
                                                                    \n\
        {                                                           \n\
            'id': 'configurations',                                 \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '3',                                   \n\
            'tkey': '',                                             \n\
            'pkey2s': 'version',                                    \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'id',                                 \n\
                    'fillspace': 30,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required',                                 \n\
                        'rowid'                                     \n\
                    ]                                               \n\
                },                                                  \n\
                'version': {                                        \n\
                    'header': 'version',                            \n\
                    'fillspace': 14,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'date': {                                           \n\
                    'header': 'date',                               \n\
                    'fillspace': 22,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'description': {                                    \n\
                    'header': 'description',                        \n\
                    'fillspace': 30,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'yunos': {                                          \n\
                    'header': 'yunos',                              \n\
                    'fillspace': 22,                                \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'zcontent': {                                       \n\
                    'header': 'zcontent',                           \n\
                    'fillspace': 35,                                \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                '_geometry': {                                      \n\
                    'header': 'Geometry',                           \n\
                    'type': 'blob',                                 \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
                                                                    \n\
        {                                                           \n\
            'id': 'public_services',                                \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '3',                                   \n\
            'pkey2s': 'version',                                    \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'id',                                 \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required',                                 \n\
                        'rowid'                                     \n\
                    ]                                               \n\
                },                                                  \n\
                'service': {                                        \n\
                    'header': 'service',                            \n\
                    'fillspace': 18,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'realm_id': {                                       \n\
                    'header': 'realm_id',                           \n\
                    'fillspace': 30,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'description': {                                    \n\
                    'header': 'description',                        \n\
                    'fillspace': 18,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno_role': {                                      \n\
                    'header': 'yuno_role',                          \n\
                    'fillspace': 18,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno_name': {                                      \n\
                    'header': 'yuno_name',                          \n\
                    'fillspace': 18,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno_id': {                                        \n\
                    'header': 'yuno_id',                            \n\
                    'fillspace': 8,                                 \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'ip': {                                             \n\
                    'header': 'ip',                                 \n\
                    'fillspace': 16,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'port': {                                           \n\
                    'header': 'port',                               \n\
                    'fillspace': 5,                                 \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'schema': {                                         \n\
                    'header': 'schema',                             \n\
                    'fillspace': 6,                                 \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'url': {                                            \n\
                    'header': 'url',                                \n\
                    'fillspace': 22,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'version': {                                        \n\
                    'header': 'version',                            \n\
                    'fillspace': 14,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'connector': {                                      \n\
                    'header': 'connector',                          \n\
                    'fillspace': 12,                                \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                '_geometry': {                                      \n\
                    'header': 'Geometry',                           \n\
                    'type': 'blob',                                 \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";
