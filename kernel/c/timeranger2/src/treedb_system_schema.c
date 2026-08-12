/****************************************************************************
 *          TREEDB_SYSTEM_SCHEMA.C
 *
 *          Single source of truth for the treedb meta-schema.
 *          Consumed by tr_treedb (_treedb_create_topic_cols_desc) and by
 *          c_treedb (root-linux) to materialize the __system__ treedb.
 *
 *          HACK The 'cols' topic below is the STORAGE schema of a column
 *          definition, not the descriptor a user column is validated
 *          against. They differ in the primary key: a column name is only
 *          unique inside its topic, so 'cols' keys by rowid ('id') and
 *          carries the column name in 'value' (its pkey2). The validator
 *          used on user schemas is derived from this topic by
 *          _treedb_create_topic_cols_desc(), which renames 'value' back to
 *          'id' and drops the storage-only fields. Keep both in mind when
 *          adding a field: a field meant for user columns goes here and is
 *          picked up by the validator; a field that only makes sense in
 *          the __system__ treedb must be added to the skip list there.
 *
 *          Copyright (c) 2019 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
 ****************************************************************************/
#include "treedb_system_schema.h"

/*
 *
    {}  dict hook   (N unique children)
    []  list hook   (n not-unique children)
    (↖) 1 fkey      (1 parent)
    [↖] n fkeys     (n parents)
    {↖} N fkeys     (N parents) ???


    * field required
    = field inherited

                        treedbs
            ┌───────────────────────────┐
            │* id                       │
            │                           │
            │* schema_version           │
            │  c_schema_version         │
            │                           │
            │                 topics {} │ ◀─┐N
            │                           │   │
            │  _geometry                │   │
            └───────────────────────────┘   │
                                            │
                                            │
                       topics               │
            ┌───────────────────────────┐   │
            │* id                       │   │
            │                           │   │
            │               treedbs [↖] │ ──┘n
            │                           │
            │                   cols {} │ ◀─┐N
            │                           │   │
            │* pkey                     │   │
            │  pkey2s                   │   │
            │* system_flag              │   │
            │  tkey                     │   │
            │* topic_version            │   │
            │  system_topic             │   │
            │                           │   │
            │  _geometry                │   │
            └───────────────────────────┘   │
                                            │
                                            │
                       cols                 │
            ┌───────────────────────────┐   │
            │  id (rowid)               │   │
            │* value (2)                │   │
            │                           │   │
            │                topics [↖] │ ──┘n
            │                           │
            │                           │
            │* header                   │
            │  fillspace                │
            │* type                     │
            │  flag                     │
            │  enum                     │
            │  template                 │
            │  hook                     │
            │  default                  │
            │  description              │
            │  properties               │
            │                           │
            │                           │
            │  _geometry                │
            └───────────────────────────┘

*/

