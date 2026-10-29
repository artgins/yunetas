/****************************************************************************
 *          C_TEST2.C
 *
 *          The operations of a C_PROT_MBUS_M master on a simulated M-Bus:
 *          scan-primary, scan-secondary and set-primary-address.
 *
 *          The bus has six meters (identification, primary address):
 *              M1  11111111  1
 *              M2  33333333  3
 *              M3  41111111  4     M3 and M4 share address 4: their E5
 *              M4  42222222  4     collide
 *              M5  12345678  0     M5 and M6 have no primary address
 *              M6  12349999  0
 *          A selection matches the meters whose id has every digit the mask
 *          fixes; one answers E5, several answer garbage (a collision).
 *          The meters are strict about the FCB: a SND_UD with the FCB of
 *          the last one is a repeat, acknowledged and not done, and a
 *          selection clears that memory -- so the write of
 *          set-primary-address takes only with FCB 1.
 *
 *          The master reads slave 1. On its reading, scan-primary 1-5 is
 *          asked: the cycle is in progress, so it waits for the end of the
 *          cycle. Then scan-secondary, then set-primary-address of M6 to 9.
 *          Each must find exactly what the bus has. A second operation
 *          while one runs, and an address with a wildcard, are refused.
 *          Last, read-now starts a cycle at once (timeout_polling is a
 *          minute): slave 1 is read again.
 *
 *          The driver asks operation-status on a timer: a test that waits
 *          for a result, not a polling design.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <c_prot_mbus_m.h>
#include <mbus_telegram.h>
#include "c_test2.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define BUS_URL             "tcp://127.0.0.1:7762"
#define RX_SIZE             512
#define N_METERS            6
#define STATUS_CHECK_MS     100

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    const char *id;         // 8 digits
    int address;            // primary, 0 none
    int last_fcb;           // FCB of the last SND_UD, cleared by a selection
} meter_t;

typedef enum {
    PHASE_READ = 0,         // waiting for the reading of slave 1
    PHASE_SCAN_PRIMARY,
    PHASE_SCAN_SECONDARY,
    PHASE_SET_ADDRESS,
    PHASE_READ_NOW,
    PHASE_DONE,
} phase_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE const char *found_primary = "["
    "{\"address\": 1, \"secondary\": \"11111111\", \"manufacturer\": \"KAM\", \"version\": 1,"
    " \"medium\": \"water\", \"medium_code\": 7},"
    "{\"address\": 3, \"secondary\": \"33333333\", \"manufacturer\": \"KAM\", \"version\": 1,"
    " \"medium\": \"water\", \"medium_code\": 7},"
    "{\"address\": 4, \"collision\": true}"
"]";

PRIVATE const char *found_secondary = "["
    "{\"secondary\": \"11111111\", \"manufacturer\": \"KAM\", \"version\": 1, \"medium\": \"water\", \"medium_code\": 7, \"address\": 1},"
    "{\"secondary\": \"12345678\", \"manufacturer\": \"KAM\", \"version\": 1, \"medium\": \"water\", \"medium_code\": 7, \"address\": 0},"
    "{\"secondary\": \"12349999\", \"manufacturer\": \"KAM\", \"version\": 1, \"medium\": \"water\", \"medium_code\": 7, \"address\": 0},"
    "{\"secondary\": \"33333333\", \"manufacturer\": \"KAM\", \"version\": 1, \"medium\": \"water\", \"medium_code\": 7, \"address\": 3},"
    "{\"secondary\": \"41111111\", \"manufacturer\": \"KAM\", \"version\": 1, \"medium\": \"water\", \"medium_code\": 7, \"address\": 4},"
    "{\"secondary\": \"42222222\", \"manufacturer\": \"KAM\", \"version\": 1, \"medium\": \"water\", \"medium_code\": 7, \"address\": 4}"
"]";

PRIVATE const char *found_set_address = "[{\"secondary\": \"12349999\", \"address\": 9}]";

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
    hgobj timer;                // the start
    hgobj timer_status;         // C_TIMER0: operation-status every STATUS_CHECK_MS

    hgobj gobj_input_side;
    hgobj gobj_master;
    hgobj server_channel;

    uint8_t rx[RX_SIZE];
    size_t rx_len;

    meter_t meters[N_METERS];
    int selected;               // the meter selected, -1 none
    phase_t phase;
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
    priv->timer_status = gobj_create_pure_child("status", C_TIMER0, 0, gobj);

    const meter_t meters[N_METERS] = {
        {"11111111", 1, 0},
        {"33333333", 3, 0},
        {"41111111", 4, 0},
        {"42222222", 4, 0},
        {"12345678", 0, 0},
        {"12349999", 0, 0},
    };
    memcpy(priv->meters, meters, sizeof(meters));
    priv->selected = -1;

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
    gobj_start(priv->timer_status);

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
    gobj_stop(priv->timer_status);

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
    clear_timeout0(priv->timer_status);

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
PRIVATE void test_error(hgobj gobj, const char *msg, const char *detail)
{
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INTERNAL,
        "msg",          "%s", msg,
        "detail",       "%s", detail? detail : "",
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

/*
 *  The answer of n meters at once: one E5, or garbage
 */
