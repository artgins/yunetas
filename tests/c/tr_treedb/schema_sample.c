#pragma once

static char schema_sample[]= "\
{                                                                   \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'topic_name': 'users',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required','uuid']        \n\
                },                                                  \n\
                'username': {                                       \n\
                    'header': 'User Name',                          \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'firstName': {                                      \n\
                    'header': 'First Name',                         \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'lastName': {                                       \n\
                    'header': 'Last Name',                          \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'email': {                                          \n\
                    'header': 'Email',                              \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'emailVerified': {                                  \n\
                    'header': 'Email Verified',                     \n\
                    'type': 'boolean',                              \n\
                    'flag': ['persistent']                          \n\
                },                                                  \n\
                'disabled': {                                       \n\
                    'header': 'Disabled',                           \n\
                    'type': 'boolean',                              \n\
                    'flag': ['persistent'],                         \n\
                    'default': true                                 \n\
                },                                                  \n\
                'online': {                                         \n\
                    'header': 'On Line',                            \n\
                    'type': 'boolean',                              \n\
                    'flag': []                                      \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Department',                         \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'manager': {                                        \n\
                    'header': 'Manager',                            \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'attributes': {                                     \n\
                    'header': 'Attributes',                         \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'roles': {                                          \n\
                    'header': 'Roles',                              \n\
                    'type': 'array',                                \n\
                    'flag': ['fkey']                                \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
                                                                    \n\
        {                                                           \n\
            'topic_name': 'departments',                            \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'name': {                                           \n\
                    'header': 'Name',                               \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                                                                    \n\
                'department_id': {                                  \n\
                    'header': 'Top Department',                     \n\
                    'type': 'string',                               \n\
                    'flag': ['fkey']                                \n\
                },                                                  \n\
                'departments': {                                    \n\
                    'header': 'Departments',                        \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'departments': 'department_id'              \n\
                    }                                               \n\
                },                                                  \n\
                'users': {                                          \n\
                    'header': 'Users',                              \n\
                    'type': 'array',                                \n\
                    'flag': ['hook', 'fkey'],                       \n\
                    'hook': {                                       \n\
                        'users': 'departments'                      \n\
                    }                                               \n\
                },                                                  \n\
                                                                    \n\
                'managers': {                                       \n\
                    'header': 'Managers',                           \n\
                    'type': 'object',                               \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'users': 'manager',                         \n\
                        'departments': 'users'                      \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
                                                                    \n\
        {                                                           \n\
            'topic_name': 'attributes',                             \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'attribute': {                                      \n\
                    'header': 'Attribute',                          \n\
                    'type': 'object',                               \n\
                    'flag': 'persistent'                            \n\
                },                                                  \n\
                'attributes': {                                     \n\
                    'header': 'Attributes',                         \n\
                    'type': 'array',                                \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'users': 'attributes'                       \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        },                                                          \n\
                                                                    \n\
        {                                                           \n\
            'topic_name': 'roles',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Id',                                 \n\
                    'type': 'string',                               \n\
                    'flag': ['persistent','required']               \n\
                },                                                  \n\
                'role': {                                           \n\
                    'header': 'Role',                               \n\
                    'type': 'object',                               \n\
                    'flag': 'persistent'                            \n\
                },                                                  \n\
                'roles': {                                          \n\
                    'header': 'Roles',                              \n\
                    'type': 'array',                                \n\
                    'flag': ['hook'],                               \n\
                    'hook': {                                       \n\
                        'users': 'roles'                            \n\
                    }                                               \n\
                }                                                   \n\
            }                                                       \n\
        }                                                           \n\
    ]                                                               \n\
}                                                                   \n\
";
