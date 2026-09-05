#pragma once

/*
 *  Two host topics with 'file' columns, and nothing else: __assets__ and
 *  its hooks are treedb's own business, derived at open.
 *    - devices: 'foto' and 'qr' are files (two files in one record)
 *    - places:  'plano' is a file narrowed to pdf, 4 KB, by its column
 */
static char schema_sample[]= "\
{                                                                   \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'devices',                                \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','writable']               \n\
                },                                                  \n\
                'foto': {                                           \n\
                    'header': 'Photo',                              \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey','file','writable']              \n\
                },                                                  \n\
                'qr': {                                             \n\
                    'header': 'QR',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey','file','writable']              \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'topic_name': 'places',                                 \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'plano': {                                          \n\
                    'header': 'Plan',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey','file'],                        \n\
                    'properties': {                                 \n\
                        'content_types': ['application/pdf'],       \n\
                        'max_size': 4096                            \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";
