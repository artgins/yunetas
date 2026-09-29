/*
    Generated from the literal below by schema_to_diagram() (gobj-ui).
    This file holds this comment and the literal, nothing else: replace
    it whole with an export of the schema editor.

    treedb_controlcenter  (schema_version 3)

    {}  dict hook   (N unique children)
    []  list hook   (n not-unique children)
    ()  string hook (1 unique child)
    (↖) 1 fkey      (1 parent)
    [↖] n fkeys     (n parents)
    {↖} N fkeys     (N parents)

    (2) pkey2 - secondary key
    (t) tkey  - time key
    *   field required
    =   field inherited


                      scenarios
            ┌───────────────────────────┐
            │* id                       │
            │  description              │
            │  group                    │
            │  node                     │
            │  agent_url                │
            │  yunos                    │
            │  links                    │
            │  actions                  │
            │  view                     │
            │                   runs {} │ ◀─┐
            │  created_by               │   │
            │  created_at               │   │
            │  updated_by               │   │
            │  updated_at               │   │
            │  _geometry                │   │
            └───────────────────────────┘   │
                                            │
                    scenario_runs           │
            ┌───────────────────────────┐   │
            │* id                       │   │
            │           scenario_id (↖) │ ──┘
            │  action                   │
            │  username                 │
            │  started_at               │
            │  ended_at                 │
            │  result                   │
            │  comment                  │
            │  steps                    │
            │  _geometry                │
            └───────────────────────────┘
*/

static char treedb_schema_controlcenter[]= "\
{                                                                   \n\
    'id': 'treedb_controlcenter',                                   \n\
    'schema_version': '3',                                          \n\
    'topics': [                                                     \n\
        {                                                           \n\
            'id': 'scenarios',                                      \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Scenario',                           \n\
                    'type': 'string',                               \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'description': {                                    \n\
                    'header': 'Description',                        \n\
                    'type': 'string',                               \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'group': {                                          \n\
                    'header': 'Group',                              \n\
                    'type': 'string',                               \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'node': {                                           \n\
                    'header': 'Node',                               \n\
                    'type': 'string',                               \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'agent_url': {                                      \n\
                    'header': 'Agent Url',                          \n\
                    'type': 'string',                               \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'yunos': {                                          \n\
                    'header': 'Yunos',                              \n\
                    'type': 'list',                                 \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'links': {                                          \n\
                    'header': 'Links',                              \n\
                    'type': 'list',                                 \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'actions': {                                        \n\
                    'header': 'Actions',                            \n\
                    'type': 'dict',                                 \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'view': {                                           \n\
                    'header': 'View',                               \n\
                    'type': 'dict',                                 \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'runs': {                                           \n\
                    'header': 'Runs',                               \n\
                    'type': 'object',                               \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'hook'                                      \n\
                    ],                                              \n\
                    'hook': {                                       \n\
                        'scenario_runs': 'scenario_id'              \n\
                    }                                               \n\
                },                                                  \n\
                'created_by': {                                     \n\
                    'header': 'Created By',                         \n\
                    'type': 'string',                               \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'created_at': {                                     \n\
                    'header': 'Created At',                         \n\
                    'type': 'integer',                              \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'updated_by': {                                     \n\
                    'header': 'Updated By',                         \n\
                    'type': 'string',                               \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'writable',                                 \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'updated_at': {                                     \n\
                    'header': 'Updated At',                         \n\
                    'type': 'integer',                              \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
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
        {                                                           \n\
            'id': 'scenario_runs',                                  \n\
            'pkey': 'id',                                           \n\
            'system_flag': 'sf_string_key',                         \n\
            'topic_version': '1',                                   \n\
            'cols': {                                               \n\
                'id': {                                             \n\
                    'header': 'Run',                                \n\
                    'type': 'string',                               \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'persistent',                               \n\
                        'required'                                  \n\
                    ]                                               \n\
                },                                                  \n\
                'scenario_id': {                                    \n\
                    'header': 'Scenario',                           \n\
                    'type': 'string',                               \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'fkey'                                      \n\
                    ]                                               \n\
                },                                                  \n\
                'action': {                                         \n\
                    'header': 'Action',                             \n\
                    'type': 'string',                               \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'username': {                                       \n\
                    'header': 'User',                               \n\
                    'type': 'string',                               \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'started_at': {                                     \n\
                    'header': 'Started At',                         \n\
                    'type': 'integer',                              \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'ended_at': {                                       \n\
                    'header': 'Ended At',                           \n\
                    'type': 'integer',                              \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'time',                                     \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'result': {                                         \n\
                    'header': 'Result',                             \n\
                    'type': 'integer',                              \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'comment': {                                        \n\
                    'header': 'Comment',                            \n\
                    'type': 'string',                               \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
                        'persistent'                                \n\
                    ]                                               \n\
                },                                                  \n\
                'steps': {                                          \n\
                    'header': 'Steps',                              \n\
                    'type': 'list',                                 \n\
                    'fillspace': 10,                                \n\
                    'flag': [                                       \n\
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