PRIVATE void bus_acks(hgobj gobj, int n)
{
    uint8_t acks[2] = {0xE5, 0xE5};
    if(n == 1) {
        bus_send(gobj, acks, 1);
    } else if(n > 1) {
        bus_send(gobj, acks, 2);
    }
}

/*
 *  The RSP_UD of a meter: its header (KAM, version 1, water) and a volume
 */
PRIVATE void bus_rsp_ud(hgobj gobj, const meter_t *m)
{
    uint8_t data[18];
    for(int i=0; i<4; i++) {
        int hi = m->id[6 - 2*i] - '0';
        int lo = m->id[7 - 2*i] - '0';
        data[i] = (uint8_t)((hi << 4) | lo);
    }
    const uint8_t rest[14] = {0x2D, 0x2C, 0x01, 0x07, 0x01, 0x00, 0x00, 0x00,
        0x04, 0x13, 0xE8, 0x03, 0x00, 0x00};
    memcpy(data + 4, rest, sizeof(rest));

    uint8_t bf[MBUS_FRAME_MAX_LENGTH];
    size_t len = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_RSP_UD, (uint8_t)m->address,
        MBUS_CI_RESP_VARIABLE, data, sizeof(data)
    );
    bus_send(gobj, bf, len);
}

/*
 *  Does a selection (8 bytes) match a meter?
 */
PRIVATE BOOL selection_matches(const uint8_t *sel, const meter_t *m)
{
    for(int i=0; i<4; i++) {
        int digits[2] = {sel[3 - i] >> 4, sel[3 - i] & 0x0F};
        for(int k=0; k<2; k++) {
            int d = digits[k];
            if(d == 0x0F) {
                continue;
            }
            if(d != m->id[2*i + k] - '0') {
                return FALSE;
            }
        }
    }
    BOOL man_any = sel[4] == 0xFF && sel[5] == 0xFF;
    BOOL man_kam = sel[4] == 0x2D && sel[5] == 0x2C;
    if(!man_any && !man_kam) {
        return FALSE;
    }
    if(sel[6] != 0xFF && sel[6] != 1) {
        return FALSE;
    }
    if(sel[7] != 0xFF && sel[7] != 7) {
        return FALSE;
    }
    return TRUE;
}

/***************************************************************************
 *  A frame from the master, at the bus
 ***************************************************************************/
