#pragma once

/* ◀▲▼▶
 *  TreeDB Schema for MQTT Broker Model
 *
 *  Design principle:
 *  - `clients`: persistent client identity and settings (survives sessions)
 *  - `sessions`: session-specific data, deleted on clean_start=true
 *  - `retained_msgs`: retained messages by topic
 *
 *  In-memory subscription tree is built separately from session data at startup.
 *

    ()  string hook (1 unique children)
    {}  dict hook   (N unique children)
    []  list hook   (n not-unique children)
    (↖) 1 fkey      (1 parent)
    [↖] n fkeys     (n parents)
    {↖} N fkeys     (N parents) ???

    * field required
    = field inherited

                    client_groups
            ┌───────────────────────────┐
            │* id                       │
            │                           │
            │         client_groups {}  │ ◀─┐N
            │                           │   │
            │         group_parent (↖)  │ ──┘ 1
            │                           │
            │  description              │
            │  icon                     │
            │  enabled                  │
            │  time                     │
            │  language                 │
            │  cluster                  │
            │  properties               │
            │  coordinates              │
            │                           │
            │               managers {} │ ◀─────────────────┐N
            │                clients {} │ ◀─┐N              │
            │                           │   │               │
            │                           │   │               │
            │  _geometry                │   │               │
            └───────────────────────────┘   │               │
                                            │               │
                                            │               │
                                            │               │
                        clients             │               │
            ┌───────────────────────────┐   │               │
            │* id (client_id)           │   │               │
            │                           │   │n              │
            │         client_groups [↖] │ ──┘               │
            │                           │                   │
            │  name                     │                   │
            │  description              │                   │
            │  enabled                  │                   │
            │  max_sessions             │                   │
            │  time                     │                   │
            │           client_type [↖] │ ──┐ n     │       │
            │  properties               │   │               │
            │  coordinates              │   │               │
            │  settings             <----------------- using client_types.template_settings
            │  yuno                     │   │               │    TODO    make a shortcut to get it
            │                           │   │               │
            │              sessions {}  │ ◀─────────┐ N     │
            │                           │   │       │       │
            │                           │   │       │       │
            │  _geometry                │   │       │       │
            └───────────────────────────┘   │       │       │
                                            │       │       │
                      client_types          │       │       │
            ┌───────────────────────────┐   │       │       │
            │* id                       │   │       │       │
            │                           │   │       │       │
            │                clients {} │ ◀─┘ N     │       │
            │  name                     │           │       │
            │  description              │           │       │
            │  icon                     │           │       │
            │  properties               │           │       │
            │  time                     │           │       │
            │  template_settings        │           │       │
            │                           │           │       │
            │  _geometry                │           │       │
            └───────────────────────────┘           │       │
                                                    │       │
                       sessions                     │       │
            ┌───────────────────────────┐           │       │
            │* id (session_id)          │           │       │
            │                           │           │       │
            │              client_id (↖)│ ──────────┘1      │
            │                           │                   │
            │  protocol_version         │                   │
            │  clean_start              │                   │
            │  keep_alive               │                   │
            │  session_expiry_interval  │                   │
            │  connected                │                   │
            │  time                     │                   │
            │                           │                   │
            │  subscriptions            │  (json array)     │
            │                           │                   │
            │  will_topic               │                   │
            │  will_payload             │                   │
            │  will_qos                 │                   │
            │  will_retain              │                   │
            │  will_delay_interval      │                   │
            │  will_properties          │                   │
            │                           │                   │
            │  inflight_msgs            │  (json array - QoS 1/2 pending)
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


                    retained_msgs
            ┌───────────────────────────┐
            │* id (topic)               │
            │                           │
            │  payload                  │
            │  qos                      │
            │  time                     │
            │  message_expiry_interval  │
            │  payload_format_indicator │
            │  content_type             │
            │  response_topic           │
            │  correlation_data         │
            │  user_properties          │
            │                           │
            │  _geometry                │
            └───────────────────────────┘


            alarms
            ┌───────────────────────────┐
            │  id                       │   client_id
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
            'id': 'client_groups',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Client Group',                       \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'client_groups': {                                  \n\
                    'header': 'Groups',                             \n\
                    'type': 'object',                               \n\
                    'flag': [                                       \n\
                        'hook'                                      \n\
                    ],                                              \n\
                    'hook': {                                       \n\
                        'client_groups': 'group_parent'             \n\
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
                'enabled': {                                        \n\
                    'header': 'enabled',                            \n\
                    'type': 'boolean',                              \n\
                    'default': true,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'time': {                                           \n\
                    'header': 'Update Time',                        \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'now',                                      \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'language': {                                       \n\
                    'header': 'Language',                           \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent',                               \n\
                        'required',                                 \n\
                        'enum'                                      \n\
                    ],                                              \n\
                    'enum': [                                       \n\
                        'es',                                       \n\
                        'en'                                        \n\
                    ]                                               \n\
                },                                                  \n\
                'cluster': {                                        \n\
                    'header': 'cluster',                            \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
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
                'coordinates': {                                    \n\
                    'header': 'Coordinates',                        \n\
                    'type': 'dict',                                 \n\
                    'default': {                                    \n\
                        'geometry': {                               \n\
                            'type': 'Point',                        \n\
                            'coordinates': [0, 0]                   \n\
                        }                                           \n\
                    },                                              \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'writable',                                 \n\
                        'coordinates'                               \n\
                    ]                                               \n\
                },                                                  \n\
                'managers': {                                       \n\
                    'header': 'Managers',                           \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'users': 'user_groups'                      \n\
                    }                                               \n\
                },                                                  \n\
                'clients': {                                        \n\
                    'header': 'Clients',                            \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'clients': 'client_groups'                  \n\
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
            'id': 'clients',                                        \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Client Id',                          \n\
                    'fillspace': 40,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'client_groups': {                                  \n\
                    'header': 'Client Groups',                      \n\
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
                'max_sessions': {                                   \n\
                    'header': 'Max Sessions',                       \n\
                    'fillspace': 4,                                 \n\
                    'type': 'integer',                              \n\
                    'default': 1,                                   \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'time': {                                           \n\
                    'header': 'Update Time',                        \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'now',                                      \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'client_type': {                                    \n\
                    'header': 'Type',                               \n\
                    'type': 'array',                                \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
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
                'coordinates': {                                    \n\
                    'header': 'Coordinates',                        \n\
                    'type': 'dict',                                 \n\
                    'default': {                                    \n\
                        'geometry': {                               \n\
                            'type': 'Point',                        \n\
                            'coordinates': [0, 0]                   \n\
                        }                                           \n\
                    },                                              \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'writable',                                 \n\
                        'coordinates'                               \n\
                    ]                                               \n\
                },                                                  \n\
                'settings': {                                       \n\
                    'header': 'Client Settings',                    \n\
                    'type': 'dict',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno': {                                           \n\
                    'header': 'Yuno',                               \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'sessions': {                                       \n\
                    'header': 'Sessions',                           \n\
                    'fillspace': 10,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'sessions': 'client_id'                     \n\
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
            'id': 'client_types',                                   \n\
            'pkey': 'id',                                           \n\
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
                'clients': {                                        \n\
                    'header': 'Clients',                            \n\
                    'type': 'object',                               \n\
                    'flag': [                                       \n\
                        'hook'                                      \n\
                    ],                                              \n\
                    'hook': {                                       \n\
                        'clients': 'client_type'                    \n\
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
                'time': {                                           \n\
                    'header': 'Update Time',                        \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'now',                                      \n\
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
            'id': 'sessions',                                       \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Session Id',                         \n\
                    'fillspace': 40,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'client_id': {                                      \n\
                    'header': 'Client Id',                          \n\
                    'fillspace': 30,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'protocol_version': {                               \n\
                    'header': 'Protocol',                           \n\
                    'fillspace': 4,                                 \n\
                    'type': 'integer',                              \n\
                    'default': 4,                                   \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'clean_start': {                                    \n\
                    'header': 'Clean Start',                        \n\
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': true,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'keep_alive': {                                     \n\
                    'header': 'Keep Alive',                         \n\
                    'fillspace': 6,                                 \n\
                    'type': 'integer',                              \n\
                    'default': 60,                                  \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'session_expiry_interval': {                        \n\
                    'header': 'Session Expiry',                     \n\
                    'fillspace': 10,                                \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'connected': {                                      \n\
                    'header': 'Connected',                          \n\
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'time': {                                           \n\
                    'header': 'Update Time',                        \n\
                    'fillspace': 15,                                \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'now',                                      \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'subscriptions': {                                  \n\
                    'header': 'Subscriptions',                      \n\
                    'fillspace': 30,                                \n\
                    'type': 'array',                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ],                                              \n\
                    'description': 'Array of subscription objects: {topic_filter, qos, no_local, retain_as_published, retain_handling, identifier}' \n\
                },                                                  \n\
                'will_topic': {                                     \n\
                    'header': 'Will Topic',                         \n\
                    'fillspace': 30,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'will_payload': {                                   \n\
                    'header': 'Will Payload',                       \n\
                    'fillspace': 20,                                \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'will_qos': {                                       \n\
                    'header': 'Will QoS',                           \n\
                    'fillspace': 4,                                 \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'will_retain': {                                    \n\
                    'header': 'Will Retain',                        \n\
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'will_delay_interval': {                            \n\
                    'header': 'Will Delay',                         \n\
                    'fillspace': 10,                                \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'will_properties': {                                \n\
                    'header': 'Will Props',                         \n\
                    'fillspace': 20,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'inflight_msgs': {                                  \n\
                    'header': 'Inflight',                           \n\
                    'fillspace': 20,                                \n\
                    'type': 'array',                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ],                                              \n\
                    'description': 'QoS 1/2 messages pending acknowledgment' \n\
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
            'id': 'users',                                          \n\
            'pkey': 'id',                                           \n\
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
                'language': {                                       \n\
                    'header': 'Language',                           \n\
                    'type': 'string',                               \n\
                    'default': 'es',                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent',                               \n\
                        'required',                                 \n\
                        'enum'                                      \n\
                    ],                                              \n\
                    'enum': [                                       \n\
                        'es',                                       \n\
                        'en'                                        \n\
                    ]                                               \n\
                },                                                  \n\
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
                'time': {                                           \n\
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
                    'header': 'Client Id',                          \n\
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
                    'header': 'Client Name',                        \n\
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
                '_client_type': {                                   \n\
                    'header': 'Client Type',                        \n\
                    'type': 'array',                                \n\
                    'flag': ['persistent']                          \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";
