/***********************************************************************
 *          C_PROT_MBUS_M.C
 *          Prot_mbus_m GClass.
 *
 *          M-Bus master (EN 13757-2/-3): reads the meters of a wired bus.
 *
 *          The bottom is a C_UART on a serial M-Bus master (a level
 *          converter: 2400 baud, 8E1 by default), or a C_TCP to a
 *          transparent M-Bus to TCP gateway when the url is tcp://host:port.
 *          The parent may give the bottom itself.
 *
 *          Configuration:
 *
 *              "slaves": [
 *                  {"address": 1},
 *                  {"address": 2, "disabled": true},
 *                  {"secondary": "12345678"},
 *                  {"secondary": "12345678", "manufacturer": "KAM",
 *                   "version": 8, "medium": "water"}
 *              ]
 *
 *          A slave is read by its primary address (1-250), or by its
 *          secondary address: the 8 digits of its identification, and,
 *          to be stricter, its manufacturer, version and medium. A slave
 *          that cannot be read as written is left out, with an error.
 *
 *          THE CYCLE. The slaves are read one after the other:
 *
 *              primary:    SND_NKE -> E5, REQ_UD2 -> RSP_UD [, REQ_UD2 ...]
 *              secondary:  SND_UD select (CI 52) -> E5,
 *                          REQ_UD2 to FD -> RSP_UD [, REQ_UD2 ...]
 *
 *          The SND_NKE resets the slave's link, so its readout starts at
 *          the first telegram. The first REQ_UD2 carries FCB 1; when a
 *          telegram ends with DIF 1F (more records follow) the FCB is
 *          toggled and the next one asked for, up to max_telegrams. A
 *          request without a valid answer is repeated, with the same FCB,
 *          up to `retries` times; a frame that does not parse is waited
 *          out until timeout_response, so the rest of a collision or of
 *          noise does not fall into the next request. After the last
 *          slave the master waits timeout_polling and starts again.
 *
 *          WHAT IS PUBLISHED. One EV_ON_MESSAGE per slave read, as soon as
 *          its reading is complete (a cycle of 250 meters is minutes long:
 *          a reading carries the time it was read, not the end of the
 *          cycle):
 *
 *              {
 *                  "address": 1,                   // or "secondary": "12345678"
 *                  "header": {...},                // see mbus_telegram.h
 *                  "records": {"volume": {"value": 12.345, "unit": "m3"}, ...},
 *                  "mfr_data": "...",              // when the meter sends it
 *                  "telegrams": 2
 *              }
 *
 *          Only what was read: a slave that was not read in a cycle
 *          publishes nothing in it, never its earlier reading again.
 *
 *          FAILURES ARE LOGGED ON TRANSITION. The first time a slave is not
 *          read, a WARNING "M-Bus slave not read" with the reason (no
 *          answer, a bad frame -- with its bytes --, an answer that cannot
 *          be decoded, an application error). The same reason again logs
 *          nothing; another reason logs again. When the slave is read again,
 *          an INFO "M-Bus slave answered again" with what it had failed
 *          with. Every frame is counted in the stats.
 *
 *          OPERATIONS. Three commands work on the bus itself, for putting
 *          meters into service: scan-primary (SND_NKE to each address, and
 *          REQ_UD2 to the ones that answer, to say who they are),
 *          scan-secondary (the wildcard tree search: a selection with F in
 *          the digits not fixed yet; nothing answers, one answers E5, or
 *          several answer at once and the garbage is a collision that
 *          fixes one digit more), and set-primary-address (select a meter
 *          by its secondary address, write it 01 7A <address>, and check
 *          it answers SND_NKE there). A command starts its operation and
 *          answers at once; the operation runs when the bus is free (at
 *          the end of the cycle in progress), the cycle waits while it
 *          runs, and its state and result are asked with operation-status.
 *          The end of an operation is logged, as an INFO when it is done
 *          and a WARNING when it failed.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <strings.h>

#include "msg_ievent.h"
#include "g_ev_kernel.h"
#include "g_st_kernel.h"
#include "c_timer0.h"
#include "c_tcp.h"
#include "c_uart.h"
#include "mbus_telegram.h"
#include "c_prot_mbus_m.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define RX_BUFFER_SIZE      (2*MBUS_FRAME_MAX_LENGTH)
#define DUMP_MAX_BYTES      64      // the bytes of a bad frame shown in its warning
#define FAILURE_SIZE        64

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef enum {
    STEP_LINK_RESET = 1,    // SND_NKE to the primary address
    STEP_SELECT,            // SND_UD CI 52: select by secondary address
    STEP_REQUEST,           // REQ_UD2
} step_t;

typedef enum {
    OP_NONE = 0,
    OP_SCAN_PRIMARY,
    OP_SCAN_SECONDARY,
    OP_SET_ADDRESS,
} op_t;

typedef enum {
    OPS_PROBE = 1,          // SND_NKE to an address, or a selection
    OPS_IDENTIFY,           // REQ_UD2: who answered
    OPS_WRITE,              // SND_UD CI 51: 01 7A <new address>
    OPS_VERIFY,             // SND_NKE to the new address
} op_step_t;

typedef struct {
    int address;                            // 1-250; 0 when read by secondary address
    char secondary[16];                     // the 8 digits, "" when primary
    uint8_t selection[MBUS_SECONDARY_ADDRESS_SIZE];
    BOOL disabled;
    char failing[FAILURE_SIZE];             // what the last read failed with, "" when read
    json_t *jn_last;                        // the last reading published
} slave_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int load_slaves(hgobj gobj);
PRIVATE void free_slaves(hgobj gobj);
PRIVATE void start_cycle(hgobj gobj);
PRIVATE void next_slave(hgobj gobj);
PRIVATE void send_step(hgobj gobj);
PRIVATE void start_operation(hgobj gobj);
PRIVATE void op_send(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
/*---------------------------------------------*
 *      Commands
 *---------------------------------------------*/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_dump_data(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_scan_primary(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_scan_secondary(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_set_primary_address(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_operation_status(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_read_now(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_dump_data[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER,   "address",      0,              0,          "Primary address of the slave (all when neither is given)"),
SDATAPM (DTP_STRING,    "secondary",    0,              0,          "Secondary address of the slave"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_scan_primary[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER,   "from",         0,              "1",        "First primary address to try"),
SDATAPM (DTP_INTEGER,   "to",           0,              "250",      "Last primary address to try"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_set_primary_address[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "secondary",    0,              0,          "Secondary address (8 digits) of the meter"),
SDATAPM (DTP_STRING,    "manufacturer", 0,              0,          "Its manufacturer (3 letters), to be stricter"),
SDATAPM (DTP_INTEGER,   "new_address",  0,              0,          "The primary address to give it, 1-250"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,     pm_help,        cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "dump-data",        0,          pm_dump_data,   cmd_dump_data,  "Last reading of the slaves"),
SDATACM (DTP_SCHEMA,    "scan-primary",     0,          pm_scan_primary, cmd_scan_primary, "Start a search of the bus by primary address (see operation-status)"),
SDATACM (DTP_SCHEMA,    "scan-secondary",   0,          0,              cmd_scan_secondary, "Start a search of the bus by secondary address (see operation-status)"),
SDATACM (DTP_SCHEMA,    "set-primary-address", 0,       pm_set_primary_address, cmd_set_primary_address, "Give a meter, selected by its secondary address, a primary address (see operation-status)"),
SDATACM (DTP_SCHEMA,    "operation-status", 0,          0,              cmd_operation_status, "State and result of the last operation"),
SDATACM (DTP_SCHEMA,    "read-now",         0,          0,              cmd_read_now,   "Start a cycle now, without waiting for timeout_polling"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description---------- */
SDATA (DTP_STRING,      "url",              SDF_RD,             "",         "Serial device of the M-Bus master (/dev/ttyUSB0), or tcp://host:port of a transparent M-Bus gateway"),
SDATA (DTP_INTEGER,     "baudrate",         SDF_RD,             "2400",     "Serial: baud rate"),
SDATA (DTP_INTEGER,     "bytesize",         SDF_RD,             "8",        "Serial: byte size"),
SDATA (DTP_STRING,      "parity",           SDF_RD,             "even",     "Serial: parity (none, odd, even)"),
SDATA (DTP_INTEGER,     "stopbits",         SDF_RD,             "1",        "Serial: stop bits"),
SDATA (DTP_LIST,        "slaves",           SDF_WR,             "[]",       "Slaves to read: [{\"address\": 1}, {\"secondary\": \"12345678\"}, ...]. Read at start"),
SDATA (DTP_INTEGER,     "timeout_polling",  SDF_WR,             "60000",    "Milliseconds from the end of a cycle to the next one"),
SDATA (DTP_INTEGER,     "timeout_response", SDF_WR,             "500",      "Milliseconds to wait for an answer"),
SDATA (DTP_INTEGER,     "retries",          SDF_WR,             "2",        "Times a request without a valid answer is repeated"),
SDATA (DTP_INTEGER,     "max_telegrams",    SDF_WR,             "8",        "Most telegrams of one reading (DIF 1F, more records follow)"),
SDATA (DTP_INTEGER,     "txFrames",         SDF_RD|SDF_RSTATS,  "0",        "Frames sent"),
SDATA (DTP_INTEGER,     "rxFrames",         SDF_RD|SDF_RSTATS,  "0",        "Valid frames received"),
SDATA (DTP_INTEGER,     "badFrames",        SDF_RD|SDF_RSTATS,  "0",        "Frames received that do not parse, or are not the answer asked for"),
SDATA (DTP_INTEGER,     "timeouts",         SDF_RD|SDF_RSTATS,  "0",        "Requests with no answer in timeout_response"),
SDATA (DTP_INTEGER,     "repeats",          SDF_RD|SDF_RSTATS,  "0",        "Requests repeated"),
SDATA (DTP_INTEGER,     "readings",         SDF_RD|SDF_RSTATS,  "0",        "Readings published"),
SDATA (DTP_INTEGER,     "failedReads",      SDF_RD|SDF_RSTATS,  "0",        "Reads of a slave that failed"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "Subscriber of output-events. If null then subscriber is the parent"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES  = 0x0001,
    TRACE_TRAFFIC   = 0x0002,
    TRACE_POLLING   = 0x0004,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace the readings published"},
{"traffic",         "Trace dump traffic"},
{"polling",         "Trace the steps of the cycle and of the operations"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    json_int_t timeout_polling;
    json_int_t timeout_response;
    json_int_t retries;
    json_int_t max_telegrams;

    slave_t *slaves;
    int n_slaves;
    int idx;                        // the slave being read, -1 before the first
    step_t step;
    uint8_t c_request;              // C of the REQ_UD2: the FCB moves
    int attempts;                   // repeats of the request in flight
    int telegrams;                  // of the reading in progress
    json_t *jn_reading;             // the reading in progress

    uint8_t rx[RX_BUFFER_SIZE];
    size_t rx_len;
    BOOL rx_bad;                    // a bad frame arrived: wait out the timeout
    char rx_why[256];
    char rx_dump[2*DUMP_MAX_BYTES + 1];
    BOOL unexpected_warned;         // bytes with no request in flight, warned once

    BOOL inform_on_close;

    op_t op;                        // the operation running, OP_NONE when the cycle runs
    op_t op_asked;                  // the operation asked for, till it starts
    BOOL op_pending;                // asked for: it starts when the bus is free
    op_step_t op_step;
    int op_address;                 // scan-primary: the address probed
    int op_to;                      // scan-primary: the last one
    char op_mask[9];                // scan-secondary: the id probed, F for any digit
    int op_pos;                     // scan-secondary: the digit being enumerated
    char op_secondary[16];          // set-primary-address
    int op_new_address;
    uint8_t op_selection[MBUS_SECONDARY_ADDRESS_SIZE];
    json_t *jn_op;                  // state and result of the last operation

    json_int_t txFrames;
    json_int_t rxFrames;
    json_int_t badFrames;
    json_int_t timeouts;
    json_int_t repeats;
    json_int_t readings;
    json_int_t failedReads;
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

    /*
     *  C_TIMER0, not C_TIMER: an answer is waited for in hundreds of
     *  milliseconds, and C_TIMER counts in seconds
     */
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER0, 0, gobj);

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout_polling,       gobj_read_integer_attr)
    SET_PRIV(timeout_response,      gobj_read_integer_attr)
    SET_PRIV(retries,               gobj_read_integer_attr)
    SET_PRIV(max_telegrams,         gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout_polling,         gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(timeout_response,      gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(retries,               gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(max_telegrams,         gobj_read_integer_attr)
    END_EQ_SET_PRIV()

    /*
     *  `stats=__reset__` writes the defaults of the SDF_RSTATS attrs: the
     *  counters live in priv (mt_reading), so they are zeroed here
     */
    if(!path) {
        return;
    }
    if(strcmp(path, "txFrames")==0) {
        priv->txFrames = 0;
    } else if(strcmp(path, "rxFrames")==0) {
        priv->rxFrames = 0;
    } else if(strcmp(path, "badFrames")==0) {
        priv->badFrames = 0;
    } else if(strcmp(path, "timeouts")==0) {
        priv->timeouts = 0;
    } else if(strcmp(path, "repeats")==0) {
        priv->repeats = 0;
    } else if(strcmp(path, "readings")==0) {
        priv->readings = 0;
    } else if(strcmp(path, "failedReads")==0) {
        priv->failedReads = 0;
    }
}

/***************************************************************************
 *      Framework Method reading
 ***************************************************************************/
PRIVATE SData_Value_t mt_reading(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    SData_Value_t v = {0,{0}};
    if(strcmp(name, "txFrames")==0) {
        v.found = 1;
        v.v.i = priv->txFrames;
    } else if(strcmp(name, "rxFrames")==0) {
        v.found = 1;
        v.v.i = priv->rxFrames;
    } else if(strcmp(name, "badFrames")==0) {
        v.found = 1;
        v.v.i = priv->badFrames;
    } else if(strcmp(name, "timeouts")==0) {
        v.found = 1;
        v.v.i = priv->timeouts;
    } else if(strcmp(name, "repeats")==0) {
        v.found = 1;
        v.v.i = priv->repeats;
    } else if(strcmp(name, "readings")==0) {
        v.found = 1;
        v.v.i = priv->readings;
    } else if(strcmp(name, "failedReads")==0) {
        v.found = 1;
        v.v.i = priv->failedReads;
    }

    return v;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    free_slaves(gobj);
    JSON_DECREF(priv->jn_op)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    free_slaves(gobj);
    load_slaves(gobj);

    hgobj bottom_gobj = gobj_bottom_gobj(gobj);
    if(!bottom_gobj) {
        const char *url = gobj_read_str_attr(gobj, "url");
        if(empty_string(url)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "M-Bus master without url and without a bottom",
                NULL
            );
            return -1;
        }
        if(strncasecmp(url, "tcp://", 6) == 0) {
            bottom_gobj = gobj_create_pure_child(
                gobj_name(gobj),
                C_TCP,
                json_pack("{s:s}", "url", url),
                gobj
            );
        } else {
            bottom_gobj = gobj_create_pure_child(
                gobj_name(gobj),
                C_UART,
                json_pack("{s:s, s:I, s:I, s:s, s:I}",
                    "url", url,
                    "baudrate", (json_int_t)gobj_read_integer_attr(gobj, "baudrate"),
                    "bytesize", (json_int_t)gobj_read_integer_attr(gobj, "bytesize"),
                    "parity", gobj_read_str_attr(gobj, "parity"),
                    "stopbits", (json_int_t)gobj_read_integer_attr(gobj, "stopbits")
                ),
                gobj
            );
        }
        if(!bottom_gobj) {
            // Error already logged
            return -1;
        }
        gobj_set_bottom_gobj(gobj, bottom_gobj);
    }

    gobj_start(priv->timer);
    gobj_start(bottom_gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop(priv->timer);
    JSON_DECREF(priv->jn_reading)

    hgobj bottom_gobj = gobj_bottom_gobj(gobj);
    if(bottom_gobj && gobj_is_running(bottom_gobj)) {
        gobj_stop(bottom_gobj);
    }

    return 0;
}




                    /***************************
                     *      Commands
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw)
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(
        gobj,
        0,
        jn_resp,
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *  The last reading of one slave, or of every slave
 ***************************************************************************/
PRIVATE json_t *cmd_dump_data(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int address = (int)kw_get_int(gobj, kw, "address", 0, 0);
    const char *secondary = kw_get_str(gobj, kw, "secondary", "", 0);

    json_t *jn_data = json_array();
    for(int i=0; i<priv->n_slaves; i++) {
        slave_t *s = &priv->slaves[i];
        if(address && s->address != address) {
            continue;
        }
        if(!empty_string(secondary) && strcmp(s->secondary, secondary) != 0) {
            continue;
        }
        json_t *jn_slave = json_object();
        if(s->address) {
            json_object_set_new(jn_slave, "address", json_integer(s->address));
        } else {
            json_object_set_new(jn_slave, "secondary", json_string(s->secondary));
        }
        json_object_set_new(jn_slave, "disabled", json_boolean(s->disabled));
        json_object_set_new(jn_slave, "failing", json_string(s->failing));
        json_object_set_new(jn_slave, "last_reading", s->jn_last? json_incref(s->jn_last) : json_null());
        json_array_append_new(jn_data, jn_slave);
    }

    if((address || !empty_string(secondary)) && json_array_size(jn_data) == 0) {
        JSON_DECREF(jn_data)
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: M-Bus slave not found", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_data,
        kw  // owned
    );
}

/***************************************************************************
 *  A cycle now: the bus must be open and between cycles
 ***************************************************************************/
PRIVATE json_t *cmd_read_now(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_current_state(gobj) != ST_CONNECTED || priv->op || priv->op_pending) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: M-Bus not between cycles (not connected, a cycle or an operation in progress)",
                gobj_yuno_role_plus_name()
            ),
            0,
            0,
            kw  // owned
        );
    }
    if(priv->n_slaves == 0) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: M-Bus master with no slaves", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    start_cycle(gobj);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: M-Bus cycle started", gobj_yuno_role_plus_name()),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *  An operation can be asked for with the bus open and no other operation
 *  in progress. NULL when it can, else the refusal to answer.
 ***************************************************************************/
PRIVATE json_t *operation_refused(hgobj gobj, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_current_state(gobj) == ST_DISCONNECTED) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: M-Bus not connected", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }
    if(priv->op || priv->op_pending) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: M-Bus operation already in progress: %s",
                gobj_yuno_role_plus_name(),
                kw_get_str(gobj, priv->jn_op, "operation", "", 0)
            ),
            0,
            0,
            kw  // owned
        );
    }
    return NULL;
}

/***************************************************************************
 *  Note the operation asked for, and start it when the bus is free
 ***************************************************************************/
PRIVATE json_t *operation_asked(hgobj gobj, op_t op, const char *name, json_t *jn_args, json_t *kw)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->jn_op)
    priv->jn_op = json_pack("{s:s, s:s, s:o, s:I, s:n, s:[], s:s}",
        "operation", name,
        "state", "waiting",
        "args", jn_args,
        "asked", (json_int_t)time_in_seconds(),
        "ended",
        "found",
        "error", ""
    );
    priv->op_asked = op;
    priv->op_pending = TRUE;

    if(gobj_current_state(gobj) == ST_CONNECTED) {
        start_operation(gobj);  // the bus is between cycles
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%s: M-Bus %s %s: see operation-status",
            gobj_yuno_role_plus_name(), name, priv->op_pending? "waits for the cycle to end" : "started"
        ),
        0,
        json_incref(priv->jn_op),
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_scan_primary(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_refused = operation_refused(gobj, kw);
    if(jn_refused) {
        return jn_refused;
    }

    int from = (int)kw_get_int(gobj, kw, "from", MBUS_ADDRESS_MIN, 0);
    int to = (int)kw_get_int(gobj, kw, "to", MBUS_ADDRESS_MAX, 0);
    if(from < MBUS_ADDRESS_MIN || to > MBUS_ADDRESS_MAX || from > to) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: M-Bus primary addresses are 1-250, from <= to", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    priv->op_address = from;
    priv->op_to = to;
    return operation_asked(gobj, OP_SCAN_PRIMARY, "scan-primary",
        json_pack("{s:i, s:i}", "from", from, "to", to), kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_scan_secondary(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_refused = operation_refused(gobj, kw);
    if(jn_refused) {
        return jn_refused;
    }

    snprintf(priv->op_mask, sizeof(priv->op_mask), "FFFFFFFF");
    priv->op_pos = 0;
    return operation_asked(gobj, OP_SCAN_SECONDARY, "scan-secondary", json_object(), kw);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_set_primary_address(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_refused = operation_refused(gobj, kw);
    if(jn_refused) {
        return jn_refused;
    }

    const char *secondary = kw_get_str(gobj, kw, "secondary", "", 0);
    const char *manufacturer = kw_get_str(gobj, kw, "manufacturer", "", 0);
    int new_address = (int)kw_get_int(gobj, kw, "new_address", 0, 0);

    char why[256] = "";
    if(mbus_build_secondary_address(priv->op_selection, secondary, manufacturer, -1, -1,
            why, sizeof(why)) < 0 || strchr(secondary, 'F') || strchr(secondary, 'f')) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: M-Bus secondary address must be 8 digits, no wildcard: %s",
                gobj_yuno_role_plus_name(), why
            ),
            0,
            0,
            kw  // owned
        );
    }
    if(new_address < MBUS_ADDRESS_MIN || new_address > MBUS_ADDRESS_MAX) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("%s: M-Bus new_address must be 1-250", gobj_yuno_role_plus_name()),
            0,
            0,
            kw  // owned
        );
    }

    snprintf(priv->op_secondary, sizeof(priv->op_secondary), "%s", secondary);
    priv->op_new_address = new_address;
    return operation_asked(gobj, OP_SET_ADDRESS, "set-primary-address",
        json_pack("{s:s, s:s, s:i}",
            "secondary", secondary,
            "manufacturer", manufacturer,
            "new_address", new_address
        ),
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_operation_status(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        priv->jn_op? json_incref(priv->jn_op) : json_null(),
        kw  // owned
    );
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  The slaves of the configuration. One that cannot be read as written is
 *  left out, with its error.
 ***************************************************************************/
PRIVATE int load_slaves(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_slaves = gobj_read_json_attr(gobj, "slaves");
    size_t n = json_array_size(jn_slaves);
    if(n == 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "M-Bus master with no slaves",
            NULL
        );
        return 0;
    }

    priv->slaves = gbmem_calloc(n, sizeof(slave_t));
    if(!priv->slaves) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "no memory for the slaves",
            "n",            "%d", (int)n,
            NULL
        );
        return -1;
    }

    size_t idx; json_t *jn_slave;
    json_array_foreach(jn_slaves, idx, jn_slave) {
        slave_t *s = &priv->slaves[priv->n_slaves];
        if(!json_is_object(jn_slave)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "M-Bus slave is not a dict, left out",
                "slave",        "%j", jn_slave,
                NULL
            );
            continue;
        }

        json_t *jn_address = json_object_get(jn_slave, "address");
        const char *secondary = json_string_value(json_object_get(jn_slave, "secondary"));

        if(jn_address && !secondary) {
            json_int_t address = json_integer_value(jn_address);
            if(!json_is_integer(jn_address) || address < MBUS_ADDRESS_MIN || address > MBUS_ADDRESS_MAX) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "M-Bus primary address must be 1-250, slave left out",
                    "slave",        "%j", jn_slave,
                    NULL
                );
                continue;
            }
            BOOL repeated = FALSE;
            for(int i=0; i<priv->n_slaves; i++) {
                if(priv->slaves[i].address == address) {
                    repeated = TRUE;
                    break;
                }
            }
            if(repeated) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "M-Bus primary address repeated, slave left out",
                    "slave",        "%j", jn_slave,
                    NULL
                );
                continue;
            }
            s->address = (int)address;

        } else if(secondary && !jn_address) {
            const char *manufacturer = json_string_value(json_object_get(jn_slave, "manufacturer"));
            json_t *jn_version = json_object_get(jn_slave, "version");
            json_t *jn_medium = json_object_get(jn_slave, "medium");
            int version = jn_version? (int)json_integer_value(jn_version) : -1;
            int medium = -1;
            if(json_is_string(jn_medium)) {
                medium = mbus_medium_code(json_string_value(jn_medium));
                if(medium < 0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PARAMETER,
                        "msg",          "%s", "M-Bus medium unknown, slave left out",
                        "slave",        "%j", jn_slave,
                        NULL
                    );
                    continue;
                }
            } else if(jn_medium) {
                medium = (int)json_integer_value(jn_medium);
            }
            char why[256] = "";
            if(mbus_build_secondary_address(s->selection, secondary, manufacturer, version, medium,
                    why, sizeof(why)) < 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER,
                    "msg",          "%s", "M-Bus secondary address not valid, slave left out",
                    "why",          "%s", why,
                    "slave",        "%j", jn_slave,
                    NULL
                );
                continue;
            }
            snprintf(s->secondary, sizeof(s->secondary), "%s", secondary);

        } else {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "M-Bus slave needs an address or a secondary, not both, left out",
                "slave",        "%j", jn_slave,
                NULL
            );
            continue;
        }

        s->disabled = json_is_true(json_object_get(jn_slave, "disabled"));
        priv->n_slaves++;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void free_slaves(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    for(int i=0; i<priv->n_slaves; i++) {
        JSON_DECREF(priv->slaves[i].jn_last)
    }
    GBMEM_FREE(priv->slaves);
    priv->n_slaves = 0;
    priv->idx = -1;
}

