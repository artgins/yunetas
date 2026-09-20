#pragma once

/*
 *  Topic "items" with a pkey2 ("version"), and a child topic "parts"
 *  hanging from it. Used to exercise treedb_delete_instance() on the
 *  secondary pkey2 index without touching the on-disk record, and the
 *  link of a NEW INSTANCE of the parent to a child that already names it
 *  (the shape of the agent's `create-yuno`: the hook of the instance is
 *  empty, the child's fkey is the one the previous instance wrote).
 */
static char schema_sample[]= "\
{                                                                   \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'items',                                  \n\
            'pkey': 'id',                                           \n\
            'pkey2s': 'version',                                    \n\
            'system_flag': 'sf_string_key',                         \n\
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
                'parts': {                                          \n\
                    'header': 'Parts',                              \n\
                    'fillspace': 10,                                \n\
                    'type': 'array',                                \n\
                    'flag': ['hook'],                               \n\
                    'hook': {'parts': 'item'}                       \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'topic_name': 'parts',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'item': {                                           \n\
                    'header': 'Item',                               \n\
                    'fillspace': 20,                                \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";
