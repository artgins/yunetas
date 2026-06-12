#pragma once

/*
 *  Two topics:
 *    - "items": system topic (system_topic: true) with a pkey2 ("version")
 *      and a __system__ col, to exercise the record-level and
 *      instance-level system guards plus the topic-level guard.
 *    - "scratch": plain topic, control case for treedb_delete_topic().
 */
static char schema_sample[]= "\
{                                                                   \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'items',                                  \n\
            'pkey': 'id',                                           \n\
            'pkey2s': 'version',                                    \n\
            'system_flag': 'sf_string_key',                         \n\
            'system_topic': true,                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'version': {                                        \n\
                    'header': 'Version',                            \n\
                    'fillspace': 10,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'payload': {                                        \n\
                    'header': 'Payload',                            \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                '__system__': {                                     \n\
                    'header': 'System',                             \n\
                    'fillspace': 4,                                 \n\
                    'type': 'boolean',                              \n\
                    'default': false,                               \n\
                    'flag': ['persistent']                          \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'topic_name': 'scratch',                                \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";