/***************************************************************************
 *  The slave in a log: its primary or its secondary address
 ***************************************************************************/
PRIVATE const char *slave_name(slave_t *s, char *bf, size_t size)
{
    if(s->address) {
        snprintf(bf, size, "%d", s->address);
    } else {
        snprintf(bf, size, "%s", s->secondary);
    }
    return bf;
}

/***************************************************************************
 *  Bytes in hex, capped, for a warning
 ***************************************************************************/
PRIVATE void dump_bytes(char *dst, size_t size, const uint8_t *bf, size_t len)
{
    size_t n = 0;
    dst[0] = 0;
    for(size_t i=0; i<len && i<DUMP_MAX_BYTES && n + 2 < size; i++) {
        snprintf(dst + n, size - n, "%02X", bf[i]);
        n += 2;
    }
}

/***************************************************************************
 *  Send a frame to the bottom
 ***************************************************************************/
PRIVATE int send_frame(hgobj gobj, const uint8_t *bf, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_create(len, len);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "gbuffer_create() FAILED",
            "len",          "%d", (int)len,
            NULL
        );
        return -1;
    }
    gbuffer_append(gbuf, (void *)bf, len);

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s ==> %s",
            gobj_short_name(gobj),
            gobj_short_name(gobj_bottom_gobj(gobj))
        );
    }

    priv->txFrames++;
    json_t *kw_send = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    return gobj_send_event(gobj_bottom_gobj(gobj), EV_TX_DATA, kw_send, gobj);
}

