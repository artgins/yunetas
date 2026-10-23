/****************************************************************************
 *          C_TEST1.C
 *
 *          A C_PROT_MBUS_M master against a simulated M-Bus.
 *
 *          The server is raw (C_PROT_RAW), as a transparent M-Bus to TCP
 *          gateway would be, and this gclass answers as the meters of the
 *          bus. The master reads:
 *          - slave 1 (primary): SND_NKE -> E5, then two telegrams: the
 *            first ends with DIF 1F (more records follow). The first
 *            REQ_UD2 must carry FCB 1 (7B) and the second FCB 0 (5B);
 *          - slave 2 (primary): silent in cycles 1 and 2. In cycle 3 it
 *            answers the SND_NKE, and its first RSP_UD has a bad checksum:
 *            the master waits out the timeout and repeats the REQ_UD2 with
 *            the SAME FCB, and the slave answers it well;
 *          - slave 12345678 KAM (secondary): selected every cycle (CI 52),
 *            then read at FD;
 *          - slave 3 is disabled: it must never be asked.
 *
 *          Checked:
 *          - each cycle publishes what it read and only that: slave 2
 *            publishes nothing in cycles 1 and 2;
 *          - slave 2 is logged once, as a warning, in cycle 1, not in cycle
 *            2, and as answered again in cycle 3;
 *          - the readings, the FCBs, the selection bytes, and the stats.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <c_prot_mbus_m.h>
#include <mbus_telegram.h>
#include "c_test1.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BUS_URL             "tcp://127.0.0.1:7761"
#define RX_SIZE             512
#define LAST_CYCLE          3
#define SECONDARY_SELECTION "78 56 34 12 2D 2C FF FF"     // 12345678, KAM, any version and medium

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
    {'address': 1},                                                 \n\
    {'address': 2},                                                 \n\
    {'secondary': '12345678', 'manufacturer': 'KAM'},               \n\
    {'address': 3, 'disabled': true}                                \n\
]                                                                   \n\
";

/*
 *  What each cycle must publish, in order
 */
PRIVATE const char *slave1_reading = "{\"address\": 1, \"records\": {"
    "\"volume\": {\"value\": 12.345, \"unit\": \"m3\"}, "
    "\"flow_temperature\": {\"value\": 21.5, \"unit\": \"C\"}}, \"telegrams\": 2}";
PRIVATE const char *slave2_reading = "{\"address\": 2, \"records\": {"
    "\"power\": {\"value\": 7, \"unit\": \"W\"}}, \"telegrams\": 1}";
PRIVATE const char *secondary_reading = "{\"secondary\": \"12345678\", "
    "\"header\": {\"id\": \"12345678\", \"manufacturer\": \"KAM\", \"version\": 1, "
    "\"medium\": \"water\", \"medium_code\": 7, \"access_no\": 1, \"status\": 0}, "
    "\"records\": {\"volume\": {\"value\": 1.0, \"unit\": \"m3\"}}, \"telegrams\": 1}";

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

    uint8_t rx[RX_SIZE];        // what the bus received and did not answer yet
    size_t rx_len;

    int cycle;                  // counted at the bus, by the SND_NKE to slave 1
    int slave1_requests;        // REQ_UD2 to slave 1 in this cycle
    int slave2_requests;        // REQ_UD2 to slave 2 in this cycle
    int selections;             // selections of 12345678 seen
    BOOL selected;              // 12345678 is selected
    int messages;               // readings published in this cycle
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
 *  A test failure: an error, so the strict list of logs fails the test
 ***************************************************************************/
PRIVATE void bus_error(hgobj gobj, const char *msg, int value)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", msg,
        "value",        "%d", value,
        "cycle",        "%d", priv->cycle,
        NULL
    );
}

/***************************************************************************
 *  Send bytes from the bus
 ***************************************************************************/
PRIVATE void bus_send(hgobj gobj, const uint8_t *bf, size_t len)
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

PRIVATE void bus_ack(hgobj gobj)
{
    uint8_t ack = 0xE5;
    bus_send(gobj, &ack, 1);
}

/*
 *  An RSP_UD with these user data (CI and the bytes after it, in hex)
 */
