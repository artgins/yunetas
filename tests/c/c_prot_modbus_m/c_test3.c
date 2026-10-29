/****************************************************************************
 *          C_TEST3.C
 *
 *          A C_PROT_MODBUS_M master with RTU framing and no block to poll,
 *          against a raw slave (REV-CLOUD V48.2).
 *
 *          The master is given its own C_TCP (as a gate does), and the raw
 *          server (C_PROT_RAW) answers RTU frames by hand, CRC included. Its
 *          map holds only slave 0, the RTU broadcast: reported once at start
 *          and never polled. So every tick of timeout_polling ends a cycle
 *          that polled nothing, as a master that only serves requests does.
 *          (C_TIMER counts in seconds: a tick is a second, whatever less is
 *          asked.)
 *
 *          - Three reads are sent, REQUEST_PERIOD_MS apart (two empty cycles
 *            or more between two answers),
 *            and the slave answers each with a noise byte after it, in the
 *            same bytes. The noise is dropped and counted (rx_noise_dropped)
 *            and warned ONCE: a cycle that polled nothing is not a cycle
 *            "without noise". Up to the fix every noisy answer was a warning
 *            and an info ("a cycle came without bytes after its answers"):
 *            the empty cycles between two answers ended the noise.
 *          - The noise of an answer is counted before its answer is
 *            published: on the answer of read n rx_noise_dropped is n.
 *          - On the answer of the last read the test stops the master,
 *            inside its EV_MODBUS_ANSWER. Nothing more is polled and nothing
 *            is logged for it. Up to the fix the master went on with its
 *            polling after the answer, on a stopped gobj: "cur_slave_ NULL",
 *            once there and once more at the next tick.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <c_prot_modbus_m.h>
#include "c_test3.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define SLAVE_URL           "tcp://127.0.0.1:7764"
#define TIMEOUT_POLLING     1000    // ms: the empty cycles of the master
#define REQUEST_PERIOD_MS   3000    // between an answer and the next read: 2 empty cycles or more
#define STOPPED_WAIT_MS     2000    // after the stop: ticks the master must not take
#define N_REQUESTS          3
#define FIRST_ADDRESS       70      // the reads ask 70, 71, ...: the slave answers the address
#define RX_SIZE             512

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
        'id': 0,                                                    \n\
        'mapping': [                                                \n\
            {'type': 'holding_register', 'address': 0, 'size': 1}    \n\
        ],                                                          \n\
        'conversion': [                                             \n\
            {'id': 'y', 'type': 'holding_register', 'address': 0, 'format': 'uint16'} \n\
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

    int requests_sent;
    int answers_seen;
    BOOL done;                  // the master was stopped: the test ends at the next timeout
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
 *  The CRC of Modbus RTU (CRC-16, polynomial 0xA001 reflected, from 0xFFFF),
 *  computed bit by bit: not the master's tables
 ***************************************************************************/
PRIVATE uint16_t rtu_crc(const uint8_t *bf, size_t len)
{
    uint16_t crc = 0xFFFF;
    for(size_t i=0; i<len; i++) {
        crc ^= bf[i];
        for(int bit=0; bit<8; bit++) {
            if(crc & 1) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001);
            } else {
                crc = (uint16_t)(crc >> 1);
            }
        }
    }
    return crc;
}

/***************************************************************************
 *  One whole request at the slave, its CRC already checked: a read of one
 *  register is answered with its address, and a noise byte after the
 *  answer, in the same bytes
 ***************************************************************************/
PRIVATE void slave_request(hgobj gobj, const uint8_t *req)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int unit = req[0];
    int function = req[1];
    int address = (req[2] << 8) | req[3];
    int quantity = (req[4] << 8) | req[5];

    if(priv->done) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A request reached the slave after the master stopped",
            "unit",         "%d", unit,
            "modbus_function", "%d", function,
            NULL
        );
        return;
    }
    if(unit != 1 || function != 3 || quantity != 1 ||
            address < FIRST_ADDRESS || address >= FIRST_ADDRESS + N_REQUESTS) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Unexpected request at the slave",
            "unit",         "%d", unit,
            "modbus_function", "%d", function,
            "address",      "%d", address,
            "quantity",     "%d", quantity,
            NULL
        );
        return;
    }

    uint8_t rsp[5 + 2 + 1] = {req[0], req[1], 2, 0, (uint8_t)address};
    uint16_t crc = rtu_crc(rsp, 5);
    rsp[5] = (uint8_t)(crc & 0xFF);
    rsp[6] = (uint8_t)(crc >> 8);
    rsp[7] = 0x00;  // the noise: what a RS485 line can carry when the slave releases it

    gbuffer_t *gbuf = gbuffer_create(sizeof(rsp), sizeof(rsp));
    gbuffer_append(gbuf, rsp, sizeof(rsp));
    gobj_send_event(
        priv->server_channel,
        EV_SEND_MESSAGE,
        json_pack("{s:I}", "gbuffer", (json_int_t)(uintptr_t)gbuf),
        gobj
    );
}