/***************************************************************************
 *  Send a request, and wait for its answer
 ***************************************************************************/
PRIVATE void send_request(hgobj gobj, const uint8_t *bf, size_t len)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->rx_len = 0;
    priv->rx_bad = FALSE;
    priv->rx_why[0] = 0;
    priv->rx_dump[0] = 0;

    gobj_change_state(gobj, ST_WAIT_RESPONSE);
    set_timeout0(priv->timer, priv->timeout_response);
    send_frame(gobj, bf, len);
}

/***************************************************************************
 *  Send the request of the current step of the cycle
 ***************************************************************************/
PRIVATE void send_step(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    slave_t *s = &priv->slaves[priv->idx];

    uint8_t bf[MBUS_FRAME_MAX_LENGTH];
    size_t len = 0;
    const char *what = "";

    switch(priv->step) {
        case STEP_LINK_RESET:
            len = mbus_build_short_frame(bf, sizeof(bf), MBUS_C_SND_NKE, (uint8_t)s->address);
            what = "SND_NKE";
            break;
        case STEP_SELECT:
            len = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_SND_UD, MBUS_ADDRESS_SELECTED,
                MBUS_CI_SELECT_SECONDARY, s->selection, sizeof(s->selection)
            );
            what = "select";
            break;
        case STEP_REQUEST:
            len = mbus_build_short_frame(bf, sizeof(bf), priv->c_request,
                s->address? (uint8_t)s->address : MBUS_ADDRESS_SELECTED
            );
            what = "REQ_UD2";
            break;
    }

    if(gobj_trace_level(gobj) & TRACE_POLLING) {
        char name[32];
        gobj_trace_msg(gobj, "M-Bus %s to %s, attempt %d, telegram %d",
            what, slave_name(s, name, sizeof(name)), priv->attempts + 1, priv->telegrams + 1
        );
    }

    send_request(gobj, bf, len);
}

