let FSM = {
    'states': {
        'ST_IDLE':
            [
                ['EV_TIMEOUT', null, 'ST_IDLE']
            ]
    },
    'event_types': [
        ['EV_TIMEOUT', 'output']
    ]
};

gclass_create(
    gclass_name,
    event_types,
    states,
    gmt,
    lmt,
    tattr_desc,
    priv_size,
    authz_table,
    command_table,
    s_user_trace_level,
    gclass_flag
);


export {
    FSM,
};
