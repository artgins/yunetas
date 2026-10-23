/****************************************************************************
 *          C_TEST1.C
 *
 *          A C_PROT_MODBUS_M master against a raw Modbus TCP slave.
 *
 *          The server is raw (C_PROT_RAW) and answers Modbus TCP by hand.
 *          The master polls two slaves:
 *          - slave 1: block A (holding 0, 2 registers: "a", uint32) and
 *            block B (holding 10, 1 register: "b", uint16);
 *          - slave 2: block C (holding 0, 1 register: "c", uint16).
 *
 *          A cycle starts at the request of block A. In cycles 1 and 2,
 *          block B answers exception 2 and slave 2 exception 11; in cycle 3
 *          every block answers. So:
 *          - cycle 1 publishes {slave_id 1, a} and nothing of slave 2, and
 *            ends at once. Up to 7.27.7 it also published b and c as 0 (the
 *            cells were never read), and over TCP each exception waited for
 *            a CRC that TCP does not carry: the cycle took timeout_response
 *            per exception;
 *          - each exception is logged once, when the block starts failing,
 *            and "Modbus block answered again" when it answers (cycle 3);
 *          - on the message of cycle 1 a write is queued: the slave must see
 *            it once, and the polling must go on after it. Up to 7.27.7 the
 *            write was released while the queue still held it (BAD
 *            json_decref()) and the polling stopped.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <c_prot_modbus_m.h>
#include "c_test1.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SLAVE_URL       "tcp://127.0.0.1:7759"
#define CYCLE_MAX_MS    800     // a cycle answered at once; an exception that waits costs 1 s
#define WRITE_ADDRESS   20
#define WRITE_VALUE     7
#define RX_SIZE         512

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE char slaves_config[] = "\
[                                                                   \n\
    {                                                               \n\
        'id': 1,                                                    \n\
        'mapping': [                                                \n\
            {'type': 'holding_register', 'address': 0, 'size': 2},   \n\
            {'type': 'holding_register', 'address': 10, 'size': 1}   \n\
        ],                                                          \n\
        'conversion': [                                             \n\
            {'id': 'a', 'type': 'holding_register', 'address': 0, 'format': 'uint32'}, \n\
            {'id': 'b', 'type': 'holding_register', 'address': 10, 'format': 'uint16'} \n\
        ]                                                           \n\
    },                                                              \n\
    {                                                               \n\
        'id': 2,                                                    \n\
        'mapping': [                                                \n\
            {'type': 'holding_register', 'address': 0, 'size': 1}    \n\
        ],                                                          \n\
        'conversion': [                                             \n\
            {'id': 'c', 'type': 'holding_register', 'address': 0, 'format': 'uint16'} \n\
        ]                                                           \n\
    }                                                               \n\
]                                                                   \n\
";

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description--*/
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",     "Timeout"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    json_int_t timeout;
    hgobj timer;

    hgobj gobj_input_side;
    hgobj gobj_master;
    hgobj server_channel;       // the channel of the server, where the master connected

    uint8_t rx[RX_SIZE];        // what the slave received and did not answer yet
    size_t rx_len;

    int cycle;                  // counted at the slave, by the requests of block A
    uint64_t t_cycle;           // CYCLE_MAX_MS from the request of block A
    int writes_seen;            // at the slave: must be 1
    BOOL write_queued;
    BOOL done;
} PRIVATE_DATA;





                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************************
 *      Framework Method create
 ***************************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_master && gobj_is_running(priv->gobj_master)) {
        gobj_stop(priv->gobj_master);
    }
    gobj_stop(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, NULL, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);

    set_timeout(priv->timer, priv->timeout); // then the master connects

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}




                    /***************************
                     *      Commands
                     ***************************/




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Send an answer from the slave
 ***************************************************************************/
