#pragma once

/* ◀▲▼▶

    ()  string hook (1 unique children)
    {}  dict hook   (N unique children)
    []  list hook   (n not-unique children)
    (↖) 1 fkey      (1 parent)
    [↖] n fkeys     (n parents)
    {↖} N fkeys     (N parents) ???


    * field required
    = field inherited

                    device_groups
            ┌───────────────────────────┐
            │* id                       │
            │                           │
            │         device_groups {}  │ ◀─┐N
            │                           │   │
            │         group_parent (↖)  │ ──┘ 1
            │                           │
            │  description              │
            │  icon                     │
            │  properties               │
            │  enabled                  │
            │  coordinates              │
            │  language                 │
            │  cluster                  │
            │                           │
            │               managers {} │ ◀─────────────────┐N
            │                devices {} │ ◀─┐N              │
            │                           │   │               │
            │                           │   │               │
            │  _geometry                │   │               │
            └───────────────────────────┘   │               │
                                            │               │
                                            │               │
                                            │               │
                        devices             │               │
            ┌───────────────────────────┐   │               │
            │* id                       │   │               │
            │                           │   │n              │
            │         device_groups [↖] │ ──┘               │
            │                           │                   │
            │  name                     │                   │
            │  description              │                   │
            │  enabled                  │                   │
            │  coordinates              │                   │       TODO users of templates:
            │  settings             <------------------------- using device_types.template_settings
            │  properties               │                   │       TODO    make a shortcut to get it
            │  time                     │                   │
            │  yuno                     │                   │
            │                           │                   │
            │           device_type [↖] │ ──┐ n             │
            │                           │   │               │
            │  _geometry                │   │               │
            └───────────────────────────┘   │               │
                                            │               │
                      device_types          │               │
            ┌───────────────────────────┐   │               │
            │* id                       │   │               │
            │                           │   │               │
            │                devices {} │ ◀─┘ N             │
            │  name                     │                   │
            │  description              │                   │
            │  icon                     │                   │
            │  properties               │                   │
            │  template_settings        │                   │
            │                           │                   │
            │  _geometry                │                   │
            └───────────────────────────┘                   │
                                                            │
                                                            │
                        users                               │
            ┌───────────────────────────┐                   │
            │* id                       │                   │
            │                           │                   │
            │         user_groups [↖]   │ ──────────────────┘
            │                           │ n
            │                           │
            │  enabled                  │
            │  language                 │
            │  persistent_attrs         │
            │  properties               │
            │  time                     │
            │                           │
            │  _geometry                │
            └───────────────────────────┘

            alarms
            ┌───────────────────────────┐
            │  id                       │   device_id
            │  tm                       │
            │  alarm                    │
            │  description              │
            │  triggers                 │
            │       measure             │
            │       condition           │
            │       value               │
            │  sent                     │
            │  validated                │
            └───────────────────────────┘

*/