/***************************************************************************
 *  Read a slave from its first step
 ***************************************************************************/
PRIVATE void begin_slave(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    slave_t *s = &priv->slaves[priv->idx];

    priv->step = s->address? STEP_LINK_RESET : STEP_SELECT;
    priv->c_request = MBUS_C_REQ_UD2 | MBUS_C_FCB;
    priv->attempts = 0;
    priv->telegrams = 0;
    JSON_DECREF(priv->jn_reading)

    send_step(gobj);
}

/***************************************************************************
 *  The next slave to read, or the end of the cycle
 ***************************************************************************/
PRIVATE void next_slave(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->jn_reading)

    do {
        priv->idx++;
    } while(priv->idx < priv->n_slaves && priv->slaves[priv->idx].disabled);

    if(priv->idx >= priv->n_slaves) {
        if(gobj_trace_level(gobj) & TRACE_POLLING) {
            gobj_trace_msg(gobj, "M-Bus end of cycle");
        }
        priv->idx = -1;
        gobj_change_state(gobj, ST_CONNECTED);
        if(priv->op_pending) {
            start_operation(gobj);
            return;
        }
        set_timeout0(priv->timer, priv->timeout_polling);
        return;
    }

    begin_slave(gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void start_cycle(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->idx = -1;
    next_slave(gobj);
}

/***************************************************************************
 *  A read of the current slave failed: logged when the reason is new
 ***************************************************************************/
PRIVATE void slave_failed(hgobj gobj, const char *reason, const char *detail)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    slave_t *s = &priv->slaves[priv->idx];

    priv->failedReads++;
    if(strcmp(s->failing, reason) != 0) {
        char name[32];
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "M-Bus slave not read",
            "slave",        "%s", slave_name(s, name, sizeof(name)),
            "reason",       "%s", reason,
            "detail",       "%s", detail? detail : "",
            "bytes",        "%s", priv->rx_dump,
            NULL
        );
        snprintf(s->failing, sizeof(s->failing), "%s", reason);
    }

    next_slave(gobj);
}