PRIVATE void bus_rsp_ud(hgobj gobj, uint8_t a, uint8_t ci, const char *hex, BOOL bad_checksum)
{
    uint8_t data[MBUS_FRAME_MAX_LENGTH];
    size_t n = 0;
    const char *p = hex;
    unsigned v;
    int consumed;
    while(n < sizeof(data) && sscanf(p, " %2x%n", &v, &consumed) == 1) {
        data[n++] = (uint8_t)v;
        p += consumed;
    }

    uint8_t bf[MBUS_FRAME_MAX_LENGTH];
    size_t len = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_RSP_UD, a, ci, data, n);
    if(bad_checksum) {
        bf[len - 2] = (uint8_t)(bf[len - 2] + 1);
    }
    bus_send(gobj, bf, len);
}

/***************************************************************************
 *  A frame from the master, at the bus
 ***************************************************************************/
PRIVATE void bus_frame(hgobj gobj, const mbus_frame_t *frame, const uint8_t *raw, size_t raw_len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(frame->type == MBUS_FRAME_SHORT && frame->c == MBUS_C_SND_NKE) {
        switch(frame->a) {
            case 1:
                priv->cycle++;
                priv->slave1_requests = 0;
                priv->slave2_requests = 0;
                priv->messages = 0;
                bus_ack(gobj);
                return;
            case 2:
                if(priv->cycle >= LAST_CYCLE) {
                    bus_ack(gobj);
                }
                return;     // silent in cycles 1 and 2
            default:
                bus_error(gobj, "SND_NKE to a slave that is not on the bus (or disabled)", frame->a);
                return;
        }
    }

    if(frame->type == MBUS_FRAME_LONG && frame->c == MBUS_C_SND_UD &&
            frame->a == MBUS_ADDRESS_SELECTED && frame->ci == MBUS_CI_SELECT_SECONDARY) {
        uint8_t want[MBUS_SECONDARY_ADDRESS_SIZE];
        unsigned v[8];
        sscanf(SECONDARY_SELECTION, "%x %x %x %x %x %x %x %x",
            &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
        for(int i=0; i<8; i++) {
            want[i] = (uint8_t)v[i];
        }
        if(frame->data_len == sizeof(want) && memcmp(frame->data, want, sizeof(want)) == 0) {
            priv->selections++;
            priv->selected = TRUE;
            bus_ack(gobj);
        } else {
            priv->selected = FALSE;
            bus_error(gobj, "A selection that is not the one configured", (int)frame->data_len);
        }
        return;
    }

    if(frame->type == MBUS_FRAME_SHORT && (frame->c & ~MBUS_C_FCB) == MBUS_C_REQ_UD2) {
        switch(frame->a) {
            case 1:
                priv->slave1_requests++;
                if(priv->slave1_requests == 1) {
                    if(frame->c != (MBUS_C_REQ_UD2|MBUS_C_FCB)) {
                        bus_error(gobj, "The first REQ_UD2 must carry FCB 1", frame->c);
                    }
                    bus_rsp_ud(gobj, 1, MBUS_CI_RESP_NO_HEADER, "04 13 39 30 00 00 1F", FALSE);
                } else if(priv->slave1_requests == 2) {
                    if(frame->c != MBUS_C_REQ_UD2) {
                        bus_error(gobj, "The REQ_UD2 of the second telegram must toggle the FCB", frame->c);
                    }
                    bus_rsp_ud(gobj, 1, MBUS_CI_RESP_NO_HEADER, "02 5A D7 00", FALSE);
                } else {
                    bus_error(gobj, "A third REQ_UD2 to slave 1 in a cycle", priv->slave1_requests);
                }
                return;

            case 2:
                if(priv->cycle < LAST_CYCLE) {
                    bus_error(gobj, "REQ_UD2 to slave 2 though it did not answer the SND_NKE", frame->c);
                    return;
                }
                priv->slave2_requests++;
                if(frame->c != (MBUS_C_REQ_UD2|MBUS_C_FCB)) {
                    bus_error(gobj, "A repeated REQ_UD2 must keep its FCB", frame->c);
                }
                bus_rsp_ud(gobj, 2, MBUS_CI_RESP_NO_HEADER, "01 2B 07",
                    priv->slave2_requests == 1  // the first answer is broken
                );
                return;

            case MBUS_ADDRESS_SELECTED:
                if(!priv->selected) {
                    bus_error(gobj, "REQ_UD2 to FD with no slave selected", frame->c);
                    return;
                }
                priv->selected = FALSE;
                bus_rsp_ud(gobj, 0, MBUS_CI_RESP_VARIABLE,
                    "78 56 34 12 2D 2C 01 07 01 00 00 00 04 13 E8 03 00 00", FALSE
                );
                return;

            default:
                bus_error(gobj, "REQ_UD2 to a slave that is not on the bus (or disabled)", frame->a);
                return;
        }
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "A frame the bus does not expect",
        "type",         "%d", (int)frame->type,
        "c",            "%d", frame->c,
        "a",            "%d", frame->a,
        "len",          "%d", (int)raw_len,
        NULL
    );
}

/***************************************************************************
 *  Bytes at the bus: answer every whole frame
 ***************************************************************************/
PRIVATE void bus_rx(hgobj gobj, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
    if(!gbuf) {
        // Error already logged
        return;
    }
    size_t len = gbuffer_leftbytes(gbuf);
    if(priv->rx_len + len > sizeof(priv->rx)) {
        bus_error(gobj, "The bus got more than it can hold", (int)len);
        priv->rx_len = 0;
        return;
    }
    memcpy(priv->rx + priv->rx_len, gbuffer_cur_rd_pointer(gbuf), len);
    priv->rx_len += len;

    while(priv->rx_len > 0) {
        mbus_frame_t frame;
        char why[256] = "";
        int r = mbus_parse_frame(priv->rx, priv->rx_len, &frame, why, sizeof(why));
        if(r == 0) {
            return;
        }
        if(r < 0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "The master sent a bad frame",
                "why",          "%s", why,
                NULL
            );
            priv->rx_len = 0;
            return;
        }
        bus_frame(gobj, &frame, priv->rx, (size_t)r);
        memmove(priv->rx, priv->rx + r, priv->rx_len - (size_t)r);
        priv->rx_len -= (size_t)r;
    }
}