static char treedb_schema_mqtt_broker[]= "\
{                                                                   \n\
    'id': 'treedb_mqtt_broker',                                     \n\
    'schema_version': '1',                                         	\n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'device_groups',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Device Group',                       \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'device_groups': {                                  \n\
                    'header': 'Groups',                             \n\
                    'type': 'object',                               \n\
                    'flag': [                                       \n\
                        'hook'                                      \n\
                    ],                                              \n\
                    'hook': {                                       \n\
                        'device_groups': 'group_parent'             \n\
                    }                                               \n\
                },                                                  \n\
                'group_parent': {                                   \n\
                    'header': 'Parent Group',                       \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'description': {                                    \n\
                    'header': 'Description',                        \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'icon': {                                           \n\
                    'header': 'Icon',                               \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'image',                                    \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'properties': {                                     \n\
                    'header': 'Properties',                         \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'enabled': {                                        \n\
                    'header': 'enabled',                            \n\
                    'type': 'boolean',                              \n\
                    'default': true,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'coordinates': {                                \n\
                    'header': 'Coordinates',                    \n\
                    'type': 'dict',                             \n\
                    'default': {                                \n\
                        'geometry': {                           \n\
                            'type': 'Point',                    \n\
                            'coordinates': [0, 0]               \n\
                        }                                       \n\
                    },                                          \n\
                    'flag': [                                   \n\
                        'persistent',                           \n\
                        'writable',                             \n\
                        'coordinates'                           \n\
                    ]                                           \n\
                },                                              \n\
                'language': {                   \n\
                    'header': 'Language',       \n\
                    'type': 'string',           \n\
                    'flag': [                   \n\
                        'writable',             \n\
                        'persistent',           \n\
                        'required',             \n\
                        'enum'                  \n\
                    ],                          \n\
                    'enum': [                   \n\
                        'es',                   \n\
                        'en'                    \n\
                    ]                           \n\
                },                              \n\
                'cluster': {                                        \n\
                    'header': 'cluster',                            \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'devices': {                                        \n\
                    'header': 'Devices',                            \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'devices': 'device_groups'                  \n\
                    }                                               \n\
                },                                                  \n\
                'managers': {                                       \n\
                    'header': 'Managers',                           \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'users': 'user_groups'                      \n\
                    }                                               \n\
                },                                                  \n\
                '_geometry': {                                      \n\
                    'header': 'Geometry',                           \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
                                                                    \n\
        {                                                           \n\
            'id': 'devices',                                        \n\
            'pkey': 'id',                                           \n\
            'tkey': 'tm',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Device',                             \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'device_groups': {                                  \n\
                    'header': 'Device Groups',                      \n\
                    'type': 'array',                                \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'description': {                                    \n\
                    'header': 'Description',                        \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'enabled': {                                        \n\
                    'header': 'Enabled',                            \n\
                    'type': 'boolean',                              \n\
                    'default': true,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'coordinates': {                                \n\
                    'header': 'Coordinates',                    \n\
                    'type': 'dict',                             \n\
                    'default': {                                \n\
                        'geometry': {                           \n\
                            'type': 'Point',                    \n\
                            'coordinates': [0, 0]               \n\
                        }                                       \n\
                    },                                          \n\
                    'flag': [                                   \n\
                        'persistent',                           \n\
                        'writable',                             \n\
                        'coordinates'                           \n\
                    ]                                           \n\
                },                                              \n\
                'device_type': {                                    \n\
                    'header': 'Type',                               \n\
                    'type': 'array',                                \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'settings': {                                       \n\
                    'header': 'Device Settings',                    \n\
                    'type': 'dict',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'properties': {                                     \n\
                    'header': 'Properties',                         \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'tm': {                                             \n\
                    'header': 'Update Time',                        \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'now',                                      \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'connected': {                                      \n\
                    'header': 'Connected',                          \n\
                    'type': 'boolean',                              \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno': {                                           \n\
                    'header': 'Yuno',                               \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                '_geometry': {                                      \n\
                    'header': 'Geometry',                           \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
                                                                    \n\
        {                                                           \n\
            'id': 'device_types',                                   \n\
            'pkey': 'id',                                           \n\
            'tkey': 'tm',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Type',                               \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'devices': {                                        \n\
                    'header': 'Devices',                            \n\
                    'type': 'object',                               \n\
                    'flag': [                                       \n\
                        'hook'                                      \n\
                    ],                                              \n\
                    'hook': {                                       \n\
                        'devices': 'device_type'                    \n\
                    }                                               \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'description': {                                    \n\
                    'header': 'Description',                        \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'icon': {                                           \n\
                    'header': 'Icon',                               \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'image',                                    \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'properties': {                                     \n\
                    'header': 'Properties',                         \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'template_settings': {                              \n\
                    'header': 'Template Settings',                  \n\
                    'type': 'dict',                                 \n\
                    'flag': [                                       \n\
                        'template',                                 \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ],                                              \n\
                    'template': {                                   \n\
                        'template_version': {                           \n\
                            'type': 'integer',                          \n\
                            'default': 7,                               \n\
                            'flag': [                                   \n\
                                'hidden',                               \n\
                                'persistent'                            \n\
                            ]                                           \n\
                        },                                              \n\
                        'top_measures': {                               \n\
                            'type': 'array',                            \n\
                            'default': ['temperature'],                 \n\
                            'flag': [                                   \n\
                                'persistent',                           \n\
                                'writable',                             \n\
                                'enum'                                  \n\
                            ],                                          \n\
                            'placeholder': 'select a measure',          \n\
                            'enum': '$device_measures'                  \n\
                        },                                              \n\
                        'coordinates': {                                \n\
                            'type': 'dict',                             \n\
                            'default': {                                \n\
                                'geometry': {                           \n\
                                    'type': 'Point',                    \n\
                                    'coordinates': [0, 0]               \n\
                                }                                       \n\
                            },                                          \n\
                            'flag': [                                   \n\
                                'persistent',                           \n\
                                'writable',                             \n\
                                'coordinates'                           \n\
                            ]                                           \n\
                        },                                              \n\
                        'alarm_colors': {                               \n\
                            'type': 'object',                           \n\
                            'flag': [                                   \n\
                                'writable',                             \n\
                                'persistent',                           \n\
                                'template'                              \n\
                            ],                                          \n\
                            'template': {                               \n\
                                'active_not_validated': {                       \n\
                                    'type': 'string',                           \n\
                                    'default': 'red',                           \n\
                                    'flag': [                                   \n\
                                        'writable',                             \n\
                                        'persistent',                           \n\
                                        'color'                                 \n\
                                    ]                                           \n\
                                },                                              \n\
                                'active_validated': {                           \n\
                                    'type': 'string',                           \n\
                                    'default': '#e5a50a',                       \n\
                                    'flag': [                                   \n\
                                        'writable',                             \n\
                                        'persistent',                           \n\
                                        'color'                                 \n\
                                    ]                                           \n\
                                },                                              \n\
                                'not_active_not_validated': {                   \n\
                                    'type': 'string',                           \n\
                                    'default': 'Fuchsia',                       \n\
                                    'flag': [                                   \n\
                                        'writable',                             \n\
                                        'persistent',                           \n\
                                        'color'                                 \n\
                                    ]                                           \n\
                                }                                               \n\
                            }                                           \n\
                        },                                              \n\
                        'alarms': {                                     \n\
                            'type': 'array',                            \n\
                            'flag': [                                   \n\
                                'persistent',                           \n\
                                'writable',                             \n\
                                'table'                                 \n\
                            ],                                          \n\
                            'table': {                                  \n\
                                'id': {                                 \n\
                                    'header': 'alarm',                  \n\
                                    'type': 'id',                       \n\
                                    'flag': [                           \n\
                                        'persistent',                   \n\
                                        'writable',                     \n\
                                        'required'                      \n\
                                    ]                                   \n\
                                },                                      \n\
                                'description': {                        \n\
                                    'type': 'string',                   \n\
                                    'flag': [                           \n\
                                        'writable',                     \n\
                                        'persistent'                    \n\
                                    ]                                   \n\
                                },                                      \n\
                                'notifications': {                      \n\
                                    'type': 'string',                   \n\
                                    'flag': [                           \n\
                                        'writable',                     \n\
                                        'persistent',                   \n\
                                        'tel',                          \n\
                                        'email'                         \n\
                                    ]                                   \n\
                                },                                      \n\
                                'triggers': {                           \n\
                                    'type': 'array',                    \n\
                                    'flag': [                           \n\
                                        'writable',                     \n\
                                        'persistent',                   \n\
                                        'table'                         \n\
                                    ],                                  \n\
                                    'table': {                          \n\
                                        'id': {                         \n\
                                            'header': 'measure',        \n\
                                            'type': 'string',           \n\
                                            'flag': [                   \n\
                                                'writable',             \n\
                                                'persistent',           \n\
                                                'required',             \n\
                                                'id',                   \n\
                                                'enum'                  \n\
                                            ],                          \n\
                                            'placeholder': 'select a measure', \n\
                                            'enum': '$device_measures'  \n\
                                        },                              \n\
                                        'operation': {                  \n\
                                            'type': 'string',           \n\
                                            'flag': [                   \n\
                                                'writable',             \n\
                                                'persistent',           \n\
                                                'required',             \n\
                                                'enum'                  \n\
                                            ],                          \n\
                                            'enum': [                   \n\
                                                '>',                    \n\
                                                '>=',                   \n\
                                                '<',                    \n\
                                                '<=',                   \n\
                                                '=',                    \n\
                                                '≠',                    \n\
                                                '~'                     \n\
                                            ]                           \n\
                                        },                              \n\
                                        'value': {                      \n\
                                            'type': 'string',           \n\
                                            'flag': [                   \n\
                                                'writable',             \n\
                                                'persistent',           \n\
                                                'required'              \n\
                                            ]                           \n\
                                        }                               \n\
                                    }                                   \n\
                                },                                      \n\
                                'enabled': {                            \n\
                                    'type': 'boolean',                  \n\
                                    'default': true,                    \n\
                                    'flag': [                           \n\
                                        'writable',                     \n\
                                        'persistent'                    \n\
                                    ]                                   \n\
                                },                                      \n\
                                'max_age': {                            \n\
                                    'type': 'integer',                  \n\
                                    'default': '60m',                   \n\
                                    'flag': [                           \n\
                                        'writable',                     \n\
                                        'persistent'                    \n\
                                    ]                                   \n\
                                }                                       \n\
                            }                                           \n\
                        }                                           \n\
                    }                                               \n\
                },                                                  \n\
                'tm': {                                             \n\
                    'header': 'Update Time',                        \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'now',                                      \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                '_geometry': {                                      \n\
                    'header': 'Geometry',                           \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
                                                                    \n\
        {                                                           \n\
            'id': 'users',                                          \n\
            'pkey': 'id',                                           \n\
            'tkey': 'tm',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'User',                               \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'user_groups': {                                    \n\
                    'header': 'Manager',                            \n\
                    'type': 'array',                                \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'enabled': {                                        \n\
                    'header': 'Enabled',                            \n\
                    'type': 'boolean',                              \n\
                    'default': true,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'language': {                   \n\
                    'header': 'Language',       \n\
                    'type': 'string',           \n\
                    'default': 'es',            \n\
                    'flag': [                   \n\
                        'writable',             \n\
                        'persistent',           \n\
                        'required',             \n\
                        'enum'                  \n\
                    ],                          \n\
                    'enum': [                   \n\
                        'es',                   \n\
                        'en'                    \n\
                    ]                           \n\
                },                              \n\
                'persistent_attrs': {                               \n\
                    'header': 'Persistent Attrs',                   \n\
                    'type': 'dict',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'properties': {                                     \n\
                    'header': 'Properties',                         \n\
                    'type': 'dict',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'tm': {                                             \n\
                    'header': 'Update Time',                        \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'now',                                      \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                '_geometry': {                                      \n\
                    'header': 'Geometry',                           \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";

static char msg2db_schema_alarms[]= "\
{                                                                   \n\
    'id': 'msg2db_alarms',                                          \n\
    'schema_version': '1',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'alarms',                                         \n\
            'pkey': 'id',                                           \n\
            'tkey': 'tm',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'pkey2': 'alarm',                                       \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Device Id',                          \n\
                    'type': 'string',                               \n\
                    'fillspace': 2,                                 \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'tm': {                                             \n\
                    'header': 'Time',                               \n\
                    'fillspace': 2,                                 \n\
                    'type': 'integer',                              \n\
                    'flag': ['persistent','required','time']        \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Device Name',                        \n\
                    'fillspace': 1,                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'alarm': {                                          \n\
                    'header': 'Alarm',                              \n\
                    'fillspace': 2,                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'active': {                                         \n\
                    'header': 'Active',                             \n\
                    'fillspace': 1,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'validated': {                                      \n\
                    'header': 'Validated',                          \n\
                    'fillspace': 1,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
                    'flag': ['persistent','writable']               \n\
                },                                                  \n\
                                                                    \n\
                'description': {                                    \n\
                    'header': 'Description',                        \n\
                    'fillspace': 3,                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'triggers': {                                       \n\
                    'header': 'Triggers',                           \n\
                    'fillspace': 1,                                 \n\
                    'type': 'array',                                \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                '_old_triggers': {                                  \n\
                    'header': 'Old Triggers',                       \n\
                    'type': 'array',                                \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'sent': {                                           \n\
                    'header': 'Sent',                               \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'notified': {                                       \n\
                    'header': 'Notified',                           \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                '_yuno': {                                          \n\
                    'header': 'Yuno',                               \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                '_device_type': {                                   \n\
                    'header': 'Device Type',                        \n\
                    'type': 'array',                                \n\
                    'flag': ['persistent']                          \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";