char treedb_system_schema[]= "\
{                                                       \n\
    'id': 'treedb_system_schema',                       \n\
    'schema_version': '10',                             \n\
    'topics': [                                         \n\
        {                                               \n\
            'id': 'treedbs',                            \n\
            'pkey': 'id',                               \n\
            'system_flag': 'sf_string_key',             \n\
            'topic_version': '3',                       \n\
            'system_topic': true,                       \n\
            'cols': {                                   \n\
                'id': {                                 \n\
                    'header': 'Treedb',                 \n\
                    'fillspace': 20,                    \n\
                    'type': 'string',                   \n\
                    'flag': [                           \n\
                        'persistent',                   \n\
                        'required'                      \n\
                    ]                                   \n\
                },                                      \n\
                'schema_version': {                     \n\
                    'header': 'Schema Version',         \n\
                    'fillspace': 10,                    \n\
                    'type': 'integer',                  \n\
                    'flag': [                           \n\
                        'wild',                         \n\
                        'writable',                     \n\
                        'persistent',                   \n\
                        'required'                      \n\
                    ]                                   \n\
                },                                      \n\
                'c_schema_version': {                   \n\
                    'header': 'From C Version',         \n\
                    'fillspace': 10,                    \n\
                    'type': 'integer',                  \n\
                    'flag': [                           \n\
                        'wild',                         \n\
                        'persistent'                    \n\
                    ]                                   \n\
                },                                      \n\
                'topics': {                             \n\
                    'header': 'Topics',                 \n\
                    'fillspace': 20,                    \n\
                    'type': 'dict',                     \n\
                    'flag': ['hook'],                   \n\
                    'hook': {                           \n\
                        'topics': 'treedbs'             \n\
                    }                                   \n\
                },                                      \n\
                '_geometry': {                          \n\
                    'header': 'Geometry',               \n\
                    'type': 'blob',                     \n\
                    'fillspace': 10,                    \n\
                    'flag': [                           \n\
                        'persistent'                    \n\
                    ]                                   \n\
                }                                       \n\
            }                                           \n\
        },                                              \n\
                                                        \n\
        {                                               \n\
            'id': 'topics',                             \n\
            'pkey': 'id',                               \n\
            'system_flag': 'sf_string_key',             \n\
            'topic_version': '4',                       \n\
            'system_topic': true,                       \n\
            'cols': {                                   \n\
                'id': {                                 \n\
                    'header': 'Topic',                  \n\
                    'fillspace': 20,                    \n\
                    'type': 'string',                   \n\
                    'flag': [                           \n\
                        'persistent',                   \n\
                        'required'                      \n\
                    ]                                   \n\
                },                                      \n\
                'treedbs': {                            \n\
                    'header': 'Treedbs',                \n\
                    'fillspace': 20,                    \n\
                    'type': 'array',                    \n\
                    'flag': [                           \n\
                        'fkey'                          \n\
                    ]                                   \n\
                },                                      \n\
                'cols': {                               \n\
                    'header': 'Columns',                \n\
                    'fillspace': 20,                    \n\
                    'type': 'dict',                     \n\
                    'flag': ['hook'],                   \n\
                    'hook': {                           \n\
                        'cols': 'topics'                \n\
                    }                                   \n\
                },                                      \n\
                'pkey': {                               \n\
                    'header': 'Primary Key',            \n\
                    'fillspace': 20,                    \n\
                    'type': 'string',                   \n\
                    'default': 'id',                    \n\
                    'flag': [                           \n\
                        'writable',                     \n\
                        'persistent',                   \n\
                        'required'                      \n\
                    ]                                   \n\
                },                                      \n\
                'pkey2s': {                             \n\
                    'header': 'Secondary Keys',         \n\
                    'fillspace': 20,                    \n\
                    'type': 'blob',                     \n\
                    'flag': [                           \n\
                        'writable',                     \n\
                        'persistent'                    \n\
                    ]                                   \n\
                },                                      \n\
                'system_flag': {                        \n\
                    'header': 'System Flag',            \n\
                    'fillspace': 20,                    \n\
                    'type': 'string',                   \n\
                    'default': 'sf_string_key',         \n\
                    'flag': [                           \n\
                        'writable',                     \n\
                        'persistent',                   \n\
                        'required'                      \n\
                    ]                                   \n\
                },                                      \n\
                'tkey': {                               \n\
                    'header': 'Time Key',               \n\
                    'fillspace': 10,                    \n\
                    'type': 'string',                   \n\
                    'default': '',                      \n\
                    'flag': [                           \n\
                        'writable',                     \n\
                        'persistent'                    \n\
                    ]                                   \n\
                },                                      \n\
                'topic_version': {                      \n\
                    'header': 'Topic Version',          \n\
                    'fillspace': 10,                    \n\
                    'type': 'integer',                  \n\
                    'flag': [                           \n\
                        'wild',                         \n\
                        'writable',                     \n\
                        'persistent',                   \n\
                        'required'                      \n\
                    ]                                   \n\
                },                                      \n\
                'system_topic': {                       \n\
                    'header': 'System Topic',           \n\
                    'fillspace': 6,                     \n\
                    'type': 'boolean',                  \n\
                    'flag': [                           \n\
                        'writable',                     \n\
                        'persistent'                    \n\
                    ]                                   \n\
                },                                      \n\
                '_geometry': {                          \n\
                    'header': 'Geometry',               \n\
                    'type': 'blob',                     \n\
                    'fillspace': 10,                    \n\
                    'flag': [                           \n\
                        'persistent'                    \n\
                    ]                                   \n\
                }                                       \n\
            }                                           \n\
        },                                              \n\
                                                        \n\
        {                                               \n\
            'id': 'cols',                               \n\
            'pkey': 'id',                               \n\
            'system_flag': 'sf_string_key',             \n\
            'topic_version': '7',                       \n\
            'system_topic': true,                       \n\
            'pkey2s': 'value',                          \n\
            'cols': {                                   \n\
                'id': {                                 \n\
                    'header': 'rowid',                  \n\
                    'fillspace': 4,                     \n\
                    'type': 'string',                   \n\
                    'flag': [                           \n\
                        'persistent',                   \n\
                        'rowid'                         \n\
                    ]                                   \n\
                },                                      \n\
                'value': {                              \n\
                    'header': 'Column',                 \n\
                    'fillspace': 10,                    \n\
                    'type': 'string',                   \n\
                    'flag': [                           \n\
                        'persistent',                   \n\
                        'required'                      \n\
                    ]                                   \n\
                },                                      \n\
                'topics': {                             \n\
                    'header': 'Topics',                 \n\
                    'fillspace': 20,                    \n\
                    'type': 'array',                    \n\
                    'flag': [                           \n\
                        'fkey'                          \n\
                    ]                                   \n\
                },                                      \n\
                'header': {                             \n\
                    'header': 'Header',                 \n\
                    'fillspace': 10,                    \n\
                    'type': 'string',                   \n\
                    'flag': [                           \n\
                        'required',                     \n\
                        'persistent',                   \n\
                        'writable'                      \n\
                    ]                                   \n\
                },                                      \n\
                'fillspace': {                          \n\
                    'header': 'Fillspace',              \n\
                    'fillspace': 3,                     \n\
                    'type': 'integer',                  \n\
                    'default': 10,                      \n\
                    'flag': [                           \n\
                        'writable',                     \n\
                        'persistent'                    \n\
                    ]                                   \n\
                },                                      \n\
                'type': {                               \n\
                    'header': 'Type',                   \n\
                    'fillspace': 5,                     \n\
                    'type': 'string',                   \n\
                    'enum': [                           \n\
                        'string',                       \n\
                        'integer',                      \n\
                        'object',                       \n\
                        'dict',                         \n\
                        'array',                        \n\
                        'list',                         \n\
                        'real',                         \n\
                        'boolean',                      \n\
                        'blob'                          \n\
                    ],                                  \n\
                    'flag': [                           \n\
                        'enum',                         \n\
                        'required',                     \n\
                        'persistent',                   \n\
                        'notnull',                      \n\
                        'writable'                      \n\
                    ]                                   \n\
                },                                      \n\
                'placeholder': {                        \n\
                    'header': 'Placeholder',            \n\
                    'fillspace': 10,                    \n\
                    'type': 'string',                   \n\
                    'flag': [                           \n\
                        'persistent',                   \n\
                        'writable'                      \n\
                    ]                                   \n\
                },                                      \n\
                'flag': {                               \n\
                    'header': 'Flag',                   \n\
                    'fillspace': 14,                    \n\
                    'type': 'array',                    \n\
                    'enum': [                           \n\
                        '',                             \n\
                        'persistent',                   \n\
                        'required',                     \n\
                        'notnull',                      \n\
                        'wild',                         \n\
                        'inherit',                      \n\
                        'readable',                     \n\
                        'writable',                     \n\
                        'hidden',                       \n\
                        'stats',                        \n\
                        'rstats',                       \n\
                        'pstats',                       \n\
                                                        \n\
                        'hook',                         \n\
                        'fkey',                         \n\
                        'enum',                         \n\
                                                        \n\
                        'template',                     \n\
                        'uuid',                         \n\
                        'rowid',                        \n\
                        'password',                     \n\
                        'email',                        \n\
                        'url',                          \n\
                        'time',                         \n\
                        'now',                          \n\
                        'date',                         \n\
                        'color',                        \n\
                        'image',                        \n\
                        'tel',                          \n\
                        'table',                        \n\
                        'id',                           \n\
                        'currency',                     \n\
                        'hex',                          \n\
                        'binary',                       \n\
                        'percent',                      \n\
                        'base64',                       \n\
                        'coordinates',                  \n\
                        'gbuffer'                       \n\
                    ],                                  \n\
                    'flag': [                           \n\
                        'enum',                         \n\
                        'persistent',                   \n\
                        'writable'                      \n\
                    ]                                   \n\
                },                                      \n\
                'enum': {                               \n\
                    'header': 'Enum',                   \n\
                    'fillspace': 10,                    \n\
                    'type': 'blob',                     \n\
                    'flag': [                           \n\
                        'writable',                     \n\
                        'persistent'                    \n\
                    ]                                   \n\
                },                                      \n\
                'template': {                           \n\
                    'header': 'Template',               \n\
                    'fillspace': 8,                     \n\
                    'type': 'blob',                     \n\
                    'flag': [                           \n\
                        'writable',                     \n\
                        'persistent'                    \n\
                    ]                                   \n\
                },                                      \n\
                'hook': {                               \n\
                    'header': 'Hook',                   \n\
                    'fillspace': 8,                     \n\
                    'type': 'blob',                     \n\
                    'flag': [                           \n\
                        'writable',                     \n\
                        'persistent'                    \n\
                    ]                                   \n\
                },                                      \n\
                'pkey2s': {                             \n\
                    'header': 'Secondary Keys',         \n\
                    'fillspace': 8,                     \n\
                    'type': 'blob',                     \n\
                    'flag': [                           \n\
                        'writable',                     \n\
                        'persistent'                    \n\
                    ]                                   \n\
                },                                      \n\
                'default': {                            \n\
                    'header': 'Default',                \n\
                    'fillspace': 8,                     \n\
                    'type': 'blob',                     \n\
                    'flag': [                           \n\
                        'writable',                     \n\
                        'persistent'                    \n\
                    ]                                   \n\
                },                                      \n\
                'description': {                        \n\
                    'header': 'Description',            \n\
                    'fillspace': 8,                     \n\
                    'type': 'string',                   \n\
                    'flag': [                           \n\
                        'writable',                     \n\
                        'persistent'                    \n\
                    ]                                   \n\
                },                                      \n\
                'properties': {                         \n\
                    'header': 'Properties',             \n\
                    'fillspace': 8,                     \n\
                    'type': 'blob',                     \n\
                    'flag': [                           \n\
                        'writable',                     \n\
                        'persistent'                    \n\
                    ]                                   \n\
                },                                      \n\
                '_geometry': {                          \n\
                    'header': 'Geometry',               \n\
                    'type': 'blob',                     \n\
                    'fillspace': 10,                    \n\
                    'flag': [                           \n\
                        'persistent'                    \n\
                    ]                                   \n\
                }                                       \n\
            }                                           \n\
        }                                               \n\
    ]                                                   \n\
}                                                       \n\
";
