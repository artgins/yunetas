/*------------------------------------------------------------*
 *          Foto final
 *------------------------------------------------------------*/
char foto_final_departments[]= "\
{ \n\
    'treedb_test': { \n\
        '__schema_version__': 0, \n\
        '__snaps__': { \n\
            'id': {} \n\
        }, \n\
        '__graphs__': { \n\
            'id': {} \n\
        }, \n\
        'users': { \n\
            'id': {} \n\
        }, \n\
        'departments': { \n\
            'id': { \n\
                'direction': { \n\
                    'id': 'direction', \n\
                    'name': 'Dirección', \n\
                    'department_id': '', \n\
                    'departments': { \n\
                        'administration': { \n\
                            'id': 'administration', \n\
                            'name': 'Administración', \n\
                            'department_id': 'departments^direction^departments', \n\
                            'departments': { \n\
                                'operation': { \n\
                                    'id': 'operation', \n\
                                    'name': 'Gestión', \n\
                                    'department_id': 'departments^administration^departments', \n\
                                    'departments': {}, \n\
                                    'users': [], \n\
                                    'managers': {}, \n\
                                    '__md_treedb__': { \n\
                                        'treedb_name': 'treedb_test', \n\
                                        'topic_name': 'departments', \n\
                                        'g_rowid': 1, \n\
                                        'i_rowid': 1, \n\
                                        't': 99999, \n\
                                        'tm': 0, \n\
                                        'pure_node': true, \n\
                                        'tag': 0 \n\
                                    } \n\
                                }, \n\
                                'development': { \n\
                                    'id': 'development', \n\
                                    'name': 'Desarrollo', \n\
                                    'department_id': 'departments^administration^departments', \n\
                                    'departments': {}, \n\
                                    'users': [], \n\
                                    'managers': {}, \n\
                                    '__md_treedb__': { \n\
                                        'treedb_name': 'treedb_test', \n\
                                        'topic_name': 'departments', \n\
                                        'g_rowid': 1, \n\
                                        'i_rowid': 1, \n\
                                        't': 99999, \n\
                                        'tm': 0, \n\
                                        'pure_node': true, \n\
                                        'tag': 0 \n\
                                    } \n\
                                } \n\
                            }, \n\
                            'users': [], \n\
                            'managers': {}, \n\
                            '__md_treedb__': { \n\
                                'treedb_name': 'treedb_test', \n\
                                'topic_name': 'departments', \n\
                                'g_rowid': 1, \n\
                                'i_rowid': 1, \n\
                                't': 99999, \n\
                                'tm': 0, \n\
                                'pure_node': true, \n\
                                'tag': 0 \n\
                            } \n\
                        } \n\
                    }, \n\
                    'users': [], \n\
                    'managers': {}, \n\
                    '__md_treedb__': { \n\
                        'treedb_name': 'treedb_test', \n\
                        'topic_name': 'departments', \n\
                        'g_rowid': 1, \n\
                        'i_rowid': 1, \n\
                        't': 99999, \n\
                        'tm': 0, \n\
                        'pure_node': true, \n\
                        'tag': 0 \n\
                    } \n\
                }, \n\
                'administration': { \n\
                    'id': 'administration', \n\
                    'name': 'Administración', \n\
                    'department_id': 'departments^direction^departments', \n\
                    'departments': { \n\
                        'operation': { \n\
                            'id': 'operation', \n\
                            'name': 'Gestión', \n\
                            'department_id': 'departments^administration^departments', \n\
                            'departments': {}, \n\
                            'users': [], \n\
                            'managers': {}, \n\
                            '__md_treedb__': { \n\
                                'treedb_name': 'treedb_test', \n\
                                'topic_name': 'departments', \n\
                                'g_rowid': 1, \n\
                                'i_rowid': 1, \n\
                                't': 99999, \n\
                                'tm': 0, \n\
                                'pure_node': true, \n\
                                'tag': 0 \n\
                            } \n\
                        }, \n\
                        'development': { \n\
                            'id': 'development', \n\
                            'name': 'Desarrollo', \n\
                            'department_id': 'departments^administration^departments', \n\
                            'departments': {}, \n\
                            'users': [], \n\
                            'managers': {}, \n\
                            '__md_treedb__': { \n\
                                'treedb_name': 'treedb_test', \n\
                                'topic_name': 'departments', \n\
                                'g_rowid': 1, \n\
                                'i_rowid': 1, \n\
                                't': 99999, \n\
                                'tm': 0, \n\
                                'pure_node': true, \n\
                                'tag': 0 \n\
                            } \n\
                        } \n\
                    }, \n\
                    'users': [], \n\
                    'managers': {}, \n\
                    '__md_treedb__': { \n\
                        'treedb_name': 'treedb_test', \n\
                        'topic_name': 'departments', \n\
                        'g_rowid': 1, \n\
                        'i_rowid': 1, \n\
                        't': 99999, \n\
                        'tm': 0, \n\
                        'pure_node': true, \n\
                        'tag': 0 \n\
                    } \n\
                }, \n\
                'operation': { \n\
                    'id': 'operation', \n\
                    'name': 'Gestión', \n\
                    'department_id': 'departments^administration^departments', \n\
                    'departments': {}, \n\
                    'users': [], \n\
                    'managers': {}, \n\
                    '__md_treedb__': { \n\
                        'treedb_name': 'treedb_test', \n\
                        'topic_name': 'departments', \n\
                        'g_rowid': 1, \n\
                        'i_rowid': 1, \n\
                        't': 99999, \n\
                        'tm': 0, \n\
                        'pure_node': true, \n\
                        'tag': 0 \n\
                    } \n\
                }, \n\
                'development': { \n\
                    'id': 'development', \n\
                    'name': 'Desarrollo', \n\
                    'department_id': 'departments^administration^departments', \n\
                    'departments': {}, \n\
                    'users': [], \n\
                    'managers': {}, \n\
                    '__md_treedb__': { \n\
                        'treedb_name': 'treedb_test', \n\
                        'topic_name': 'departments', \n\
                        'g_rowid': 1, \n\
                        'i_rowid': 1, \n\
                        't': 99999, \n\
                        'tm': 0, \n\
                        'pure_node': true, \n\
                        'tag': 0 \n\
                    } \n\
                } \n\
            } \n\
        }, \n\
        'attributes': { \n\
            'id': {} \n\
        }, \n\
        'roles': { \n\
            'id': {} \n\
        } \n\
    } \n\
} \n\
";
