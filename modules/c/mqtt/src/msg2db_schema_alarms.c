static char msg2db_schema_alarms[]= "\
{                                                                   \n\
    'id': 'msg2db_alarms',                                          \n\
    'schema_version': '2',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'alarms',                                         \n\
            'pkey': 'id',                                           \n\
            'tkey': 'tm',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'pkey2': 'alarm',                                       \n\
            'topic_version': '2',                                   \n\
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
                    'fillspace': 1,                                 \n\
                    'type': 'array',                                \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'sent': {                                           \n\
                    'header': 'Sent',                               \n\
                    'fillspace': 1,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'notified': {                                       \n\
                    'header': 'Notified',                           \n\
                    'fillspace': 1,                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                '_yuno': {                                          \n\
                    'header': 'Yuno',                               \n\
                    'fillspace': 1,                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                '_client_type': {                                   \n\
                    'header': 'Client Type',                        \n\
                    'fillspace': 1,                                 \n\
                    'type': 'array',                                \n\
                    'flag': ['persistent']                          \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";