/***************************************************************************
 *  The current slave was read: publish what it answered
 ***************************************************************************/
PRIVATE void slave_read(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    slave_t *s = &priv->slaves[priv->idx];

    if(s->failing[0]) {
        char name[32];
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "M-Bus slave answered again",
            "slave",        "%s", slave_name(s, name, sizeof(name)),
            "failed_with",  "%s", s->failing,
            NULL
        );
        s->failing[0] = 0;
    }

    json_t *kw = json_object();
    if(s->address) {
        json_object_set_new(kw, "address", json_integer(s->address));
    } else {
        json_object_set_new(kw, "secondary", json_string(s->secondary));
    }
    json_object_update(kw, priv->jn_reading);
    json_object_set_new(kw, "telegrams", json_integer(priv->telegrams));
    JSON_DECREF(priv->jn_reading)

    JSON_DECREF(s->jn_last)
    s->jn_last = json_deep_copy(kw);
    priv->readings++;

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_json(gobj, kw, "PUBLISH %s", gobj_short_name(gobj));
    }
    gobj_publish_event(gobj, EV_ON_MESSAGE, kw);

    next_slave(gobj);
}

/***************************************************************************
 *  Add a telegram to the reading in progress. The records of a second
 *  telegram whose id is already there get the next "#n".
 ***************************************************************************/
PRIVATE void merge_telegram(hgobj gobj, json_t *jn_data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->jn_reading) {
        priv->jn_reading = json_incref(jn_data);
        return;
    }

    json_t *jn_records = json_object_get(priv->jn_reading, "records");
    const char *id; json_t *jn_record;
    json_object_foreach(json_object_get(jn_data, "records"), id, jn_record) {
        if(!json_object_get(jn_records, id)) {
            json_object_set(jn_records, id, jn_record);
            continue;
        }
        char key[NAME_MAX];
        for(int k=2; ; k++) {
            snprintf(key, sizeof(key), "%s#%d", id, k);
            if(!json_object_get(jn_records, key)) {
                break;
            }
        }
        json_object_set(jn_records, key, jn_record);
    }

    const char *mfr = json_string_value(json_object_get(jn_data, "mfr_data"));
    if(mfr) {
        const char *before = json_string_value(json_object_get(priv->jn_reading, "mfr_data"));
        json_object_set_new(priv->jn_reading, "mfr_data", json_sprintf("%s%s", before? before : "", mfr));
    }
}

/***************************************************************************
 *  A valid frame arrived for the step in flight
 ***************************************************************************/
PRIVATE void step_answered(hgobj gobj, const mbus_frame_t *frame)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    slave_t *s = &priv->slaves[priv->idx];

    priv->unexpected_warned = FALSE;

    if(priv->step == STEP_LINK_RESET || priv->step == STEP_SELECT) {
        if(frame->type != MBUS_FRAME_ACK) {
            priv->badFrames++;
            dump_bytes(priv->rx_dump, sizeof(priv->rx_dump), priv->rx, priv->rx_len);
            slave_failed(gobj, "bad answer",
                priv->step == STEP_SELECT? "the selection was not answered with E5" :
                    "SND_NKE was not answered with E5"
            );
            return;
        }
        priv->step = STEP_REQUEST;
        priv->attempts = 0;
        send_step(gobj);
        return;
    }

    /*
     *  STEP_REQUEST: an RSP_UD, from the slave asked (a selected slave
     *  answers with its own primary address, whatever it is)
     */
    if(frame->type != MBUS_FRAME_LONG || (frame->c & 0xCF) != MBUS_C_RSP_UD ||
            (s->address && frame->a != s->address)) {
        priv->badFrames++;
        dump_bytes(priv->rx_dump, sizeof(priv->rx_dump), priv->rx, priv->rx_len);
        slave_failed(gobj, "bad answer", "REQ_UD2 was not answered with an RSP_UD of the slave");
        return;
    }

    char why[256] = "";
    BOOL more = FALSE;
    json_t *jn_data = mbus_decode_user_data(frame, &more, why, sizeof(why));
    if(!jn_data) {
        dump_bytes(priv->rx_dump, sizeof(priv->rx_dump), priv->rx, priv->rx_len);
        slave_failed(gobj, "undecodable", why);
        return;
    }
    if(json_object_get(jn_data, "application_error")) {
        char detail[64];
        snprintf(detail, sizeof(detail), "application error %d",
            (int)json_integer_value(json_object_get(jn_data, "application_error"))
        );
        JSON_DECREF(jn_data)
        slave_failed(gobj, "application error", detail);
        return;
    }
    if(!json_object_get(jn_data, "records")) {
        JSON_DECREF(jn_data)
        dump_bytes(priv->rx_dump, sizeof(priv->rx_dump), priv->rx, priv->rx_len);
        slave_failed(gobj, "bad answer", "REQ_UD2 answered with no records (an alarm status)");
        return;
    }

    priv->telegrams++;
    merge_telegram(gobj, jn_data);
    JSON_DECREF(jn_data)

    if(more) {
        if(priv->telegrams < priv->max_telegrams) {
            priv->c_request ^= MBUS_C_FCB;
            priv->attempts = 0;
            send_step(gobj);
            return;
        }
        if(gobj_trace_level(gobj) & TRACE_POLLING) {
            char name[32];
            gobj_trace_msg(gobj, "M-Bus %s has more telegrams than max_telegrams (%d)",
                slave_name(s, name, sizeof(name)), (int)priv->max_telegrams
            );
        }
    }

    slave_read(gobj);
}