PRIVATE void bus_frame(hgobj gobj, const mbus_frame_t *frame)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(frame->type == MBUS_FRAME_SHORT && frame->c == MBUS_C_SND_NKE) {
        int n = 0;
        for(int i=0; i<N_METERS; i++) {
            if(priv->meters[i].address == frame->a) {
                n++;
            }
        }
        bus_acks(gobj, n);
        return;
    }

    if(frame->type == MBUS_FRAME_LONG && (frame->c & ~MBUS_C_FCB) == MBUS_C_SND_UD &&
            frame->a == MBUS_ADDRESS_SELECTED && frame->ci == MBUS_CI_SELECT_SECONDARY &&
            frame->data_len == MBUS_SECONDARY_ADDRESS_SIZE) {
        int n = 0;
        priv->selected = -1;
        for(int i=0; i<N_METERS; i++) {
            if(selection_matches(frame->data, &priv->meters[i])) {
                n++;
                priv->selected = i;
            }
        }
        if(n != 1) {
            priv->selected = -1;
        } else {
            priv->meters[priv->selected].last_fcb = 0;  // a selection clears the FCB memory
        }
        bus_acks(gobj, n);
        return;
    }

    if(frame->type == MBUS_FRAME_LONG && (frame->c & ~MBUS_C_FCB) == MBUS_C_SND_UD &&
            frame->a == MBUS_ADDRESS_SELECTED && frame->ci == MBUS_CI_DATA_SEND) {
        if(priv->selected < 0 || frame->data_len != 3 || frame->data[0] != 0x01 || frame->data[1] != 0x7A) {
            test_error(gobj, "A write the bus does not expect", 0);
            return;
        }
        /*
         *  A strict meter (m-bus.com 5 and 7.2): a SND_UD with the FCB of
         *  the last one is a repeat, acknowledged and not done again. So
         *  the first one after the selection must carry FCB 1
         */
        meter_t *m = &priv->meters[priv->selected];
        int fcb = (frame->c & MBUS_C_FCB)? 1 : 0;
        if(fcb != m->last_fcb) {
            m->last_fcb = fcb;
            m->address = frame->data[2];
        }
        bus_acks(gobj, 1);
        return;
    }

    if(frame->type == MBUS_FRAME_SHORT && (frame->c & ~MBUS_C_FCB) == MBUS_C_REQ_UD2) {
        if(frame->a == MBUS_ADDRESS_SELECTED) {
            if(priv->selected >= 0) {
                bus_rsp_ud(gobj, &priv->meters[priv->selected]);
            }
            return;
        }
        int n = 0;
        int found = -1;
        for(int i=0; i<N_METERS; i++) {
            if(priv->meters[i].address == frame->a) {
                n++;
                found = i;
            }
        }
        if(n == 1) {
            bus_rsp_ud(gobj, &priv->meters[found]);
        } else if(n > 1) {
            bus_acks(gobj, 2);
        }
        return;
    }

    test_error(gobj, "A frame the bus does not expect", 0);
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
        test_error(gobj, "The bus got more than it can hold", 0);
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
            test_error(gobj, "The master sent a bad frame", why);
            priv->rx_len = 0;
            return;
        }
        bus_frame(gobj, &frame);
        memmove(priv->rx, priv->rx + r, priv->rx_len - (size_t)r);
        priv->rx_len -= (size_t)r;
    }
}

/***************************************************************************
 *  A command to the master, that must answer `expected_result`
 ***************************************************************************/
PRIVATE void master_command(hgobj gobj, const char *command, json_t *kw, int expected_result)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *resp = gobj_command(priv->gobj_master, command, kw, gobj);
    int result = (int)kw_get_int(gobj, resp, "result", -999, 0);
    if(result != expected_result) {
        char detail[512];
        snprintf(detail, sizeof(detail), "%s answered %d, not %d: %s",
            command, result, expected_result, kw_get_str(gobj, resp, "comment", "", 0)
        );
        test_error(gobj, "A command of the master answered something else", detail);
    }
    JSON_DECREF(resp)
}

/***************************************************************************
 *  The result of the operation of the phase must be what the bus has
 ***************************************************************************/