PRIVATE void slave_send(hgobj gobj, const uint8_t *bf, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(len, len);
    gbuffer_append(gbuf, (void *)bf, len);
    gobj_send_event(
        priv->server_channel,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
}

/***************************************************************************
 *  An exception answer to a request
 ***************************************************************************/
PRIVATE void slave_exception(hgobj gobj, const uint8_t *req, int code)
{
    uint8_t rsp[9];
    memcpy(rsp, req, 2);                // transaction id
    rsp[2] = 0;                         // protocol
    rsp[3] = 0;
    rsp[4] = 0;                         // length: unit, function, code
    rsp[5] = 3;
    rsp[6] = req[6];                    // unit
    rsp[7] = (uint8_t)(req[7] | 0x80);  // function with the exception bit
    rsp[8] = (uint8_t)code;
    slave_send(gobj, rsp, sizeof(rsp));
}

/***************************************************************************
 *  The registers of a read
 ***************************************************************************/
PRIVATE void slave_registers(hgobj gobj, const uint8_t *req, const uint16_t *regs, int n)
{
    uint8_t rsp[9 + 2*8];
    memcpy(rsp, req, 2);
    rsp[2] = 0;
    rsp[3] = 0;
    rsp[4] = 0;
    rsp[5] = (uint8_t)(3 + 2*n);
    rsp[6] = req[6];
    rsp[7] = req[7];
    rsp[8] = (uint8_t)(2*n);
    for(int i=0; i<n; i++) {
        rsp[9 + 2*i] = (uint8_t)(regs[i] >> 8);
        rsp[10 + 2*i] = (uint8_t)(regs[i] & 0xFF);
    }
    slave_send(gobj, rsp, (size_t)(9 + 2*n));
}

/***************************************************************************
 *  One request at the slave
 ***************************************************************************/
PRIVATE void slave_request(hgobj gobj, const uint8_t *req)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int unit = req[6];
    int function = req[7];
    int address = (req[8] << 8) | req[9];
    int quantity = (req[10] << 8) | req[11];   // the value, in a write

    if(function == 6) {
        priv->writes_seen++;
        if(address != WRITE_ADDRESS || quantity != WRITE_VALUE || priv->writes_seen != 1) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The write did not reach the slave once, as sent",
                "address",      "%d", address,
                "value",        "%d", quantity,
                "writes_seen",  "%d", priv->writes_seen,
                NULL
            );
        }
        slave_send(gobj, req, 12);  // a write is answered with its echo
        return;
    }

    if(function != 3) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Unexpected function at the slave",
            "modbus_function", "%d", function,
            NULL
        );
        slave_exception(gobj, req, 1);
        return;
    }

    if(unit == 1 && address == 0 && quantity == 2) {
        priv->cycle++;
        priv->t_cycle = start_msectimer(CYCLE_MAX_MS);
        uint16_t regs[2] = {0, 0};
        switch(priv->cycle) {
            case 1:
                regs[0] = 0x0001;
                regs[1] = 0x0002;
                break;
            case 2:
                regs[1] = 3;
                break;
            default:
                regs[1] = 4;
                break;
        }
        slave_registers(gobj, req, regs, 2);

    } else if(unit == 1 && address == 10 && quantity == 1) {
        if(priv->cycle < 3) {
            slave_exception(gobj, req, 2);      // illegal data address
        } else {
            uint16_t regs[1] = {9};
            slave_registers(gobj, req, regs, 1);
        }

    } else if(unit == 2 && address == 0 && quantity == 1) {
        if(priv->cycle < 3) {
            slave_exception(gobj, req, 11);     // gateway target device failed to respond
        } else {
            uint16_t regs[1] = {5};
            slave_registers(gobj, req, regs, 1);
        }

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Unexpected read at the slave",
            "unit",         "%d", unit,
            "address",      "%d", address,
            "quantity",     "%d", quantity,
            NULL
        );
        slave_exception(gobj, req, 2);
    }
}

/***************************************************************************
 *  Bytes at the slave: answer every whole request
 ***************************************************************************/
PRIVATE void slave_rx(hgobj gobj, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
    if(!gbuf) {
        // Error already logged
        return;
    }
    size_t len = gbuffer_leftbytes(gbuf);
    if(priv->rx_len + len > sizeof(priv->rx)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The slave got more than it can hold",
            "len",          "%lu", (unsigned long)len,
            NULL
        );
        return;
    }
    memcpy(priv->rx + priv->rx_len, gbuffer_cur_rd_pointer(gbuf), len);
    priv->rx_len += len;

    while(priv->rx_len >= 6) {
        size_t frame_len = 6 + (size_t)((priv->rx[4] << 8) | priv->rx[5]);
        if(frame_len != 12) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A request of an unexpected length",
                "frame_len",    "%lu", (unsigned long)frame_len,
                NULL
            );
            priv->rx_len = 0;
            return;
        }
        if(priv->rx_len < frame_len) {
            return;
        }
        slave_request(gobj, priv->rx);
        memmove(priv->rx, priv->rx + frame_len, priv->rx_len - frame_len);
        priv->rx_len -= frame_len;
    }
}