/***************************************************************************
 *  The operation ends: logged, and the cycle goes on (when the bus is open)
 ***************************************************************************/
PRIVATE void finish_operation(hgobj gobj, const char *error, BOOL resume)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *name = kw_get_str(gobj, priv->jn_op, "operation", "", 0);
    json_object_set_new(priv->jn_op, "state", json_string(error? "failed" : "done"));
    json_object_set_new(priv->jn_op, "ended", json_integer((json_int_t)time_in_seconds()));
    json_object_set_new(priv->jn_op, "error", json_string(error? error : ""));

    if(error) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "M-Bus operation failed",
            "operation",    "%s", name,
            "error",        "%s", error,
            "args",         "%j", json_object_get(priv->jn_op, "args"),
            NULL
        );
    } else {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "M-Bus operation done",
            "operation",    "%s", name,
            "found",        "%d", (int)json_array_size(json_object_get(priv->jn_op, "found")),
            NULL
        );
    }

    priv->op = OP_NONE;
    priv->op_asked = OP_NONE;
    priv->op_pending = FALSE;
    JSON_DECREF(priv->jn_reading)

    if(resume) {
        gobj_change_state(gobj, ST_CONNECTED);
        set_timeout0(priv->timer, priv->timeout_polling);
    }
}

/***************************************************************************
 *  What a scan found
 ***************************************************************************/
PRIVATE void op_found(hgobj gobj, json_t *jn_item)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_POLLING) {
        gobj_trace_json(gobj, jn_item, "M-Bus found");
    }
    json_array_append_new(json_object_get(priv->jn_op, "found"), jn_item);
}

/***************************************************************************
 *  Copy who a meter says it is, from the header of its answer
 ***************************************************************************/
PRIVATE void add_identity(json_t *jn_item, const mbus_frame_t *frame)
{
    char bf[256];
    BOOL more;
    json_t *jn_data = mbus_decode_user_data(frame, &more, bf, sizeof(bf));
    json_t *jn_header = jn_data? json_object_get(jn_data, "header") : NULL;
    if(!json_object_get(jn_header, "id")) {
        json_object_set_new(jn_item, "error", json_string(jn_data? "no identification in the answer" : bf));
        JSON_DECREF(jn_data)
        return;
    }
    const char *keys[] = {"id", "manufacturer", "version", "medium", "medium_code"};
    for(size_t i=0; i<ARRAY_SIZE(keys); i++) {
        json_t *v = json_object_get(jn_header, keys[i]);
        if(v) {
            json_object_set(jn_item, strcmp(keys[i], "id")==0? "secondary" : keys[i], v);
        }
    }
    JSON_DECREF(jn_data)
}

/***************************************************************************
 *  The next mask of the secondary search, after the one probed answered
 *  nothing or one meter. Return FALSE when the search is over.
 ***************************************************************************/
PRIVATE BOOL next_secondary_mask(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    while(priv->op_pos >= 0) {
        if(priv->op_mask[priv->op_pos] < '9') {
            priv->op_mask[priv->op_pos]++;
            return TRUE;
        }
        priv->op_mask[priv->op_pos] = 'F';
        priv->op_pos--;
    }
    return FALSE;
}

/***************************************************************************
 *  The scan of primary addresses goes on, or ends
 ***************************************************************************/
PRIVATE void next_primary_address(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->op_address++;
    if(priv->op_address > priv->op_to) {
        finish_operation(gobj, NULL, TRUE);
        return;
    }
    priv->op_step = OPS_PROBE;
    priv->attempts = 0;
    op_send(gobj);
}

/***************************************************************************
 *  The scan of secondary addresses goes on, or ends
 ***************************************************************************/
PRIVATE void next_secondary_probe(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!next_secondary_mask(gobj)) {
        finish_operation(gobj, NULL, TRUE);
        return;
    }
    priv->op_step = OPS_PROBE;
    priv->attempts = 0;
    op_send(gobj);
}

/***************************************************************************
 *  Several meters answered the mask: fix one digit more
 ***************************************************************************/
PRIVATE void secondary_collision(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->op_pos < 7) {
        priv->op_pos++;
        priv->op_mask[priv->op_pos] = '0';
        priv->op_step = OPS_PROBE;
        priv->attempts = 0;
        op_send(gobj);
        return;
    }
    /*
     *  Two meters with the same 8 digits: told apart only by their
     *  manufacturer, version or medium, which this search does not go into
     */
    op_found(gobj, json_pack("{s:s, s:b}", "secondary", priv->op_mask, "collision", 1));
    next_secondary_probe(gobj);
}

/***************************************************************************
 *  Start the operation asked for: the bus is free
 ***************************************************************************/
PRIVATE void start_operation(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->op = priv->op_asked;
    priv->op_asked = OP_NONE;
    priv->op_pending = FALSE;
    priv->op_step = OPS_PROBE;
    priv->attempts = 0;
    json_object_set_new(priv->jn_op, "state", json_string("running"));

    if(priv->op == OP_SCAN_SECONDARY) {
        priv->op_mask[0] = '0';
    }
    op_send(gobj);
}

/***************************************************************************
 *  Send the request of the current step of the operation
 ***************************************************************************/
