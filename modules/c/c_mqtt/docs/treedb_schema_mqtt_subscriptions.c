#pragma once

/*
 *  TreeDB Schema for MQTT Broker Subscriptions Model (Mosquitto-style)
 *
    {}  dict hook   (N unique children)
    []  list hook   (n not-unique children)
    (↖) 1 fkey      (1 parent)
    [↖] n fkeys     (n parents)

    * field required

                        clients
            ┌───────────────────────────┐
            │* id (client_id)           │
            │                           │
            │          subscriptions {} │ ◀─┐N
            │                           │   │
            │  protocol_version         │
            │  clean_start              │
            │  username                 │
            │  keep_alive               │
            │  session_expiry_interval  │
            │  connected                │
            │  will_topic               │
            │  will_payload             │
            │  will_qos                 │
            │  will_retain              │
            │  will_delay_interval      │
            │                           │
            │  _geometry                │
            └───────────────────────────┘
                                            │
                                            │
                    subscriptions           │
            ┌───────────────────────────┐   │
            │* id (topic_filter)        │   │
            │                           │   │
            │              client_id (↖)│ ──┘1
            │                           │
            │* qos                      │
            │  no_local                 │
            │  retain_as_published      │
            │  retain_handling          │
            │  identifier               │
            │                           │
            │          shared_group [↖] │ ──┐n
            │                           │   │
            │  _geometry                │   │
            └───────────────────────────┘   │
                                            │
                                            │
                    shared_groups           │
            ┌───────────────────────────┐   │
            │* id (group_name)          │   │
            │                           │   │
            │          subscriptions {} │ ◀─┘N
            │                           │
            │* topic_filter             │
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

*/

static char treedb_schema_mqtt_subscriptions[]= "\
{                                                                   \n\
    'id': 'treedb_mqtt_subscriptions',                              \n\
    'schema_version': '2',                                          \n\
    'topics': [                                                     \n\
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
                'subscriptions': {                                  \n\
                    'header': 'Subscriptions',                      \n\
                    'fillspace': 20,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'subscriptions': 'client_id'                \n\
                    }                                               \n\
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
                'username': {                                       \n\
                    'header': 'Username',                           \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
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
                        'writable'                                  \n\
                    ]                                               \n\
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
            'id': 'subscriptions',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '2',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Topic Filter',                       \n\
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
                'qos': {                                            \n\
                    'header': 'QoS',                                \n\
                    'fillspace': 4,                                 \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'no_local': {                                       \n\
                    'header': 'No Local',                           \n\
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'retain_as_published': {                            \n\
                    'header': 'Retain As Pub',                      \n\
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'retain_handling': {                                \n\
                    'header': 'Retain Handling',                    \n\
                    'fillspace': 4,                                 \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'identifier': {                                     \n\
                    'header': 'Sub Id',                             \n\
                    'fillspace': 10,                                \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'shared_group': {                                   \n\
                    'header': 'Shared Group',                       \n\
                    'fillspace': 20,                                \n\
                    'type': 'array',                                \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
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
            'id': 'shared_groups',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Group Name',                         \n\
                    'fillspace': 30,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'subscriptions': {                                  \n\
                    'header': 'Subscriptions',                      \n\
                    'fillspace': 20,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'subscriptions': 'shared_group'             \n\
                    }                                               \n\
                },                                                  \n\
                'topic_filter': {                                   \n\
                    'header': 'Topic Filter',                       \n\
                    'fillspace': 40,                                \n\
                    'type': 'string',                               \n\
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
            'id': 'retained_msgs',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Topic',                              \n\
                    'fillspace': 50,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'payload': {                                        \n\
                    'header': 'Payload',                            \n\
                    'fillspace': 30,                                \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'qos': {                                            \n\
                    'header': 'QoS',                                \n\
                    'fillspace': 4,                                 \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'time': {                                           \n\
                    'header': 'Store Time',                         \n\
                    'fillspace': 11,                                \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'now',                                      \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'message_expiry_interval': {                        \n\
                    'header': 'Msg Expiry',                         \n\
                    'fillspace': 10,                                \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'payload_format_indicator': {                       \n\
                    'header': 'Format',                             \n\
                    'fillspace': 4,                                 \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'content_type': {                                   \n\
                    'header': 'Content Type',                       \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'response_topic': {                                 \n\
                    'header': 'Response Topic',                     \n\
                    'fillspace': 30,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'correlation_data': {                               \n\
                    'header': 'Correlation',                        \n\
                    'fillspace': 20,                                \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'user_properties': {                                \n\
                    'header': 'User Props',                         \n\
                    'fillspace': 20,                                \n\
                    'type': 'dict',                                 \n\
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
