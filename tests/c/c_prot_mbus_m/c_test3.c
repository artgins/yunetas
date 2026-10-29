/****************************************************************************
 *          C_TEST3.C
 *
 *          A C_PROT_MBUS_M master against a simulated M-Bus whose answers
 *          come byte by byte, as they do on a real bus: a long frame at
 *          2400 baud is ~1.2 s on the wire, more than timeout_response.
 *
 *          The server is raw (C_PROT_RAW), as a transparent M-Bus to TCP
 *          gateway would be, and this gclass answers as the one meter of the
 *          bus, at primary address 1. Its RSP_UD is 246 bytes, sent in
 *          chunks of CHUNK_SIZE bytes every CHUNK_MS: the whole frame takes
 *          ~750 ms, against a timeout_response of 100 ms.
 *          - cycle 1: the REQ_UD2 is answered with the long frame, dribbled.
 *            The master must read it: no timeout, no repeat;
 *          - cycle 2: the first answer is broken at its first byte, and the
 *            rest is dribbled all the same (noise, a collision). The master
 *            must wait until the bus is quiet before it repeats the REQ_UD2
 *            (with the same FCB), and the repeat is answered at once.
 *          - cycle 3: the first answer is noise that never goes quiet, one
 *            bad byte every NOISE_MS (less than timeout_response). The
 *            master must give up on it in a bounded time -- timeout_response
 *            plus the wire time of two frames at the baud rate, ~2.5 s here
 *            -- and repeat the REQ_UD2, which is answered at once.
 *
 *          Checked:
 *          - no request reaches the bus while an answer is on the wire;
 *          - the time the master waits out a noisy bus;
 *          - the readings, and the stats after three cycles.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <c_prot_mbus_m.h>
#include <mbus_telegram.h>
#include "c_test3.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BUS_URL             "tcp://127.0.0.1:7763"
#define RX_SIZE             512
#define LAST_CYCLE          3
#define MFR_BYTES           230     // the records fill the frame up to 246 bytes
#define CHUNK_SIZE          10
#define CHUNK_MS            30      // 25 chunks: ~750 ms, timeout_response is 100
#define TIMEOUT_RESPONSE    100
#define NOISE_MS            80      // a byte of noise before each timeout_response
#define NOISE_WAIT_MIN      2400    // the wire time of two frames (522 bytes, 11 bits, 2400 baud)
#define NOISE_WAIT_MAX      3500    // that, plus timeout_response twice, plus margin

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
    {'address': 1}                                                  \n\
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
    hgobj timer;                // C_TIMER0: the start, then the chunks of an answer

    hgobj gobj_input_side;
    hgobj gobj_master;
    hgobj server_channel;       // the channel of the server, where the master connected

    uint8_t rx[RX_SIZE];        // what the bus received and did not answer yet
    size_t rx_len;

    uint8_t tx[MBUS_FRAME_MAX_LENGTH];  // the answer being dribbled
    size_t tx_len;
    size_t tx_pos;
    BOOL dribbling;             // an answer is on the wire
    BOOL noising;               // noise is on the wire, till the next request
    uint64_t t_noise;           // when the noise started, monotonic ms

    int cycle;                  // counted at the bus, by the SND_NKE
    int requests;               // REQ_UD2 in this cycle
    int messages;               // readings published
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);

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

    if(!priv->done) {
        // the auto-kill time ended the test: a failure, not a pass
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The test stopped before its last cycle",
            "cycle",        "%d", priv->cycle,
            NULL
        );
    }
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

    set_timeout0(priv->timer, priv->timeout); // then the master connects

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout0(priv->timer);

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

/***************************************************************************
 *  The long RSP_UD of the meter: a volume, and manufacturer data up to
 *  the end of the frame
 ***************************************************************************/
PRIVATE size_t build_long_answer(uint8_t *bf, size_t size)
{
    uint8_t data[MBUS_FRAME_MAX_LENGTH];
    size_t n = 0;
    const uint8_t volume[] = {0x04, 0x13, 0x39, 0x30, 0x00, 0x00};  // 12.345 m3
    memcpy(data + n, volume, sizeof(volume));
    n += sizeof(volume);
    data[n++] = 0x0F;   // manufacturer data up to the end
    for(int i=0; i<MFR_BYTES; i++) {
        data[n++] = (uint8_t)i;
    }
    return mbus_build_long_frame(bf, size, MBUS_C_RSP_UD, 1, MBUS_CI_RESP_NO_HEADER, data, n);
}

/***************************************************************************
 *  Start to dribble an answer: CHUNK_SIZE bytes every CHUNK_MS
 ***************************************************************************/
PRIVATE void bus_dribble(hgobj gobj, BOOL broken)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->tx_len = build_long_answer(priv->tx, sizeof(priv->tx));
    if(broken) {
        priv->tx[0] = 0x69;     // not a start byte: the frame is bad from its first byte
    }
    priv->tx_pos = 0;
    priv->dribbling = TRUE;
    set_timeout0(priv->timer, CHUNK_MS);
}

/***************************************************************************
 *  The next chunk of the answer on the wire
 ***************************************************************************/
PRIVATE void bus_next_chunk(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t n = priv->tx_len - priv->tx_pos;
    if(n > CHUNK_SIZE) {
        n = CHUNK_SIZE;
    }
    bus_send(gobj, priv->tx + priv->tx_pos, n);
    priv->tx_pos += n;
    if(priv->tx_pos < priv->tx_len) {
        set_timeout0(priv->timer, CHUNK_MS);
    } else {
        priv->dribbling = FALSE;
    }
}