PRIVATE void op_send(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint8_t bf[MBUS_FRAME_MAX_LENGTH];
    size_t len = 0;
    char why[256] = "";

    switch(priv->op) {
        case OP_SCAN_PRIMARY:
            if(priv->op_step == OPS_PROBE) {
                len = mbus_build_short_frame(bf, sizeof(bf), MBUS_C_SND_NKE, (uint8_t)priv->op_address);
            } else {
                len = mbus_build_short_frame(bf, sizeof(bf), MBUS_C_REQ_UD2|MBUS_C_FCB,
                    (uint8_t)priv->op_address
                );
            }
            break;

        case OP_SCAN_SECONDARY:
            if(priv->op_step == OPS_PROBE) {
                uint8_t sel[MBUS_SECONDARY_ADDRESS_SIZE];
                if(mbus_build_secondary_address(sel, priv->op_mask, NULL, -1, -1, why, sizeof(why)) < 0) {
                    gobj_log_error(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_INTERNAL,
                        "msg",          "%s", "M-Bus search mask not valid",
                        "mask",         "%s", priv->op_mask,
                        "why",          "%s", why,
                        NULL
                    );
                    finish_operation(gobj, "search mask not valid", TRUE);
                    return;
                }
                len = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_SND_UD, MBUS_ADDRESS_SELECTED,
                    MBUS_CI_SELECT_SECONDARY, sel, sizeof(sel)
                );
            } else {
                len = mbus_build_short_frame(bf, sizeof(bf), MBUS_C_REQ_UD2|MBUS_C_FCB,
                    MBUS_ADDRESS_SELECTED
                );
            }
            break;

        case OP_SET_ADDRESS:
            switch(priv->op_step) {
                case OPS_PROBE:
                    len = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_SND_UD, MBUS_ADDRESS_SELECTED,
                        MBUS_CI_SELECT_SECONDARY, priv->op_selection, sizeof(priv->op_selection)
                    );
                    break;
                case OPS_WRITE:
                    {
                        // DIF 01 (8 bits), VIF 7A (bus address), the address
                        uint8_t record[3] = {0x01, 0x7A, (uint8_t)priv->op_new_address};
                        len = mbus_build_long_frame(bf, sizeof(bf), MBUS_C_SND_UD, MBUS_ADDRESS_SELECTED,
                            MBUS_CI_DATA_SEND, record, sizeof(record)
                        );
                    }
                    break;
                default:    // OPS_VERIFY
                    len = mbus_build_short_frame(bf, sizeof(bf), MBUS_C_SND_NKE,
                        (uint8_t)priv->op_new_address
                    );
                    break;
            }
            break;

        default:
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "M-Bus operation unknown",
                "op",           "%d", (int)priv->op,
                NULL
            );
            finish_operation(gobj, "operation unknown", TRUE);
            return;
    }

    if(gobj_trace_level(gobj) & TRACE_POLLING) {
        gobj_trace_msg(gobj, "M-Bus operation %d step %d, address %d, mask %s, attempt %d",
            (int)priv->op, (int)priv->op_step, priv->op_address, priv->op_mask, priv->attempts + 1
        );
    }
    send_request(gobj, bf, len);
}

/***************************************************************************
 *  A valid frame arrived for the step of the operation
 ***************************************************************************/
PRIVATE void op_answered(hgobj gobj, const mbus_frame_t *frame)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL ack = frame->type == MBUS_FRAME_ACK;
    BOOL rsp_ud = frame->type == MBUS_FRAME_LONG && (frame->c & 0xCF) == MBUS_C_RSP_UD;

    switch(priv->op) {
        case OP_SCAN_PRIMARY:
            if(priv->op_step == OPS_PROBE) {
                if(!ack) {
                    priv->badFrames++;
                    op_found(gobj, json_pack("{s:i, s:s}",
                        "address", priv->op_address, "error", "SND_NKE not answered with E5"
                    ));
                    next_primary_address(gobj);
                    return;
                }
                priv->op_step = OPS_IDENTIFY;
                priv->attempts = 0;
                op_send(gobj);
                return;
            }
            {
                json_t *jn_item = json_pack("{s:i}", "address", priv->op_address);
                if(rsp_ud) {
                    add_identity(jn_item, frame);
                } else {
                    priv->badFrames++;
                    json_object_set_new(jn_item, "error", json_string("REQ_UD2 not answered with an RSP_UD"));
                }
                op_found(gobj, jn_item);
                next_primary_address(gobj);
            }
            return;

        case OP_SCAN_SECONDARY:
            if(priv->op_step == OPS_PROBE) {
                if(!ack) {
                    priv->badFrames++;
                    secondary_collision(gobj);
                    return;
                }
                priv->op_step = OPS_IDENTIFY;
                priv->attempts = 0;
                op_send(gobj);
                return;
            }
            if(!rsp_ud) {
                priv->badFrames++;
                secondary_collision(gobj);
                return;
            }
            {
                json_t *jn_item = json_object();
                add_identity(jn_item, frame);
                if(!json_object_get(jn_item, "secondary")) {
                    json_object_set_new(jn_item, "secondary", json_string(priv->op_mask));
                }
                json_object_set_new(jn_item, "address", json_integer(frame->a));
                op_found(gobj, jn_item);
                next_secondary_probe(gobj);
            }
            return;

        case OP_SET_ADDRESS:
            if(!ack) {
                priv->badFrames++;
                finish_operation(gobj,
                    priv->op_step == OPS_PROBE? "the selection was not answered with E5" :
                    priv->op_step == OPS_WRITE? "the write was not answered with E5" :
                        "SND_NKE to the new address was not answered with E5",
                    TRUE
                );
                return;
            }
            if(priv->op_step == OPS_VERIFY) {
                op_found(gobj, json_pack("{s:s, s:i}",
                    "secondary", priv->op_secondary, "address", priv->op_new_address
                ));
                finish_operation(gobj, NULL, TRUE);
                return;
            }
            priv->op_step = priv->op_step == OPS_PROBE? OPS_WRITE : OPS_VERIFY;
            priv->attempts = 0;
            op_send(gobj);
            return;

        default:
            finish_operation(gobj, "operation unknown", TRUE);
            return;
    }
}

/***************************************************************************
 *  No valid answer to the step of the operation, retries spent
 ***************************************************************************/