/***************************************************************************
 *  A reading of the master must be what is expected
 ***************************************************************************/
PRIVATE void check_reading(hgobj gobj, json_t *kw, const char *expected_)
{
    json_t *expected = json_loads(expected_, 0, 0);
    if(!expected || !json_equal(kw, expected)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The master published something else",
            "published",    "%j", kw,
            "expected",     "%s", expected_,
            NULL
        );
    }
    JSON_DECREF(expected)
}

/***************************************************************************
 *  The counters of the master after three cycles
 ***************************************************************************/
PRIVATE void check_stats(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    struct {
        const char *name;
        json_int_t value;
    } want[] = {
        {"badFrames",   1},     // slave 2's first answer of cycle 3
        {"timeouts",    4},     // slave 2's SND_NKE in cycles 1 and 2, sent twice each
        {"repeats",     3},     // those two, and slave 2's REQ_UD2 of cycle 3
        {"failedReads", 2},     // slave 2 in cycles 1 and 2
        {"readings",    7},     // 2 + 2 + 3
    };
    for(size_t i=0; i<ARRAY_SIZE(want); i++) {
        json_int_t got = gobj_read_integer_attr(priv->gobj_master, want[i].name);
        if(got != want[i].value) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "A counter of the master is not what three cycles give",
                "counter",      "%s", want[i].name,
                "got",          "%d", (int)got,
                "want",         "%d", (int)want[i].value,
                NULL
            );
        }
    }
    if(priv->selections != LAST_CYCLE) {
        bus_error(gobj, "The secondary slave must be selected once per cycle", priv->selections);
    }
}

/***************************************************************************
 *  A reading of the master
 ***************************************************************************/
PRIVATE void master_reading(hgobj gobj, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->done) {
        return;
    }
    priv->messages++;

    if(priv->cycle < LAST_CYCLE) {
        switch(priv->messages) {
            case 1:
                check_reading(gobj, kw, slave1_reading);
                break;
            case 2:
                check_reading(gobj, kw, secondary_reading);
                break;
            default:
                bus_error(gobj, "A cycle without slave 2 published more than two readings", priv->messages);
                break;
        }
        return;
    }

    switch(priv->messages) {
        case 1:
            check_reading(gobj, kw, slave1_reading);
            break;
        case 2:
            check_reading(gobj, kw, slave2_reading);
            break;
        case 3:
            check_reading(gobj, kw, secondary_reading);
            check_stats(gobj);
            priv->done = TRUE;
            set_yuno_must_die();
            break;
        default:
            bus_error(gobj, "The last cycle published more than three readings", priv->messages);
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
            "mbus",
            C_PROT_MBUS_M,
            json_pack("{s:s, s:i, s:i, s:i, s:o}",
                "url", BUS_URL,
                "timeout_polling", 200,
                "timeout_response", 100,
                "retries", 1,
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
 *  A message: at the server, the master's frames; from the master, a
 *  reading
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        bus_rx(gobj, kw);
    } else if(src == priv->gobj_master) {
        master_reading(gobj, kw);
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
            "msg",          "%s", "The connection closed before the last cycle",
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
