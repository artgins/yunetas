#pragma once

/*
 *
    {}  dict hook   (N unique childs)
    []  list hook   (n not-unique childs)
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
            │                           │
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
            │* pkey                     │
            │  pkey2s                   │
            │* system_flag              │
            │  tkey                     │
            │* topic_version            │
            │                           │
            │                           │
            │                   cols {} │ ◀─┐N
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
            │  hook                     │
            │  default                  │
            │  description              │
            │  properties               │
            │                           │
            │                           │
            │  _geometry                │
            └───────────────────────────┘

*/

static char treedb_system_schema[]= "\
{                                                                   \n\
    'id': 'treedb_system_schema',                                   \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'treedbs',                                        \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Treedb',                             \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'schema_version': {                                 \n\
                    'header': 'Schema Version',                     \n\
                    'fillspace': 10,                                \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'wild',                                     \n\
                        'writable',                                 \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'topics': {                                         \n\
                    'header': 'Topics',                             \n\
                    'fillspace': 10,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'topics': 'treedbs'                         \n\
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
            'id': 'topics',                                         \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Topic',                              \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'treedbs': {                                        \n\
                    'header': 'Treedbs',                            \n\
                    'fillspace': 10,                                \n\
                    'type': 'array',                                \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'pkey': {                                           \n\
                    'header': 'Primary Key',                        \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'default': 'id',                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'pkey2s': {                                         \n\
                    'header': 'Secondary Keys',                     \n\
                    'fillspace': 10,                                \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'system_flag': {                                    \n\
                    'header': 'System Flag',                        \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'default': 'sf_string_key',                     \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'tkey': {                                           \n\
                    'header': 'Time Key',                           \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'default': '',                                  \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'topic_version': {                                  \n\
                    'header': 'Topic Version',                      \n\
                    'fillspace': 10,                                \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'wild',                                     \n\
                        'writable',                                 \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'cols': {                                           \n\
                    'header': 'Columns',                            \n\
                    'fillspace': 10,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'cols': 'topics'                            \n\
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
            'id': 'cols',                                           \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'pkey2s': 'value',                                      \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'rowid',                              \n\
                    'fillspace': 4,                                 \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'rowid'                                     \n\
                    ]                                               \n\
                },                                                  \n\
                'value': {                                          \n\
                    'header': 'Column',                             \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'topics': {                                         \n\
                    'header': 'Topics',                             \n\
                    'fillspace': 10,                                \n\
                    'type': 'array',                                \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'header': {                                         \n\
                    'header': 'Header',                             \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'fillspace': {                                      \n\
                    'header': 'Fillspace',                          \n\
                    'fillspace': 3,                                 \n\
                    'type': 'integer',                              \n\
                    'default': 3,                                   \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'type': {                                           \n\
                    'header': 'Type',                               \n\
                    'fillspace': 5,                                 \n\
                    'type': 'string',                               \n\
                    'enum': [                                       \n\
                        'string',                                   \n\
                        'integer',                                  \n\
                        'object',                                   \n\
                        'dict',                                     \n\
                        'array',                                    \n\
                        'list',                                     \n\
                        'real',                                     \n\
                        'boolean',                                  \n\
                        'blob'                                      \n\
                    ],                                              \n\
                    'flag': [                                       \n\
                        'enum',                                     \n\
                        'required',                                 \n\
                        'persistent',                               \n\
                        'notnull',                                  \n\
                        'writable'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'flag': {                                           \n\
                    'header': 'Flag',                               \n\
                    'fillspace': 14,                                \n\
                    'type': 'array',                                \n\
                    'enum': [                                       \n\
                        'persistent',                               \n\
                        'required',                                 \n\
                        'fkey',                                     \n\
                        'enum',                                     \n\
                        'hook',                                     \n\
                        'uuid',                                     \n\
                        'notnull',                                  \n\
                        'wild',                                     \n\
                        'rowid',                                    \n\
                        'inherit',                                  \n\
                        'readable',                                 \n\
                        'writable',                                 \n\
                        'stats',                                    \n\
                        'rstats',                                   \n\
                        'pstats',                                   \n\
                        'password',                                 \n\
                        'email',                                    \n\
                        'url',                                      \n\
                        'time',                                     \n\
                        'color'                                     \n\
                    ],                                              \n\
                    'flag': [                                       \n\
                        'enum',                                     \n\
                        'persistent',                               \n\
                        'writable'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'hook': {                                           \n\
                    'header': 'Hook',                               \n\
                    'fillspace': 8,                                 \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'default': {                                        \n\
                    'header': 'Default',                            \n\
                    'fillspace': 8,                                 \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'description': {                                    \n\
                    'header': 'Description',                        \n\
                    'fillspace': 8,                                 \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'properties': {                                     \n\
                    'header': 'Properties',                         \n\
                    'fillspace': 8,                                 \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
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