PRIVATE void op_not_answered(hgobj gobj, BOOL bad)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    switch(priv->op) {
        case OP_SCAN_PRIMARY:
            if(priv->op_step == OPS_PROBE) {
                if(bad) {
                    // several meters with the address: their E5 overlap
                    op_found(gobj, json_pack("{s:i, s:b}", "address", priv->op_address, "collision", 1));
                }
            } else {
                op_found(gobj, json_pack("{s:i, s:s}",
                    "address", priv->op_address,
                    "error", bad? "bad frame to REQ_UD2 (a collision?)" : "no answer to REQ_UD2"
                ));
            }
            next_primary_address(gobj);
            return;

        case OP_SCAN_SECONDARY:
            if(bad) {
                secondary_collision(gobj);
                return;
            }
            if(priv->op_step == OPS_IDENTIFY) {
                op_found(gobj, json_pack("{s:s, s:s}",
                    "secondary", priv->op_mask, "error", "no answer to REQ_UD2"
                ));
            }
            next_secondary_probe(gobj);
            return;

        case OP_SET_ADDRESS:
            {
                char error[128];
                snprintf(error, sizeof(error), "%s %s",
                    bad? "bad frame to" : "no answer to",
                    priv->op_step == OPS_PROBE? "the selection" :
                    priv->op_step == OPS_WRITE? "the write" : "SND_NKE to the new address"
                );
                finish_operation(gobj, error, TRUE);
            }
            return;

        default:
            finish_operation(gobj, "operation unknown", TRUE);
            return;
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *  The bus is open: a cycle starts
 ***************************************************************************/
PRIVATE int ac_connected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->rx_len = 0;
    priv->unexpected_warned = FALSE;

    gobj_change_state(gobj, ST_CONNECTED);

    priv->inform_on_close = TRUE;
    gobj_publish_event(gobj, EV_ON_OPEN, 0);

    if(priv->n_slaves > 0) {
        start_cycle(gobj);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The bus is closed: the reading in progress is dropped, the transport
 *  reconnects on its own and a new cycle starts then
 ***************************************************************************/
PRIVATE int ac_disconnected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout0(priv->timer);
    JSON_DECREF(priv->jn_reading)
    priv->rx_len = 0;
    priv->idx = -1;

    if(priv->op || priv->op_pending) {
        finish_operation(gobj, "the bus closed", FALSE);
    }

    if(gobj_is_volatil(src)) {
        gobj_set_bottom_gobj(gobj, 0);
    }

    if(priv->inform_on_close) {
        priv->inform_on_close = FALSE;
        gobj_publish_event(gobj, EV_ON_CLOSE, 0);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  The time between cycles is over
 ***************************************************************************/
PRIVATE int ac_timeout_polling(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->n_slaves > 0) {
        start_cycle(gobj);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Bytes with no request in flight: warned once, until a valid answer
 ***************************************************************************/
PRIVATE int ac_rx_unexpected(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
    if(gbuf) {
        size_t len = gbuffer_leftbytes(gbuf);
        priv->badFrames++;
        if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
            gobj_trace_dump_gbuf(gobj, gbuf, "%s <== %s (no request in flight)",
                gobj_short_name(gobj), gobj_short_name(src)
            );
        }
        if(!priv->unexpected_warned) {
            char dump[2*DUMP_MAX_BYTES + 1];
            dump_bytes(dump, sizeof(dump), (const uint8_t *)gbuffer_cur_rd_pointer(gbuf), len);
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL,
                "msg",          "%s", "M-Bus bytes with no request in flight, dropped",
                "len",          "%d", (int)len,
                "bytes",        "%s", dump,
                NULL
            );
            priv->unexpected_warned = TRUE;
        }
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Bytes of an answer
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
    if(!gbuf) {
        // Error already logged
        KW_DECREF(kw)
        return -1;
    }

    if(gobj_trace_level(gobj) & TRACE_TRAFFIC) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s <== %s",
            gobj_short_name(gobj),
            gobj_short_name(src)
        );
    }

    if(priv->rx_bad) {
        // the rest of a bad frame: waited out until the timeout
        KW_DECREF(kw)
        return 0;
    }

    size_t len = gbuffer_leftbytes(gbuf);
    if(priv->rx_len + len > sizeof(priv->rx)) {
        priv->rx_bad = TRUE;
        priv->badFrames++;
        snprintf(priv->rx_why, sizeof(priv->rx_why), "more than %d bytes with no frame in them",
            (int)sizeof(priv->rx)
        );
        dump_bytes(priv->rx_dump, sizeof(priv->rx_dump), priv->rx, priv->rx_len);
        KW_DECREF(kw)
        return 0;
    }
    memcpy(priv->rx + priv->rx_len, gbuffer_cur_rd_pointer(gbuf), len);
    priv->rx_len += len;

    mbus_frame_t frame;
    int r = mbus_parse_frame(priv->rx, priv->rx_len, &frame, priv->rx_why, sizeof(priv->rx_why));
    if(r == 0) {
        // more bytes to come
        KW_DECREF(kw)
        return 0;
    }
    if(r < 0 || (size_t)r != priv->rx_len) {
        if(r > 0) {
            snprintf(priv->rx_why, sizeof(priv->rx_why),
                "%d bytes after the frame: a collision or noise", (int)(priv->rx_len - (size_t)r)
            );
        }
        priv->rx_bad = TRUE;
        priv->badFrames++;
        dump_bytes(priv->rx_dump, sizeof(priv->rx_dump), priv->rx, priv->rx_len);
        KW_DECREF(kw)
        return 0;
    }

    priv->rxFrames++;
    if(priv->op) {
        op_answered(gobj, &frame);
    } else {
        step_answered(gobj, &frame);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  No valid answer in timeout_response: repeat, or the slave is not read
 ***************************************************************************/
PRIVATE int ac_timeout_response(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    BOOL bad = priv->rx_bad;
    if(!bad) {
        priv->timeouts++;
    }

    if(priv->attempts < priv->retries) {
        priv->attempts++;
        priv->repeats++;
        if(priv->op) {
            op_send(gobj);
        } else {
            send_step(gobj);
        }
        JSON_DECREF(kw)
        return 0;
    }

    if(priv->op) {
        op_not_answered(gobj, bad);
        JSON_DECREF(kw)
        return 0;
    }

    const char *step = priv->step == STEP_LINK_RESET? "SND_NKE" :
        priv->step == STEP_SELECT? "the selection" : "REQ_UD2";
    char reason[FAILURE_SIZE];
    if(bad) {
        snprintf(reason, sizeof(reason), "bad frame to %s", step);
        slave_failed(gobj, reason, priv->rx_why);
    } else {
        snprintf(reason, sizeof(reason), "no answer to %s", step);
        priv->rx_dump[0] = 0;
        slave_failed(gobj, reason, 0);
    }

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  A child stopped: the transport, or the timer (C_TIMER0 says EV_STOPPED
 *  when a timeout it held is cleared)
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }
    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create  = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start   = mt_start,
    .mt_stop    = mt_stop,
    .mt_writing = mt_writing,
    .mt_reading = mt_reading,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_PROT_MBUS_M);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL,
            "msg",      "%s", "GClass ALREADY created",
            "gclass",   "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_disconnected[] = {
        {EV_CONNECTED,          ac_connected,           ST_CONNECTED},
        {EV_STOPPED,            ac_stopped,             0},
        {0,0,0}
    };
    ev_action_t st_connected[] = {
        {EV_TIMEOUT,            ac_timeout_polling,     0},
        {EV_RX_DATA,            ac_rx_unexpected,       0},
        {EV_TX_READY,           0,                      0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {EV_STOPPED,            ac_stopped,             0},
        {0,0,0}
    };
    ev_action_t st_wait_response[] = {
        {EV_RX_DATA,            ac_rx_data,             0},
        {EV_TIMEOUT,            ac_timeout_response,    0},
        {EV_TX_READY,           0,                      0},
        {EV_DISCONNECTED,       ac_disconnected,        ST_DISCONNECTED},
        {EV_STOPPED,            ac_stopped,             0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,       st_disconnected},
        {ST_CONNECTED,          st_connected},
        {ST_WAIT_RESPONSE,      st_wait_response},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_ON_MESSAGE,         EVF_OUTPUT_EVENT},
        {EV_ON_OPEN,            EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,           EVF_OUTPUT_EVENT},
        {EV_RX_DATA,            0},
        {EV_TIMEOUT,            0},
        {EV_CONNECTED,          0},
        {EV_DISCONNECTED,       0},
        {EV_STOPPED,            0},
        {EV_TX_READY,           0},
        {NULL, 0}
    };

    /*----------------------------------------*
     *          Register GClass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0, // local methods
        attrs_table,
        sizeof(PRIVATE_DATA),
        0, // authz_table,
        command_table,
        s_user_trace_level,
        0 // gcflags
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_prot_mbus_m(void)
{
    return create_gclass(C_PROT_MBUS_M);
}