/***************************************************************************
 *  Noise that never goes quiet: a bad byte every NOISE_MS
 ***************************************************************************/
PRIVATE void bus_noise(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint8_t noise = 0x00;   // not a start byte
    bus_send(gobj, &noise, 1);
    set_timeout0(priv->timer, NOISE_MS);
}

/***************************************************************************
 *  The repeat ends the noise: it must come in the time bound of an answer
 ***************************************************************************/
PRIVATE void bus_noise_end(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->noising = FALSE;
    clear_timeout0(priv->timer);

    uint64_t waited = time_in_milliseconds_monotonic() - priv->t_noise;
    if(waited < NOISE_WAIT_MIN || waited > NOISE_WAIT_MAX) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The master waited out a noisy bus out of its time bound",
            "waited_ms",    "%d", (int)waited,
            "min_ms",       "%d", NOISE_WAIT_MIN,
            "max_ms",       "%d", NOISE_WAIT_MAX,
            NULL
        );
    }
}

/***************************************************************************
 *  A frame from the master, at the bus
 ***************************************************************************/
PRIVATE void bus_frame(hgobj gobj, const mbus_frame_t *frame)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->dribbling) {
        bus_error(gobj, "The master sent a request while its answer was on the wire", frame->c);
        return;
    }

    if(frame->type == MBUS_FRAME_SHORT && frame->c == MBUS_C_SND_NKE && frame->a == 1) {
        priv->cycle++;
        priv->requests = 0;
        uint8_t ack = 0xE5;
        bus_send(gobj, &ack, 1);
        return;
    }

    if(frame->type == MBUS_FRAME_SHORT && (frame->c & ~MBUS_C_FCB) == MBUS_C_REQ_UD2 && frame->a == 1) {
        priv->requests++;
        if(priv->noising) {
            bus_noise_end(gobj);
        }
        if(frame->c != (MBUS_C_REQ_UD2|MBUS_C_FCB)) {
            bus_error(gobj, "The REQ_UD2 (and its repeat) must carry FCB 1", frame->c);
        }
        if(priv->cycle == 1) {
            if(priv->requests == 1) {
                bus_dribble(gobj, FALSE);
            } else {
                bus_error(gobj, "A long answer, dribbled, must be read at the first REQ_UD2",
                    priv->requests
                );
            }
            return;
        }
        if(priv->requests == 1) {
            if(priv->cycle == 2) {
                bus_dribble(gobj, TRUE);
            } else {
                priv->noising = TRUE;
                priv->t_noise = time_in_milliseconds_monotonic();
                bus_noise(gobj);
            }
        } else if(priv->requests == 2) {
            uint8_t bf[MBUS_FRAME_MAX_LENGTH];
            size_t len = build_long_answer(bf, sizeof(bf));
            bus_send(gobj, bf, len);
        } else {
            bus_error(gobj, "A third REQ_UD2 in a cycle", priv->requests);
        }
        return;
    }

    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", "A frame the bus does not expect",
        "type",         "%d", (int)frame->type,
        "c",            "%d", frame->c,
        "a",            "%d", frame->a,
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
        bus_frame(gobj, &frame);
        memmove(priv->rx, priv->rx + r, priv->rx_len - (size_t)r);
        priv->rx_len -= (size_t)r;
    }
}

/***************************************************************************
 *  A reading of the master: the volume, and all the manufacturer data
 ***************************************************************************/
PRIVATE void check_reading(hgobj gobj, json_t *kw)
{
    json_t *jn_volume = json_object_get(json_object_get(kw, "records"), "volume");
    const char *mfr = json_string_value(json_object_get(kw, "mfr_data"));
    if(json_integer_value(json_object_get(kw, "address")) != 1 ||
            json_integer_value(json_object_get(kw, "telegrams")) != 1 ||
            json_real_value(json_object_get(jn_volume, "value")) != 12.345 ||
            !mfr || strlen(mfr) != 2*MFR_BYTES) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The master published something else",
            "published",    "%j", kw,
            NULL
        );
    }
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
        {"badFrames",   2},     // the broken answer of cycle 2 and the noise of 3, once each
        {"timeouts",    0},     // a dribbled answer is not a silence
        {"repeats",     2},     // the REQ_UD2 of cycles 2 and 3
        {"failedReads", 0},
        {"readings",    3},
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
    check_reading(gobj, kw);

    if(priv->messages != priv->cycle) {
        bus_error(gobj, "One reading per cycle", priv->messages);
    }
    if(priv->cycle == LAST_CYCLE) {
        check_stats(gobj);
        priv->done = TRUE;
        set_yuno_must_die();
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The start (the server listens: the master is created, and connects),
 *  then the chunks of an answer, or the bytes of noise
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
                "timeout_response", TIMEOUT_RESPONSE,
                "retries", 1,
                "slaves", jn_slaves
            ),
            gobj
        );
        gobj_start(priv->gobj_master);
    } else if(priv->dribbling) {
        bus_next_chunk(gobj);
    } else if(priv->noising) {
        bus_noise(gobj);
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
PUBLIC int register_c_test3(void)
{
    return create_gclass(C_TEST3);
}