PRIVATE void check_found(hgobj gobj, json_t *jn_op, const char *expected_)
{
    json_t *expected = json_loads(expected_, 0, 0);
    json_t *found = json_object_get(jn_op, "found");
    if(!expected || !json_equal(found, expected) ||
            strcmp(kw_get_str(gobj, jn_op, "state", "", 0), "done") != 0) {
        char *got = json_dumps(jn_op, JSON_COMPACT);
        test_error(gobj, "The operation found something else", got);
        gbmem_free(got);
    }
    JSON_DECREF(expected)
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The start: the master is created, and connects. Then, every
 *  STATUS_CHECK_MS, the state of the operation of the phase.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->timer) {
        if(!priv->gobj_master) {
            priv->gobj_master = gobj_create(
                "mbus",
                C_PROT_MBUS_M,
                json_pack("{s:s, s:i, s:i, s:i, s:[{s:i}]}",
                    "url", BUS_URL,
                    "timeout_polling", 60000,
                    "timeout_response", 30,
                    "retries", 0,
                    "slaves",
                        "address", 1
                ),
                gobj
            );
            gobj_start(priv->gobj_master);
        }
        JSON_DECREF(kw)
        return 0;
    }

    json_t *resp = gobj_command(priv->gobj_master, "operation-status", json_object(), gobj);
    json_t *jn_op = kw_get_dict(gobj, resp, "data", 0, 0);
    const char *state = kw_get_str(gobj, jn_op, "state", "", 0);
    if(strcmp(state, "done") != 0 && strcmp(state, "failed") != 0) {
        set_timeout0(priv->timer_status, STATUS_CHECK_MS);
        JSON_DECREF(resp)
        JSON_DECREF(kw)
        return 0;
    }

    switch(priv->phase) {
        case PHASE_SCAN_PRIMARY:
            check_found(gobj, jn_op, found_primary);
            priv->phase = PHASE_SCAN_SECONDARY;
            master_command(gobj, "scan-secondary", json_object(), 0);
            set_timeout0(priv->timer_status, STATUS_CHECK_MS);
            break;

        case PHASE_SCAN_SECONDARY:
            check_found(gobj, jn_op, found_secondary);
            priv->phase = PHASE_SET_ADDRESS;
            master_command(gobj, "set-primary-address",
                json_pack("{s:s, s:s, s:i}", "secondary", "1234FFFF", "manufacturer", "KAM", "new_address", 9),
                -1      // a wildcard: refused
            );
            master_command(gobj, "set-primary-address",
                json_pack("{s:s, s:s, s:i}", "secondary", "12349999", "manufacturer", "KAM", "new_address", 9),
                0
            );
            set_timeout0(priv->timer_status, STATUS_CHECK_MS);
            break;

        case PHASE_SET_ADDRESS:
            check_found(gobj, jn_op, found_set_address);
            if(priv->meters[5].address != 9) {
                test_error(gobj, "The meter did not get its new address", 0);
            }
            priv->phase = PHASE_READ_NOW;
            master_command(gobj, "read-now", json_object(), 0);
            master_command(gobj, "read-now", json_object(), -1);    // the cycle is in progress
            break;

        default:
            test_error(gobj, "A status check in a phase without operation", 0);
            break;
    }

    JSON_DECREF(resp)
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
    } else if(src == priv->gobj_master && priv->phase == PHASE_READ_NOW) {
        priv->phase = PHASE_DONE;
        set_yuno_must_die();
    } else if(src == priv->gobj_master && priv->phase == PHASE_READ) {
        /*
         *  The cycle is in progress: the scan waits for its end
         */
        priv->phase = PHASE_SCAN_PRIMARY;
        master_command(gobj, "scan-primary", json_pack("{s:i, s:i}", "from", 1, "to", 5), 0);
        master_command(gobj, "scan-secondary", json_object(), -1);     // one at a time
        set_timeout0(priv->timer_status, STATUS_CHECK_MS);
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
 *  A close before the end is a failure
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->phase != PHASE_DONE && !gobj_is_shutdowning()) {
        test_error(gobj, "The connection closed before the end", gobj_short_name(src));
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A child stopped (C_TIMER0 says it when a timeout is cleared)
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
GOBJ_DEFINE_GCLASS(C_TEST2);

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
PUBLIC int register_c_test2(void)
{
    return create_gclass(C_TEST2);
}