/***************************************************************************
 *  Bytes at the slave: answer every whole request (8 bytes: only reads
 *  of function 03 are expected)
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

    while(priv->rx_len >= 8) {
        uint16_t crc = rtu_crc(priv->rx, 6);
        if(priv->rx[6] != (crc & 0xFF) || priv->rx[7] != (crc >> 8)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A request with a bad CRC at the slave",
                "modbus_function", "%d", priv->rx[1],
                NULL
            );
        } else {
            slave_request(gobj, priv->rx);
        }
        memmove(priv->rx, priv->rx + 8, priv->rx_len - 8);
        priv->rx_len -= 8;
    }
}

/***************************************************************************
 *  Send the next read to the master
 ***************************************************************************/
PRIVATE void send_next_request(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_request = json_pack("{s:s, s:s, s:i, s:i}",
        "operation", "read",
        "type", "holding_register",
        "address", FIRST_ADDRESS + priv->requests_sent,
        "id", 1
    );
    priv->requests_sent++;
    gobj_send_event(priv->gobj_master, EV_SEND_MESSAGE, jn_request, gobj);
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  First the master is created over its own C_TCP, and connects. Then the
 *  reads go, one per timeout. After the stop of the master, the end.
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
            json_pack("{s:s, s:i, s:i, s:o}",
                "modbus_protocol", "RTU",
                "timeout_polling", TIMEOUT_POLLING,
                "timeout_response", 1,
                "slaves", jn_slaves
            ),
            gobj
        );
        hgobj gobj_tcp = gobj_create_pure_child(
            "modbus",
            C_TCP,
            json_pack("{s:s}", "url", SLAVE_URL),
            priv->gobj_master
        );
        gobj_set_bottom_gobj(priv->gobj_master, gobj_tcp);
        gobj_start(priv->gobj_master);

    } else if(priv->done) {
        if(gobj_is_running(priv->gobj_master)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The master runs after it was stopped",
                NULL
            );
        }
        set_yuno_must_die();

    } else if(priv->requests_sent < N_REQUESTS) {
        send_next_request(gobj);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The answer of a read: the next one after a while, and on the last one
 *  the master is stopped, here, inside its publication
 ***************************************************************************/
PRIVATE int ac_modbus_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int address = (int)kw_get_int(gobj, kw, "request`address", -1, KW_REQUIRED);
    json_t *expected = json_pack("{s:i, s:i, s:[i]}",
        "result", 0,
        "error_code", 0,
        "values", address
    );
    json_t *got = json_pack("{s:O, s:O, s:O}",
        "result", json_object_get(kw, "result"),
        "error_code", json_object_get(kw, "error_code"),
        "values", json_object_get(kw, "values")? json_object_get(kw, "values") : json_null()
    );
    if(address != FIRST_ADDRESS + priv->answers_seen || !json_equal(got, expected)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "A read was not answered as expected",
            "got",          "%j", got,
            "expected",     "%j", expected,
            "answer",       "%j", kw,
            NULL
        );
    }
    JSON_DECREF(got)
    JSON_DECREF(expected)
    priv->answers_seen++;

    json_int_t noise = gobj_read_integer_attr(priv->gobj_master, "rx_noise_dropped");
    if(noise != priv->answers_seen) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The noise of the answer was not counted before it was published",
            "rx_noise_dropped", "%lld", (long long)noise,
            "expected",     "%d", priv->answers_seen,
            NULL
        );
    }

    if(priv->answers_seen < N_REQUESTS) {
        set_timeout(priv->timer, REQUEST_PERIOD_MS);
    } else {
        /*
         *  On purpose: the master stopped inside its own EV_MODBUS_ANSWER,
         *  the case its receive path guards against
         */
        priv->done = TRUE;
        gobj_stop(priv->gobj_master);
        set_timeout(priv->timer, STOPPED_WAIT_MS);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A message: at the server, the master's requests; from the master, a
 *  cycle with something read, which cannot happen here
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        slave_rx(gobj, kw);
    } else if(src == priv->gobj_master) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The master published a cycle with nothing to poll",
            "published",    "%j", kw,
            NULL
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The server: the channel the master connected on. The master: connected,
 *  the reads begin.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        priv->server_channel = (hgobj)(uintptr_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, KW_REQUIRED);
        priv->rx_len = 0;
    } else if(src == priv->gobj_master) {
        set_timeout(priv->timer, REQUEST_PERIOD_MS);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A close before the master was stopped is a failure
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->done && !gobj_is_shutdowning()) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The connection closed before the master was stopped",
            "src",          "%s", gobj_short_name(src),
            "answers_seen", "%d", priv->answers_seen,
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
GOBJ_DEFINE_GCLASS(C_TEST3);

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
        {EV_MODBUS_ANSWER,          ac_modbus_answer,           0},
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
        {EV_MODBUS_ANSWER,          0},
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
PUBLIC int register_c_test3(void)
{
    return create_gclass(C_TEST3);
}