/***************************************************************************
 *  A message of the master must be what is expected
 ***************************************************************************/
PRIVATE BOOL check_message(hgobj gobj, json_t *kw, const char *expected_)
{
    json_t *expected = string2json(expected_, TRUE);
    BOOL ok = json_equal(kw, expected)? TRUE : FALSE;
    if(!ok) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The master published what the cycle did not read",
            "published",    "%j", kw,
            "expected",     "%j", expected?expected:json_null(),
            NULL
        );
    }
    JSON_DECREF(expected)
    return ok;
}

/***************************************************************************
 *  A message of the master, at the end of a cycle
 ***************************************************************************/
PRIVATE void master_message(hgobj gobj, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->done) {
        return;
    }

    if(test_msectimer(priv->t_cycle)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A cycle with exceptions did not end at once",
            "cycle",        "%d", priv->cycle,
            "max_ms",       "%d", CYCLE_MAX_MS,
            NULL
        );
    }

    int slave_id = (int)kw_get_int(gobj, kw, "slave_id", 0, KW_REQUIRED);

    switch(priv->cycle) {
        case 1:
            check_message(gobj, kw, "{\"slave_id\": 1, \"a\": 65538}");
            if(!priv->write_queued) {
                priv->write_queued = TRUE;
                gobj_send_event(priv->gobj_master, EV_SEND_MESSAGE,
                    json_pack("{s:i, s:s, s:i, s:i}",
                        "id", 1,
                        "type", "holding_register",
                        "address", WRITE_ADDRESS,
                        "value", WRITE_VALUE
                    ),
                    gobj
                );
            }
            break;

        case 2:
            check_message(gobj, kw, "{\"slave_id\": 1, \"a\": 3}");
            break;

        default:
            if(slave_id == 1) {
                check_message(gobj, kw, "{\"slave_id\": 1, \"a\": 4, \"b\": 9}");
            } else {
                check_message(gobj, kw, "{\"slave_id\": 2, \"c\": 5}");
                if(priv->writes_seen != 1) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "The queued write did not reach the slave once",
                        "writes_seen",  "%d", priv->writes_seen,
                        NULL
                    );
                }
                priv->done = TRUE;
                set_yuno_must_die();
            }
            break;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The server listens: the master is created, and connects
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->gobj_master) {
        helper_quote2doublequote(slaves_config);
        json_t *jn_slaves = string2json(slaves_config, TRUE);
        priv->gobj_master = gobj_create(
            "modbus",
            C_PROT_MODBUS_M,
            json_pack("{s:s, s:s, s:i, s:i, s:o}",
                "modbus_protocol", "TCP",
                "url", SLAVE_URL,
                "timeout_polling", 1000,
                "timeout_response", 1,
                "slaves", jn_slaves
            ),
            gobj
        );
        gobj_start(priv->gobj_master);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A message: at the server, the master's requests; from the master, what
 *  a cycle read
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        slave_rx(gobj, kw);
    } else if(src == priv->gobj_master) {
        master_message(gobj, kw);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        priv->server_channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
        priv->rx_len = 0;
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A close before the end is a failure: the master must not drop
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->done && !gobj_is_shutdowning()) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The connection closed before the third cycle",
            "src",          "%s", gobj_short_name(src),
            "cycle",        "%d", priv->cycle,
            NULL
        );
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)

    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }

    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_play = mt_play,
    .mt_pause = mt_pause,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_TEST1);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_ON_OPEN,                ac_on_open,                 0},
        {EV_ON_MESSAGE,             ac_on_message,              0},
        {EV_ON_CLOSE,               ac_on_close,                0},
        {EV_TIMEOUT,                ac_timeout,                 0},
        {EV_STOPPED,                ac_stopped,                 0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_OPEN,                0},
        {EV_ON_MESSAGE,             0},
        {EV_ON_CLOSE,               0},
        {EV_TIMEOUT,                0},
        {EV_STOPPED,                0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,  // s_user_trace_level,
        0   // gcflag_t
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_test1(void)
{
    return create_gclass(C_TEST1);
}
