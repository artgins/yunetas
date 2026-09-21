static char treedb_schema_mqtt_broker[]= "\
{                                                                   \n\
    'id': 'treedb_mqtt_broker',                                     \n\
    'schema_version': '26',                                         \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'client_groups',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '4',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Client Group',                       \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'client_groups': {                                  \n\
                    'header': 'Groups',                             \n\
                    'fillspace': 10,                                \n\
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
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'managers': {                                       \n\
                    'header': 'Managers',                           \n\
                    'fillspace': 10,                                \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'users': 'user_groups'                      \n\
                    }                                               \n\
                },                                                  \n\
                'clients': {                                        \n\
                    'header': 'Clients',                            \n\
                    'fillspace': 10,                                \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'clients': 'client_groups'                  \n\
                    }                                               \n\
                },                                                  \n\
                'description': {                                    \n\
                    'header': 'Description',                        \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'publish_acl': {                                    \n\
                    'header': 'Publish ACL',                        \n\
                    'fillspace': 20,                                \n\
                    'description': 'MQTT topic-filter patterns this group may PUBLISH to (+/# wildcards). Empty = allow-all (backward compat).',\n\
                    'type': 'array',                                \n\
                    'default': [],                                  \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'subscribe_acl': {                                  \n\
                    'header': 'Subscribe ACL',                      \n\
                    'fillspace': 20,                                \n\
                    'description': 'MQTT topic-filter patterns this group may SUBSCRIBE to (+/# wildcards). Empty = allow-all (backward compat).',\n\
                    'type': 'array',                                \n\
                    'default': [],                                  \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'icon': {                                           \n\
                    'header': 'Icon',                               \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'image',                                    \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'enabled': {                                        \n\
                    'header': 'enabled',                            \n\
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': true,                                \n\
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
                'language': {                                       \n\
                    'header': 'Language',                           \n\
                    'fillspace': 6,                                 \n\
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
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'properties': {                                     \n\
                    'header': 'Properties',                         \n\
                    'fillspace': 20,                                \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'coordinates': {                                    \n\
                    'header': 'Coordinates',                        \n\
                    'fillspace': 10,                                \n\
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
                '_geometry': {                                      \n\
                    'header': 'Geometry',                           \n\
                    'fillspace': 10,                                \n\
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
            'topic_version': '7',                                   \n\
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
                    'fillspace': 10,                                \n\
                    'type': 'array',                                \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'client_type': {                                    \n\
                    'header': 'Type',                               \n\
                    'fillspace': 10,                                \n\
                    'type': 'array',                                \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'assigned_id': {                                    \n\
                    'header': 'Assigned Id',                        \n\
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'auto_created': {                                   \n\
                    'header': 'Auto Created',                       \n\
                    'fillspace': 4,                                 \n\
                    'description': 'True if client was auto created (not created by config)',\n\
                    'type': 'boolean',                              \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'description': {                                    \n\
                    'header': 'Description',                        \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'enabled': {                                        \n\
                    'header': 'Enabled',                            \n\
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': true,                                \n\
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
                'properties': {                                     \n\
                    'header': 'Properties',                         \n\
                    'fillspace': 20,                                \n\
                    'type': 'blob',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'coordinates': {                                    \n\
                    'header': 'Coordinates',                        \n\
                    'fillspace': 10,                                \n\
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
                    'fillspace': 20,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'yuno': {                                           \n\
                    'header': 'Yuno',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                '_geometry': {                                      \n\
                    'header': 'Geometry',                           \n\
                    'fillspace': 10,                                \n\
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
            'topic_version': '2',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Type',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'clients': {                                        \n\
                    'header': 'Clients',                            \n\
                    'fillspace': 10,                                \n\
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
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'description': {                                    \n\
                    'header': 'Description',                        \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'icon': {                                           \n\
                    'header': 'Icon',                               \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'image',                                    \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'properties': {                                     \n\
                    'header': 'Properties',                         \n\
                    'fillspace': 20,                                \n\
                    'type': 'blob',                                 \n\
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
                'template_settings': {                              \n\
                    'header': 'Template Settings',                  \n\
                    'fillspace': 20,                                \n\
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
                    'fillspace': 10,                                \n\
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
            'topic_version': '8',                                   \n\
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
                'username': {                                       \n\
                    'header': 'User Name',                          \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'protocol_version': {                               \n\
                    'header': 'Protocol',                           \n\
                    'fillspace': 4,                                 \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'clean_start': {                                    \n\
                    'header': 'Clean Start',                        \n\
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'keep_alive': {                                     \n\
                    'header': 'Keep Alive',                         \n\
                    'fillspace': 6,                                 \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'session_expiry_interval': {                        \n\
                    'header': 'Session Expiry',                     \n\
                    'fillspace': 10,                                \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'in_session': {                                     \n\
                    'header': 'Connected',                          \n\
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                '_gobj_channel': {                                   \n\
                    'header': 'Channel',                            \n\
                    'fillspace': 4,                                 \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                    ]                                               \n\
                },                                                  \n\
                'peername': {                                       \n\
                    'header': 'Peername',                           \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
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
                        'persistent'                                \n\
                    ],                                              \n\
                    'description': 'Array of subscription objects: {topic_filter, qos, no_local, retain_as_published, retain_handling, identifier}' \n\
                },                                                  \n\
                'last_mid': {                                       \n\
                    'header': 'Last Mid',                           \n\
                    'fillspace': 6,                                 \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ],                                              \n\
                    'description': 'Last message id used in the session' \n\
                },                                                  \n\
                'will_topic': {                                     \n\
                    'header': 'Will Topic',                         \n\
                    'fillspace': 30,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'will_payload': {                                   \n\
                    'header': 'Will Payload',                       \n\
                    'fillspace': 20,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': [                                       \n\
                        'gbuffer',                                  \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'will_qos': {                                       \n\
                    'header': 'Will QoS',                           \n\
                    'fillspace': 4,                                 \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'will_retain': {                                    \n\
                    'header': 'Will Retain',                        \n\
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'will_delay_interval': {                            \n\
                    'header': 'Will Delay',                         \n\
                    'fillspace': 10,                                \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'will_properties': {                                \n\
                    'header': 'Will Props',                         \n\
                    'fillspace': 20,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'will_delay_time': {                                \n\
                    'header': 'Will Delay Time',                    \n\
                    'fillspace': 15,                                \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'persistent'                                \n\
                    ],                                              \n\
                    'description': 'Timestamp when client disconnected, used to calculate will delay expiry' \n\
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
            'tkey': 'tm',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '11',                                  \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Topic',                              \n\
                    'fillspace': 40,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'required',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'client_id': {                                      \n\
                    'header': 'Client',                             \n\
                    'fillspace': 30,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'tm': {                                             \n\
                    'header': 'Time',                               \n\
                    'fillspace': 15,                                \n\
                    'type': 'integer',                              \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'now',                                      \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'qos': {                                            \n\
                    'header': 'QoS',                                \n\
                    'fillspace': 4,                                 \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'expiry_interval': {                                \n\
                    'header': 'Msg Expiry',                         \n\
                    'fillspace': 10,                                \n\
                    'type': 'integer',                              \n\
                    'default': 0,                                   \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'properties': {                                     \n\
                    'header': 'User Props',                         \n\
                    'fillspace': 20,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'payload': {                                        \n\
                    'header': 'Payload',                            \n\
                    'fillspace': 30,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': [                                       \n\
                        'gbuffer',                                  \n\
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
            'id': 'users',                                          \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '2',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'User',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'user_groups': {                                    \n\
                    'header': 'Manager',                            \n\
                    'fillspace': 10,                                \n\
                    'type': 'array',                                \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'enabled': {                                        \n\
                    'header': 'Enabled',                            \n\
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': true,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'language': {                                       \n\
                    'header': 'Language',                           \n\
                    'fillspace': 6,                                 \n\
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
                    'fillspace': 20,                                \n\
                    'type': 'dict',                                 \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'properties': {                                     \n\
                    'header': 'Properties',                         \n\
                    'fillspace': 20,                                \n\
                    'type': 'dict',                                 \n\
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
                '_geometry': {                                      \n\
                    'header': 'Geometry',                           \n\
                    'fillspace': 10,                                \n\
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
