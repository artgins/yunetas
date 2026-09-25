#pragma once

/*
 *  Two topics with a pkey2 ("version"), linked by a LIST hook
 *  (parents.kids <- kids.parent), a DICT hook over a list fkey
 *  (parents.tags <- kids.tags) and a DICT hook over a string fkey
 *  (parents.pets <- kids.owner).
 *  Used to exercise treedb_delete_instance() and treedb_delete_node() on
 *  nodes whose instances hang from, or hold, other nodes.
 *
 *  And a third topic, "defaulted", whose pkey2 has a DEFAULT: a create
 *  that does not give it indexes the node by the value its record holds.
 */
static char schema_links[]= "\
{                                                                   \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'parents',                                \n\
            'pkey': 'id',                                           \n\
            'pkey2s': 'version',                                    \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'version': {                                        \n\
                    'header': 'Version',                            \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'kids': {                                           \n\
                    'header': 'Kids',                               \n\
                    'type': 'array',                                \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'kids': 'parent'                            \n\
                    }                                               \n\
                },                                                  \n\
                'tags': {                                           \n\
                    'header': 'Tags',                               \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'kids': 'tags'                              \n\
                    }                                               \n\
                },                                                  \n\
                'pets': {                                           \n\
                    'header': 'Pets',                               \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'kids': 'owner'                             \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'topic_name': 'kids',                                   \n\
            'pkey': 'id',                                           \n\
            'pkey2s': 'version',                                    \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'version': {                                        \n\
                    'header': 'Version',                            \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'parent': {                                         \n\
                    'header': 'Parent',                             \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'tags': {                                           \n\
                    'header': 'Tags',                               \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'owner': {                                          \n\
                    'header': 'Owner',                              \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'note': {                                           \n\
                    'header': 'Note',                               \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'extra': {                                          \n\
                    'header': 'Extra',                              \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
        {                                                           \n\
            'topic_name': 'defaulted',                              \n\
            'pkey': 'id',                                           \n\
            'pkey2s': 'version',                                    \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'version': {                                        \n\
                    'header': 'Version',                            \n\
                    'type': 'string',                               \n\
                    'default': 'v0',                                \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'note': {                                           \n\
                    'header': 'Note',                               \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent']                          \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";
