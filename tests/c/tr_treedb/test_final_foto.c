/****************************************************************************
 *          test.c
 *
 *          Load topic with code.
 *
 *          Copyright (c) 2019 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <argp.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <locale.h>
#include <time.h>

#include "test_tr_treedb.h"


/***************************************************************************
 *
 ***************************************************************************/
PUBLIC BOOL test_final_foto(
    json_t *tranger,
    const char *treedb_name,
    int without_ok_tests,
    int without_bad_tests,
    int show_oks,
    int verbose
)
{
    BOOL ret = TRUE;
    json_t *expected = 0;
    /*------------------------------------------------------------*
     *          Foto final
     *------------------------------------------------------------*/
char foto_final[]= "\
{\n\
    'treedb_test': {\n\
        '__schema_version__': 0, \n\
        '__snaps__': {\n\
            'id': {}\n\
        },\n\
        '__graphs__': { \n\
            'id': {} \n\
        }, \n\
        'users': {\n\
            'id': {\n\
                'cf2495a7-1cb9-4ce5-85f5-7a90dc5859d8': {\n\
                    'id': 'cf2495a7-1cb9-4ce5-85f5-7a90dc5859d8',\n\
                    'username': 'devboss@email.com',\n\
                    'firstName': 'DevBoss',\n\
                    'lastName': 'Martínez',\n\
                    'email': 'devboss@email.com',\n\
                    'emailVerified': false,\n\
                    'disabled': true,\n\
                    'online': false,\n\
                    'departments': [\n\
                        'departments^development^users'\n\
                    ],\n\
                    'manager': [\n\
                        'departments^development^managers'\n\
                    ],\n\
                    'attributes': [],\n\
                    'roles': [],\n\
                    '__md_treedb__': {\n\
                        'treedb_name': 'treedb_test',\n\
                        'topic_name': 'users',\n\
                        '__rowid__': 19,\n\
                        '__t__': 1568455917,\n\
                        '__tag__': 0,\n\
                        '__pure_node__': true\n\
                    }\n\
                },\n\
                'b6eb2c61-622c-41d3-afa0-548b3fa882cd': {\n\
                    'id': 'b6eb2c61-622c-41d3-afa0-548b3fa882cd',\n\
                    'username': 'admin@email.com',\n\
                    'firstName': 'Admin',\n\
                    'lastName': 'Martínez',\n\
                    'email': 'admin@email.com',\n\
                    'emailVerified': false,\n\
                    'disabled': true,\n\
                    'online': false,\n\
                    'departments': [\n\
                        'departments^administration^users'\n\
                    ],\n\
                    'manager': [\n\
                        'departments^administration^managers'\n\
                    ],\n\
                    'attributes': [],\n\
                    'roles': [],\n\
                    '__md_treedb__': {\n\
                        'treedb_name': 'treedb_test',\n\
                        'topic_name': 'users',\n\
                        '__rowid__': 18,\n\
                        '__t__': 1568455917,\n\
                        '__tag__': 0,\n\
                        '__pure_node__': true\n\
                    }\n\
                },\n\
                'ba115bf6-5a7c-4ac7-9852-ce32135b427f': {\n\
                    'id': 'ba115bf6-5a7c-4ac7-9852-ce32135b427f',\n\
                    'username': 'dueño@email.com',\n\
                    'firstName': 'Don',\n\
                    'lastName': 'Duenño',\n\
                    'email': 'dueño@email.com',\n\
                    'emailVerified': false,\n\
                    'disabled': true,\n\
                    'online': false,\n\
                    'departments': [\n\
                        'departments^direction^users'\n\
                    ],\n\
                    'manager': [\n\
                        'departments^direction^managers'\n\
                    ],\n\
                    'attributes': [],\n\
                    'roles': [],\n\
                    '__md_treedb__': {\n\
                        'treedb_name': 'treedb_test',\n\
                        'topic_name': 'users',\n\
                        '__rowid__': 17,\n\
                        '__t__': 1568455917,\n\
                        '__tag__': 0,\n\
                        '__pure_node__': true\n\
                    }\n\
                },\n\
                'a3586af9-2e57-489e-b6d3-ede04ea77bd0': {\n\
                    'id': 'a3586af9-2e57-489e-b6d3-ede04ea77bd0',\n\
                    'username': 'dev2@email.com',\n\
                    'firstName': 'Dev2',\n\
                    'lastName': 'Martínez',\n\
                    'email': 'dev2@email.com',\n\
                    'emailVerified': false,\n\
                    'disabled': true,\n\
                    'online': false,\n\
                    'departments': [\n\
                        'departments^development^users'\n\
                    ],\n\
                    'manager': [],\n\
                    'attributes': [],\n\
                    'roles': [],\n\
                    '__md_treedb__': {\n\
                        'treedb_name': 'treedb_test',\n\
                        'topic_name': 'users',\n\
                        '__rowid__': 15,\n\
                        '__t__': 1568455917,\n\
                        '__tag__': 0,\n\
                        '__pure_node__': true\n\
                    }\n\
                },\n\
                'b24aff31-36da-4014-95ec-8b10a1279cf3': {\n\
                    'id': 'b24aff31-36da-4014-95ec-8b10a1279cf3',\n\
                    'username': 'dev1@email.com',\n\
                    'firstName': 'Dev1',\n\
                    'lastName': 'Martínez',\n\
                    'email': 'dev1@email.com',\n\
                    'emailVerified': false,\n\
                    'disabled': true,\n\
                    'online': false,\n\
                    'departments': [\n\
                        'departments^development^users'\n\
                    ],\n\
                    'manager': [],\n\
                    'attributes': [],\n\
                    'roles': [],\n\
                    '__md_treedb__': {\n\
                        'treedb_name': 'treedb_test',\n\
                        'topic_name': 'users',\n\
                        '__rowid__': 14,\n\
                        '__t__': 1568455917,\n\
                        '__tag__': 0,\n\
                        '__pure_node__': true\n\
                    }\n\
                },\n\
                '95a4820d-89b0-4bd0-8144-2ef997638b80': {\n\
                    'id': '95a4820d-89b0-4bd0-8144-2ef997638b80',\n\
                    'username': 'op2@email.com',\n\
                    'firstName': 'Op2',\n\
                    'lastName': 'Martínez',\n\
                    'email': 'op2@email.com',\n\
                    'emailVerified': false,\n\
                    'disabled': true,\n\
                    'online': false,\n\
                    'departments': [\n\
                        'departments^operation^users'\n\
                    ],\n\
                    'manager': [],\n\
                    'attributes': [],\n\
                    'roles': [],\n\
                    '__md_treedb__': {\n\
                        'treedb_name': 'treedb_test',\n\
                        'topic_name': 'users',\n\
                        '__rowid__': 13,\n\
                        '__t__': 1568455917,\n\
                        '__tag__': 0,\n\
                        '__pure_node__': true\n\
                    }\n\
                },\n\
                'cda33bc2-d191-41e6-b779-471977869463': {\n\
                    'id': 'cda33bc2-d191-41e6-b779-471977869463',\n\
                    'username': 'op1@email.com',\n\
                    'firstName': 'Op1',\n\
                    'lastName': 'Martínez',\n\
                    'email': 'op1@email.com',\n\
                    'emailVerified': false,\n\
                    'disabled': true,\n\
                    'online': false,\n\
                    'departments': [\n\
                        'departments^operation^users'\n\
                    ],\n\
                    'manager': [],\n\
                    'attributes': [],\n\
                    'roles': [],\n\
                    '__md_treedb__': {\n\
                        'treedb_name': 'treedb_test',\n\
                        'topic_name': 'users',\n\
                        '__rowid__': 12,\n\
                        '__t__': 1568455917,\n\
                        '__tag__': 0,\n\
                        '__pure_node__': true\n\
                    }\n\
                },\n\
                '9594f724-c029-4190-8311-a42438f74a99': {\n\
                    'id': '9594f724-c029-4190-8311-a42438f74a99',\n\
                    'username': 'escritor@email.com',\n\
                    'firstName': 'Escritor',\n\
                    'lastName': 'Martínez',\n\
                    'email': 'escritor@email.com',\n\
                    'emailVerified': false,\n\
                    'disabled': true,\n\
                    'online': false,\n\
                    'departments': [\n\
                        'departments^administration^users'\n\
                    ],\n\
                    'manager': [],\n\
                    'attributes': [],\n\
                    'roles': [],\n\
                    '__md_treedb__': {\n\
                        'treedb_name': 'treedb_test',\n\
                        'topic_name': 'users',\n\
                        '__rowid__': 11,\n\
                        '__t__': 1568455917,\n\
                        '__tag__': 0,\n\
                        '__pure_node__': true\n\
                    }\n\
                }\n\
            }\n\
        },\n\
        'departments': {\n\
            'id': {\n\
                'development': {\n\
                    'id': 'development',\n\
                    'name': 'Desarrollo',\n\
                    'department_id': 'departments^administration^departments',\n\
                    'departments': {},\n\
                    'users': [\n\
                        {\n\
                            'id': 'cf2495a7-1cb9-4ce5-85f5-7a90dc5859d8',\n\
                            'username': 'devboss@email.com',\n\
                            'firstName': 'DevBoss',\n\
                            'lastName': 'Martínez',\n\
                            'email': 'devboss@email.com',\n\
                            'emailVerified': false,\n\
                            'disabled': true,\n\
                            'online': false,\n\
                            'departments': [\n\
                                'departments^development^users'\n\
                            ],\n\
                            'manager': [\n\
                                'departments^development^managers'\n\
                            ],\n\
                            'attributes': [],\n\
                            'roles': [],\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'users',\n\
                                '__rowid__': 19,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        },\n\
                        {\n\
                            'id': 'a3586af9-2e57-489e-b6d3-ede04ea77bd0',\n\
                            'username': 'dev2@email.com',\n\
                            'firstName': 'Dev2',\n\
                            'lastName': 'Martínez',\n\
                            'email': 'dev2@email.com',\n\
                            'emailVerified': false,\n\
                            'disabled': true,\n\
                            'online': false,\n\
                            'departments': [\n\
                                'departments^development^users'\n\
                            ],\n\
                            'manager': [],\n\
                            'attributes': [],\n\
                            'roles': [],\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'users',\n\
                                '__rowid__': 15,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        },\n\
                        {\n\
                            'id': 'b24aff31-36da-4014-95ec-8b10a1279cf3',\n\
                            'username': 'dev1@email.com',\n\
                            'firstName': 'Dev1',\n\
                            'lastName': 'Martínez',\n\
                            'email': 'dev1@email.com',\n\
                            'emailVerified': false,\n\
                            'disabled': true,\n\
                            'online': false,\n\
                            'departments': [\n\
                                'departments^development^users'\n\
                            ],\n\
                            'manager': [],\n\
                            'attributes': [],\n\
                            'roles': [],\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'users',\n\
                                '__rowid__': 14,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        }\n\
                    ],\n\
                    'managers': {\n\
                        'cf2495a7-1cb9-4ce5-85f5-7a90dc5859d8': {\n\
                            'id': 'cf2495a7-1cb9-4ce5-85f5-7a90dc5859d8',\n\
                            'username': 'devboss@email.com',\n\
                            'firstName': 'DevBoss',\n\
                            'lastName': 'Martínez',\n\
                            'email': 'devboss@email.com',\n\
                            'emailVerified': false,\n\
                            'disabled': true,\n\
                            'online': false,\n\
                            'departments': [\n\
                                'departments^development^users'\n\
                            ],\n\
                            'manager': [\n\
                                'departments^development^managers'\n\
                            ],\n\
                            'attributes': [],\n\
                            'roles': [],\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'users',\n\
                                '__rowid__': 19,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        }\n\
                    },\n\
                    '__md_treedb__': {\n\
                        'treedb_name': 'treedb_test',\n\
                        'topic_name': 'departments',\n\
                        '__rowid__': 7,\n\
                        '__t__': 1568455917,\n\
                        '__tag__': 0,\n\
                        '__pure_node__': true\n\
                    }\n\
                },\n\
                'operation': {\n\
                    'id': 'operation',\n\
                    'name': 'Gestión',\n\
                    'department_id': 'departments^administration^departments',\n\
                    'departments': {},\n\
                    'users': [\n\
                        {\n\
                            'id': '95a4820d-89b0-4bd0-8144-2ef997638b80',\n\
                            'username': 'op2@email.com',\n\
                            'firstName': 'Op2',\n\
                            'lastName': 'Martínez',\n\
                            'email': 'op2@email.com',\n\
                            'emailVerified': false,\n\
                            'disabled': true,\n\
                            'online': false,\n\
                            'departments': [\n\
                                'departments^operation^users'\n\
                            ],\n\
                            'manager': [],\n\
                            'attributes': [],\n\
                            'roles': [],\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'users',\n\
                                '__rowid__': 13,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        },\n\
                        {\n\
                            'id': 'cda33bc2-d191-41e6-b779-471977869463',\n\
                            'username': 'op1@email.com',\n\
                            'firstName': 'Op1',\n\
                            'lastName': 'Martínez',\n\
                            'email': 'op1@email.com',\n\
                            'emailVerified': false,\n\
                            'disabled': true,\n\
                            'online': false,\n\
                            'departments': [\n\
                                'departments^operation^users'\n\
                            ],\n\
                            'manager': [],\n\
                            'attributes': [],\n\
                            'roles': [],\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'users',\n\
                                '__rowid__': 12,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        }\n\
                    ],\n\
                    'managers': {},\n\
                    '__md_treedb__': {\n\
                        'treedb_name': 'treedb_test',\n\
                        'topic_name': 'departments',\n\
                        '__rowid__': 5,\n\
                        '__t__': 1568455917,\n\
                        '__tag__': 0,\n\
                        '__pure_node__': true\n\
                    }\n\
                },\n\
                'administration': {\n\
                    'id': 'administration',\n\
                    'name': 'Administración',\n\
                    'department_id': 'departments^direction^departments',\n\
                    'departments': {\n\
                        'development': {\n\
                            'id': 'development',\n\
                            'name': 'Desarrollo',\n\
                            'department_id': 'departments^administration^departments',\n\
                            'departments': {},\n\
                            'users': [\n\
                                {\n\
                                    'id': 'cf2495a7-1cb9-4ce5-85f5-7a90dc5859d8',\n\
                                    'username': 'devboss@email.com',\n\
                                    'firstName': 'DevBoss',\n\
                                    'lastName': 'Martínez',\n\
                                    'email': 'devboss@email.com',\n\
                                    'emailVerified': false,\n\
                                    'disabled': true,\n\
                                    'online': false,\n\
                                    'departments': [\n\
                                        'departments^development^users'\n\
                                    ],\n\
                                    'manager': [\n\
                                        'departments^development^managers'\n\
                                    ],\n\
                                    'attributes': [],\n\
                                    'roles': [],\n\
                                    '__md_treedb__': {\n\
                                        'treedb_name': 'treedb_test',\n\
                                        'topic_name': 'users',\n\
                                        '__rowid__': 19,\n\
                                        '__t__': 1568455917,\n\
                                        '__tag__': 0,\n\
                                        '__pure_node__': true\n\
                                    }\n\
                                },\n\
                                {\n\
                                    'id': 'a3586af9-2e57-489e-b6d3-ede04ea77bd0',\n\
                                    'username': 'dev2@email.com',\n\
                                    'firstName': 'Dev2',\n\
                                    'lastName': 'Martínez',\n\
                                    'email': 'dev2@email.com',\n\
                                    'emailVerified': false,\n\
                                    'disabled': true,\n\
                                    'online': false,\n\
                                    'departments': [\n\
                                        'departments^development^users'\n\
                                    ],\n\
                                    'manager': [],\n\
                                    'attributes': [],\n\
                                    'roles': [],\n\
                                    '__md_treedb__': {\n\
                                        'treedb_name': 'treedb_test',\n\
                                        'topic_name': 'users',\n\
                                        '__rowid__': 15,\n\
                                        '__t__': 1568455917,\n\
                                        '__tag__': 0,\n\
                                        '__pure_node__': true\n\
                                    }\n\
                                },\n\
                                {\n\
                                    'id': 'b24aff31-36da-4014-95ec-8b10a1279cf3',\n\
                                    'username': 'dev1@email.com',\n\
                                    'firstName': 'Dev1',\n\
                                    'lastName': 'Martínez',\n\
                                    'email': 'dev1@email.com',\n\
                                    'emailVerified': false,\n\
                                    'disabled': true,\n\
                                    'online': false,\n\
                                    'departments': [\n\
                                        'departments^development^users'\n\
                                    ],\n\
                                    'manager': [],\n\
                                    'attributes': [],\n\
                                    'roles': [],\n\
                                    '__md_treedb__': {\n\
                                        'treedb_name': 'treedb_test',\n\
                                        'topic_name': 'users',\n\
                                        '__rowid__': 14,\n\
                                        '__t__': 1568455917,\n\
                                        '__tag__': 0,\n\
                                        '__pure_node__': true\n\
                                    }\n\
                                }\n\
                            ],\n\
                            'managers': {\n\
                                'cf2495a7-1cb9-4ce5-85f5-7a90dc5859d8': {\n\
                                    'id': 'cf2495a7-1cb9-4ce5-85f5-7a90dc5859d8',\n\
                                    'username': 'devboss@email.com',\n\
                                    'firstName': 'DevBoss',\n\
                                    'lastName': 'Martínez',\n\
                                    'email': 'devboss@email.com',\n\
                                    'emailVerified': false,\n\
                                    'disabled': true,\n\
                                    'online': false,\n\
                                    'departments': [\n\
                                        'departments^development^users'\n\
                                    ],\n\
                                    'manager': [\n\
                                        'departments^development^managers'\n\
                                    ],\n\
                                    'attributes': [],\n\
                                    'roles': [],\n\
                                    '__md_treedb__': {\n\
                                        'treedb_name': 'treedb_test',\n\
                                        'topic_name': 'users',\n\
                                        '__rowid__': 19,\n\
                                        '__t__': 1568455917,\n\
                                        '__tag__': 0,\n\
                                        '__pure_node__': true\n\
                                    }\n\
                                }\n\
                            },\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'departments',\n\
                                '__rowid__': 7,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        },\n\
                        'operation': {\n\
                            'id': 'operation',\n\
                            'name': 'Gestión',\n\
                            'department_id': 'departments^administration^departments',\n\
                            'departments': {},\n\
                            'users': [\n\
                                {\n\
                                    'id': '95a4820d-89b0-4bd0-8144-2ef997638b80',\n\
                                    'username': 'op2@email.com',\n\
                                    'firstName': 'Op2',\n\
                                    'lastName': 'Martínez',\n\
                                    'email': 'op2@email.com',\n\
                                    'emailVerified': false,\n\
                                    'disabled': true,\n\
                                    'online': false,\n\
                                    'departments': [\n\
                                        'departments^operation^users'\n\
                                    ],\n\
                                    'manager': [],\n\
                                    'attributes': [],\n\
                                    'roles': [],\n\
                                    '__md_treedb__': {\n\
                                        'treedb_name': 'treedb_test',\n\
                                        'topic_name': 'users',\n\
                                        '__rowid__': 13,\n\
                                        '__t__': 1568455917,\n\
                                        '__tag__': 0,\n\
                                        '__pure_node__': true\n\
                                    }\n\
                                },\n\
                                {\n\
                                    'id': 'cda33bc2-d191-41e6-b779-471977869463',\n\
                                    'username': 'op1@email.com',\n\
                                    'firstName': 'Op1',\n\
                                    'lastName': 'Martínez',\n\
                                    'email': 'op1@email.com',\n\
                                    'emailVerified': false,\n\
                                    'disabled': true,\n\
                                    'online': false,\n\
                                    'departments': [\n\
                                        'departments^operation^users'\n\
                                    ],\n\
                                    'manager': [],\n\
                                    'attributes': [],\n\
                                    'roles': [],\n\
                                    '__md_treedb__': {\n\
                                        'treedb_name': 'treedb_test',\n\
                                        'topic_name': 'users',\n\
                                        '__rowid__': 12,\n\
                                        '__t__': 1568455917,\n\
                                        '__tag__': 0,\n\
                                        '__pure_node__': true\n\
                                    }\n\
                                }\n\
                            ],\n\
                            'managers': {},\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'departments',\n\
                                '__rowid__': 5,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        }\n\
                    },\n\
                    'users': [\n\
                        {\n\
                            'id': 'b6eb2c61-622c-41d3-afa0-548b3fa882cd',\n\
                            'username': 'admin@email.com',\n\
                            'firstName': 'Admin',\n\
                            'lastName': 'Martínez',\n\
                            'email': 'admin@email.com',\n\
                            'emailVerified': false,\n\
                            'disabled': true,\n\
                            'online': false,\n\
                            'departments': [\n\
                                'departments^administration^users'\n\
                            ],\n\
                            'manager': [\n\
                                'departments^administration^managers'\n\
                            ],\n\
                            'attributes': [],\n\
                            'roles': [],\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'users',\n\
                                '__rowid__': 18,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        },\n\
                        {\n\
                            'id': '9594f724-c029-4190-8311-a42438f74a99',\n\
                            'username': 'escritor@email.com',\n\
                            'firstName': 'Escritor',\n\
                            'lastName': 'Martínez',\n\
                            'email': 'escritor@email.com',\n\
                            'emailVerified': false,\n\
                            'disabled': true,\n\
                            'online': false,\n\
                            'departments': [\n\
                                'departments^administration^users'\n\
                            ],\n\
                            'manager': [],\n\
                            'attributes': [],\n\
                            'roles': [],\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'users',\n\
                                '__rowid__': 11,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        }\n\
                    ],\n\
                    'managers': {\n\
                        'b6eb2c61-622c-41d3-afa0-548b3fa882cd': {\n\
                            'id': 'b6eb2c61-622c-41d3-afa0-548b3fa882cd',\n\
                            'username': 'admin@email.com',\n\
                            'firstName': 'Admin',\n\
                            'lastName': 'Martínez',\n\
                            'email': 'admin@email.com',\n\
                            'emailVerified': false,\n\
                            'disabled': true,\n\
                            'online': false,\n\
                            'departments': [\n\
                                'departments^administration^users'\n\
                            ],\n\
                            'manager': [\n\
                                'departments^administration^managers'\n\
                            ],\n\
                            'attributes': [],\n\
                            'roles': [],\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'users',\n\
                                '__rowid__': 18,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        }\n\
                    },\n\
                    '__md_treedb__': {\n\
                        'treedb_name': 'treedb_test',\n\
                        'topic_name': 'departments',\n\
                        '__rowid__': 3,\n\
                        '__t__': 1568455917,\n\
                        '__tag__': 0,\n\
                        '__pure_node__': true\n\
                    }\n\
                },\n\
                'direction': {\n\
                    'id': 'direction',\n\
                    'name': 'Dirección',\n\
                    'department_id': '',\n\
                    'departments': {\n\
                        'administration': {\n\
                            'id': 'administration',\n\
                            'name': 'Administración',\n\
                            'department_id': 'departments^direction^departments',\n\
                            'departments': {\n\
                                'development': {\n\
                                    'id': 'development',\n\
                                    'name': 'Desarrollo',\n\
                                    'department_id': 'departments^administration^departments',\n\
                                    'departments': {},\n\
                                    'users': [\n\
                                        {\n\
                                            'id': 'cf2495a7-1cb9-4ce5-85f5-7a90dc5859d8',\n\
                                            'username': 'devboss@email.com',\n\
                                            'firstName': 'DevBoss',\n\
                                            'lastName': 'Martínez',\n\
                                            'email': 'devboss@email.com',\n\
                                            'emailVerified': false,\n\
                                            'disabled': true,\n\
                                            'online': false,\n\
                                            'departments': [\n\
                                                'departments^development^users'\n\
                                            ],\n\
                                            'manager': [\n\
                                                'departments^development^managers'\n\
                                            ],\n\
                                            'attributes': [],\n\
                                            'roles': [],\n\
                                            '__md_treedb__': {\n\
                                                'treedb_name': 'treedb_test',\n\
                                                'topic_name': 'users',\n\
                                                '__rowid__': 19,\n\
                                                '__t__': 1568455917,\n\
                                                '__tag__': 0,\n\
                                                '__pure_node__': true\n\
                                            }\n\
                                        },\n\
                                        {\n\
                                            'id': 'a3586af9-2e57-489e-b6d3-ede04ea77bd0',\n\
                                            'username': 'dev2@email.com',\n\
                                            'firstName': 'Dev2',\n\
                                            'lastName': 'Martínez',\n\
                                            'email': 'dev2@email.com',\n\
                                            'emailVerified': false,\n\
                                            'disabled': true,\n\
                                            'online': false,\n\
                                            'departments': [\n\
                                                'departments^development^users'\n\
                                            ],\n\
                                            'manager': [],\n\
                                            'attributes': [],\n\
                                            'roles': [],\n\
                                            '__md_treedb__': {\n\
                                                'treedb_name': 'treedb_test',\n\
                                                'topic_name': 'users',\n\
                                                '__rowid__': 15,\n\
                                                '__t__': 1568455917,\n\
                                                '__tag__': 0,\n\
                                                '__pure_node__': true\n\
                                            }\n\
                                        },\n\
                                        {\n\
                                            'id': 'b24aff31-36da-4014-95ec-8b10a1279cf3',\n\
                                            'username': 'dev1@email.com',\n\
                                            'firstName': 'Dev1',\n\
                                            'lastName': 'Martínez',\n\
                                            'email': 'dev1@email.com',\n\
                                            'emailVerified': false,\n\
                                            'disabled': true,\n\
                                            'online': false,\n\
                                            'departments': [\n\
                                                'departments^development^users'\n\
                                            ],\n\
                                            'manager': [],\n\
                                            'attributes': [],\n\
                                            'roles': [],\n\
                                            '__md_treedb__': {\n\
                                                'treedb_name': 'treedb_test',\n\
                                                'topic_name': 'users',\n\
                                                '__rowid__': 14,\n\
                                                '__t__': 1568455917,\n\
                                                '__tag__': 0,\n\
                                                '__pure_node__': true\n\
                                            }\n\
                                        }\n\
                                    ],\n\
                                    'managers': {\n\
                                        'cf2495a7-1cb9-4ce5-85f5-7a90dc5859d8': {\n\
                                            'id': 'cf2495a7-1cb9-4ce5-85f5-7a90dc5859d8',\n\
                                            'username': 'devboss@email.com',\n\
                                            'firstName': 'DevBoss',\n\
                                            'lastName': 'Martínez',\n\
                                            'email': 'devboss@email.com',\n\
                                            'emailVerified': false,\n\
                                            'disabled': true,\n\
                                            'online': false,\n\
                                            'departments': [\n\
                                                'departments^development^users'\n\
                                            ],\n\
                                            'manager': [\n\
                                                'departments^development^managers'\n\
                                            ],\n\
                                            'attributes': [],\n\
                                            'roles': [],\n\
                                            '__md_treedb__': {\n\
                                                'treedb_name': 'treedb_test',\n\
                                                'topic_name': 'users',\n\
                                                '__rowid__': 19,\n\
                                                '__t__': 1568455917,\n\
                                                '__tag__': 0,\n\
                                                '__pure_node__': true\n\
                                            }\n\
                                        }\n\
                                    },\n\
                                    '__md_treedb__': {\n\
                                        'treedb_name': 'treedb_test',\n\
                                        'topic_name': 'departments',\n\
                                        '__rowid__': 7,\n\
                                        '__t__': 1568455917,\n\
                                        '__tag__': 0,\n\
                                        '__pure_node__': true\n\
                                    }\n\
                                },\n\
                                'operation': {\n\
                                    'id': 'operation',\n\
                                    'name': 'Gestión',\n\
                                    'department_id': 'departments^administration^departments',\n\
                                    'departments': {},\n\
                                    'users': [\n\
                                        {\n\
                                            'id': '95a4820d-89b0-4bd0-8144-2ef997638b80',\n\
                                            'username': 'op2@email.com',\n\
                                            'firstName': 'Op2',\n\
                                            'lastName': 'Martínez',\n\
                                            'email': 'op2@email.com',\n\
                                            'emailVerified': false,\n\
                                            'disabled': true,\n\
                                            'online': false,\n\
                                            'departments': [\n\
                                                'departments^operation^users'\n\
                                            ],\n\
                                            'manager': [],\n\
                                            'attributes': [],\n\
                                            'roles': [],\n\
                                            '__md_treedb__': {\n\
                                                'treedb_name': 'treedb_test',\n\
                                                'topic_name': 'users',\n\
                                                '__rowid__': 13,\n\
                                                '__t__': 1568455917,\n\
                                                '__tag__': 0,\n\
                                                '__pure_node__': true\n\
                                            }\n\
                                        },\n\
                                        {\n\
                                            'id': 'cda33bc2-d191-41e6-b779-471977869463',\n\
                                            'username': 'op1@email.com',\n\
                                            'firstName': 'Op1',\n\
                                            'lastName': 'Martínez',\n\
                                            'email': 'op1@email.com',\n\
                                            'emailVerified': false,\n\
                                            'disabled': true,\n\
                                            'online': false,\n\
                                            'departments': [\n\
                                                'departments^operation^users'\n\
                                            ],\n\
                                            'manager': [],\n\
                                            'attributes': [],\n\
                                            'roles': [],\n\
                                            '__md_treedb__': {\n\
                                                'treedb_name': 'treedb_test',\n\
                                                'topic_name': 'users',\n\
                                                '__rowid__': 12,\n\
                                                '__t__': 1568455917,\n\
                                                '__tag__': 0,\n\
                                                '__pure_node__': true\n\
                                            }\n\
                                        }\n\
                                    ],\n\
                                    'managers': {},\n\
                                    '__md_treedb__': {\n\
                                        'treedb_name': 'treedb_test',\n\
                                        'topic_name': 'departments',\n\
                                        '__rowid__': 5,\n\
                                        '__t__': 1568455917,\n\
                                        '__tag__': 0,\n\
                                        '__pure_node__': true\n\
                                    }\n\
                                }\n\
                            },\n\
                            'users': [\n\
                                {\n\
                                    'id': 'b6eb2c61-622c-41d3-afa0-548b3fa882cd',\n\
                                    'username': 'admin@email.com',\n\
                                    'firstName': 'Admin',\n\
                                    'lastName': 'Martínez',\n\
                                    'email': 'admin@email.com',\n\
                                    'emailVerified': false,\n\
                                    'disabled': true,\n\
                                    'online': false,\n\
                                    'departments': [\n\
                                        'departments^administration^users'\n\
                                    ],\n\
                                    'manager': [\n\
                                        'departments^administration^managers'\n\
                                    ],\n\
                                    'attributes': [],\n\
                                    'roles': [],\n\
                                    '__md_treedb__': {\n\
                                        'treedb_name': 'treedb_test',\n\
                                        'topic_name': 'users',\n\
                                        '__rowid__': 18,\n\
                                        '__t__': 1568455917,\n\
                                        '__tag__': 0,\n\
                                        '__pure_node__': true\n\
                                    }\n\
                                },\n\
                                {\n\
                                    'id': '9594f724-c029-4190-8311-a42438f74a99',\n\
                                    'username': 'escritor@email.com',\n\
                                    'firstName': 'Escritor',\n\
                                    'lastName': 'Martínez',\n\
                                    'email': 'escritor@email.com',\n\
                                    'emailVerified': false,\n\
                                    'disabled': true,\n\
                                    'online': false,\n\
                                    'departments': [\n\
                                        'departments^administration^users'\n\
                                    ],\n\
                                    'manager': [],\n\
                                    'attributes': [],\n\
                                    'roles': [],\n\
                                    '__md_treedb__': {\n\
                                        'treedb_name': 'treedb_test',\n\
                                        'topic_name': 'users',\n\
                                        '__rowid__': 11,\n\
                                        '__t__': 1568455917,\n\
                                        '__tag__': 0,\n\
                                        '__pure_node__': true\n\
                                    }\n\
                                }\n\
                            ],\n\
                            'managers': {\n\
                                'b6eb2c61-622c-41d3-afa0-548b3fa882cd': {\n\
                                    'id': 'b6eb2c61-622c-41d3-afa0-548b3fa882cd',\n\
                                    'username': 'admin@email.com',\n\
                                    'firstName': 'Admin',\n\
                                    'lastName': 'Martínez',\n\
                                    'email': 'admin@email.com',\n\
                                    'emailVerified': false,\n\
                                    'disabled': true,\n\
                                    'online': false,\n\
                                    'departments': [\n\
                                        'departments^administration^users'\n\
                                    ],\n\
                                    'manager': [\n\
                                        'departments^administration^managers'\n\
                                    ],\n\
                                    'attributes': [],\n\
                                    'roles': [],\n\
                                    '__md_treedb__': {\n\
                                        'treedb_name': 'treedb_test',\n\
                                        'topic_name': 'users',\n\
                                        '__rowid__': 18,\n\
                                        '__t__': 1568455917,\n\
                                        '__tag__': 0,\n\
                                        '__pure_node__': true\n\
                                    }\n\
                                }\n\
                            },\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'departments',\n\
                                '__rowid__': 3,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        }\n\
                    },\n\
                    'users': [\n\
                        {\n\
                            'id': 'ba115bf6-5a7c-4ac7-9852-ce32135b427f',\n\
                            'username': 'dueño@email.com',\n\
                            'firstName': 'Don',\n\
                            'lastName': 'Duenño',\n\
                            'email': 'dueño@email.com',\n\
                            'emailVerified': false,\n\
                            'disabled': true,\n\
                            'online': false,\n\
                            'departments': [\n\
                                'departments^direction^users'\n\
                            ],\n\
                            'manager': [\n\
                                'departments^direction^managers'\n\
                            ],\n\
                            'attributes': [],\n\
                            'roles': [],\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'users',\n\
                                '__rowid__': 17,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        }\n\
                    ],\n\
                    'managers': {\n\
                        'ba115bf6-5a7c-4ac7-9852-ce32135b427f': {\n\
                            'id': 'ba115bf6-5a7c-4ac7-9852-ce32135b427f',\n\
                            'username': 'dueño@email.com',\n\
                            'firstName': 'Don',\n\
                            'lastName': 'Duenño',\n\
                            'email': 'dueño@email.com',\n\
                            'emailVerified': false,\n\
                            'disabled': true,\n\
                            'online': false,\n\
                            'departments': [\n\
                                'departments^direction^users'\n\
                            ],\n\
                            'manager': [\n\
                                'departments^direction^managers'\n\
                            ],\n\
                            'attributes': [],\n\
                            'roles': [],\n\
                            '__md_treedb__': {\n\
                                'treedb_name': 'treedb_test',\n\
                                'topic_name': 'users',\n\
                                '__rowid__': 17,\n\
                                '__t__': 1568455917,\n\
                                '__tag__': 0,\n\
                                '__pure_node__': true\n\
                            }\n\
                        }\n\
                    },\n\
                    '__md_treedb__': {\n\
                        'treedb_name': 'treedb_test',\n\
                        'topic_name': 'departments',\n\
                        '__rowid__': 1,\n\
                        '__t__': 1568455917,\n\
                        '__tag__': 0,\n\
                        '__pure_node__': true\n\
                    }\n\
                }\n\
            }\n\
        },\n\
        'attributes': {\n\
            'id': {}\n\
        },\n\
        'roles': {\n\
            'id': {}\n\
        }\n\
    }\n\
}\n\
";

    if(!without_ok_tests) {
        const char *test = "departments: foto final 1";
        set_expected_results(
            test,
            json_pack("[]"  // error's list
            ),
            verbose
        );
        helper_quote2doublequote(foto_final);
        expected = legalstring2json(foto_final, TRUE);
        json_t *treedb = kw_get_dict(tranger, "treedbs", 0, 0);

        GBUFFER *gbuf_path = gbuf_create(32*1024, 32*1024, 0, 0);
        if(!match_record(treedb, expected, verbose, gbuf_path)) {
            ret = FALSE;
            if(verbose) {
                printf("%s  --> ERROR in test: '%s'%s\n", On_Red BWhite, test, Color_Off);
                log_debug_json(0, treedb, "Record found");
                log_debug_json(0, expected, "Record expected");
            } else {
                printf("%sX%s", On_Red BWhite, Color_Off);
            }
        } else {
            if(!check_log_result(test, verbose)) {
                ret = FALSE;
            }
        }
        GBUF_DESTROY(gbuf_path);
        JSON_DECREF(expected);
    }

    return ret;
}
