/***********************************************************************
 *          C_SGATEWAY.C
 *          Sgateway GClass.
 *
 *          Simple Gateway
 *
 *          A byte relay between two TCP sides:
 *
 *              client --> input_url  (C_TCP_S)
 *                         __input_side__  C_IOGATE
 *                              tcp-1  C_CHANNEL -> C_PROT_RAW -> C_TCP (clisrv)
 *
 *                         __output_side__ C_IOGATE
 *                              output C_CHANNEL -> C_PROT_RAW -> C_TCP --> output_url
 *
 *          What arrives from one side is sent, as it is, to the other one:
 *          a COPY. The read buffer of a C_TCP is read into again as soon as
 *          its EV_RX_DATA is published, and a gbuffer handed to a transport
 *          is its own until its EV_TX_READY (up to 7.30.18 the same buffer
 *          was handed over: as soon as one side was slower, what was in its
 *          queue was overwritten, and the next ones were refused with an
 *          ERROR each).
 *
 *          Back-pressure: what one side sends is not read from the other
 *          one faster than it goes. After a relay the side read is paused
 *          (`rx_paused` of its C_TCP) and it is resumed at the EV_TX_READY
 *          of the side written: a slow peer, or one that does not read,
 *          slows the other peer down through TCP, and the queue of the
 *          gateway holds one read at most.
 *
 *          The input side takes ONE connection at a time (one channel,
 *          `tcp-1`, found by the `child_tree_filter` of its C_TCP_S, legacy
 *          method: the clisrv C_TCP is created at each accept and destroyed
 *          when it ends). A second client is refused while the first one is
 *          connected.
 *
 *          ONE OUTPUT CONNECTION PER CLIENT. The output side is a client
 *          C_TCP to `output_url`, started when a client connects and stopped
 *          when it leaves: no client inherits the session of another one
 *          with the backend (up to 7.30.18 the output connection was shared
 *          by the clients that came one after the other). Until the output
 *          is open the client is not read, and what it had sent is kept and
 *          goes first. Not read, a client that leaves meanwhile is not seen
 *          either: what it sent is delivered when its output opens, and its
 *          leaving is seen then (a pause or a stop before drops it, counted
 *          in `dropMsgs`). An output that cannot connect tries again after
 *          `timeout_between_connections` ms, doubling up to 30 s, while its
 *          client waits; an output that closes once open drops its client.
 *          A TLS output (tcps://) takes its configuration from `crypto`, a
 *          TLS input (tcps://) from `input_crypto`.
 *
 *          Bytes that arrive for a side that has closed are dropped
 *          (counted in `dropMsgs`; one WARNING per side each time it
 *          closes, not one per message).
 *
 *          A pause (and the stop of the yuno, which pauses it first) ends
 *          the client and its output, and starts no output again: in
 *          7.30.19 the end of the output's stop started it again for the
 *          client still being closed (one that did not read held its close
 *          with a write in flight): a session with the backend while the
 *          gateway was paused, and "Destroying a RUNNING gobj" at the stop.
 *
 *          The open and the close of each side are said at the trace level
 *          `connections` only: a peer that connects again and again does
 *          not write the log.
 *
 *          The urls are taken at each play: write `input_url` and
 *          `output_url` (config kw, or write-str at run time, then pause
 *          and play the yuno). A play without them is refused with an
 *          ERROR, and the yuno goes on running, not playing.
 *
 *          Copyright (c) 2020 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include "c_sgateway.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define OUTPUT_RECONNECT_MAX_MS     30000   // backoff cap of the output's connection attempts

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int create_input_side(hgobj gobj);
PRIVATE int create_output_side(hgobj gobj);
PRIVATE int set_output_url(hgobj gobj, const char *output_url);
PRIVATE int open_input_side(hgobj gobj);
PRIVATE int close_input_side(hgobj gobj);
PRIVATE int open_output_side(hgobj gobj);
PRIVATE int close_output_side(hgobj gobj);
PRIVATE int connect_output(hgobj gobj);
PRIVATE void disconnect_output(hgobj gobj);
PRIVATE int relay(hgobj gobj, hgobj dst, gbuffer_t *gbuf, hgobj src_tcp);
PRIVATE void set_reading(hgobj tcp, BOOL paused);
PRIVATE const char *side_name(hgobj gobj, hgobj side);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authzs[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "authz",        0,              0,          "permission to search"),
SDATAPM (DTP_STRING,    "service",      0,              0,          "Service where to search the permission. If empty print all service's permissions"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATACM2 (DTP_SCHEMA,   "authzs",           0,                  0,              pm_authzs,      cmd_authzs,     "Authorization's help"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description---------- */
SDATA (DTP_STRING,      "input_url",        SDF_WR|SDF_PERSIST, 0,          "Url where the input side listens (tcp://host:port, tcps:// with TLS). Taken at each play"),
SDATA (DTP_STRING,      "output_url",       SDF_WR|SDF_PERSIST, 0,          "Url the output side connects to (tcp://host:port, tcps:// with TLS, see crypto). Taken at each play"),
SDATA (DTP_DICT,        "crypto",           SDF_RD,             "{\"ssl_use_system_ca\": true, \"ssl_verify_mode\": \"required\"}", "TLS of the output side (tcps://). Verifies the server's certificate against the system CA by default; a private CA: {\"ssl_trusted_certificate\": \"/path/ca.pem\"}. Replaces the default whole. Taken at each play"),
SDATA (DTP_DICT,        "input_crypto",     SDF_RD,             "{}",       "TLS of the input side (tcps://), the `crypto` of its C_TCP_S: {\"ssl_certificate\": \"/path/cert.pem\", \"ssl_certificate_key\": \"/path/key.pem\"}. Without a certificate a tcps:// input does not listen and the yuno exits. Taken at each play"),
SDATA (DTP_INTEGER,     "timeout_between_connections", SDF_RD, "100",       "Output side: ms to wait before connecting again when its connection fails, doubled at each failure up to 30 s"),
SDATA (DTP_INTEGER,     "txMsgs",           SDF_RD|SDF_RSTATS,  0,          "Messages sent to the other side"),
SDATA (DTP_INTEGER,     "rxMsgs",           SDF_RD|SDF_RSTATS,  0,          "Messages received from either side"),
SDATA (DTP_INTEGER,     "dropMsgs",         SDF_RD|SDF_RSTATS,  0,          "Messages dropped: the other side had closed, or a pause or a stop came before the output of the client opened"),

SDATA (DTP_INTEGER,     "txMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (DTP_INTEGER,     "rxMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (DTP_INTEGER,     "maxtxMsgsec",      SDF_WR|SDF_RSTATS,  0,          "Max Messages by second"),
SDATA (DTP_INTEGER,     "maxrxMsgsec",      SDF_WR|SDF_RSTATS,  0,          "Max Messages by second"),
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",     "Period of the rates, in ms"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "Subscriber of output-events"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES      = 0x0001,
    TRACE_CONNECTIONS   = 0x0002,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{"connections",     "Trace the open and the close of each side"},
{0, 0},
};

/*---------------------------------------------*
 *      GClass authz levels
 *---------------------------------------------*/
PRIVATE sdata_desc_t authz_table[] = {
/*-AUTHZ-- type---------name----------------flag----alias---items---description--*/
SDATA_END()
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int32_t timeout;
    hgobj timer;

    hgobj gobj_input_side;
    hgobj gobj_server_port;
    hgobj gobj_input_channel;
    hgobj gobj_input_tcp;       // the C_TCP of the client connected now
    hgobj gobj_output_side;
    hgobj gobj_output_prot;
    hgobj gobj_output_tcp;
    BOOL input_side_opened;
    BOOL output_side_opened;
    BOOL output_wanted;         // a client is connected: its output connects
    BOOL input_drop_said;
    BOOL output_drop_said;
    gbuffer_t *gbuf_pending;    // what the client sent before its output opened
    json_int_t client_seq;      // clients connected, to tell whose drop was posted
    json_int_t output_owner_seq;    // the client whose output was started last

    json_int_t txMsgs;
    json_int_t rxMsgs;
    json_int_t dropMsgs;
    json_int_t txMsgsec;
    json_int_t rxMsgsec;

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
     *  SERVICE subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    } else if(gobj_is_pure_child(gobj)) {
        subscriber = gobj_parent(gobj);
        gobj_subscribe_event(gobj, NULL, NULL, subscriber);
    }

    create_input_side(gobj);
    create_output_side(gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,               gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,             gobj_read_integer_attr)
    END_EQ_SET_PRIV()

    /*
     *  `stats=__reset__` writes the defaults of the SDF_RSTATS attrs: the
     *  counters live in priv (mt_reading)
     */
    if(path && strcmp(path, "txMsgs")==0) {
        priv->txMsgs = 0;
    } else if(path && strcmp(path, "rxMsgs")==0) {
        priv->rxMsgs = 0;
    } else if(path && strcmp(path, "dropMsgs")==0) {
        priv->dropMsgs = 0;
    }
}

/***************************************************************************
 *      Framework Method reading
 ***************************************************************************/
PRIVATE SData_Value_t mt_reading(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    SData_Value_t v = {0,{0}};
    if(strcmp(name, "txMsgs")==0) {
        v.found = 1;
        v.v.i = priv->txMsgs;
    } else if(strcmp(name, "rxMsgs")==0) {
        v.found = 1;
        v.v.i = priv->rxMsgs;
    } else if(strcmp(name, "dropMsgs")==0) {
        v.found = 1;
        v.v.i = priv->dropMsgs;
    }
    return v;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    GBUFFER_DECREF(priv->gbuf_pending)
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

    clear_timeout(priv->timer);
    gobj_stop(priv->timer);
    return 0;
}

/***************************************************************************
 *      Framework Method play
 *  Yuneta rule:
 *  If service has mt_play then start only the service gobj.
 *      (Let mt_play be responsible to start their tree)
 *  If service has not mt_play then start the tree with gobj_start_tree().
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *input_url = gobj_read_str_attr(gobj, "input_url");
    const char *output_url = gobj_read_str_attr(gobj, "output_url");

    if(empty_string(input_url)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "What yuno input url?",
            NULL
        );
        return -1;
    }

    if(empty_string(output_url)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "What yuno output url?",
            NULL
        );
        return -1;
    }

    if(!priv->gobj_server_port || !priv->gobj_output_prot) {
        // Error already logged (create_input_side(), create_output_side())
        return -1;
    }

    gobj_write_str_attr(priv->gobj_server_port, "url", input_url);
    gobj_write_new_json_attr(priv->gobj_server_port, "crypto",
        json_deep_copy(gobj_read_json_attr(gobj, "input_crypto"))
    );
    if(set_output_url(gobj, output_url) < 0) {
        // Error already logged
        return -1;
    }

    open_input_side(gobj);
    open_output_side(gobj);

    set_timeout_periodic(priv->timer, priv->timeout);

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->output_wanted = FALSE;    // the end of the output's stop must not start it again
    close_input_side(gobj);
    close_output_side(gobj);

    clear_timeout(priv->timer);
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
    KW_INCREF(kw);
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
 *
 ***************************************************************************/
PRIVATE json_t *cmd_authzs(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw)
    json_t *jn_resp = gobj_build_authzs_doc(gobj, cmd, kw);
    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_resp,
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  __input_side__: a C_TCP_S and one channel, tcp-1, with a bare
 *  C_PROT_RAW: the C_TCP_S creates its C_TCP at each accept (legacy
 *  method, child_tree_filter). Its url is written at each play.
 ***************************************************************************/
PRIVATE int create_input_side(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_iogate = json_pack("{s:I}",
        "subscriber", (json_int_t)(uintptr_t)gobj
    );
    priv->gobj_input_side = gobj_create_service(
        "__input_side__",
        C_IOGATE,
        kw_iogate,
        gobj
    );
    if(!priv->gobj_input_side) {
        // Error already logged
        return -1;
    }

    json_t *kw_server = json_pack("{s:{s:{s:s, s:s, s:b, s:b}}}",
        "child_tree_filter",
            "kw",
                "__prefix_gobj_name__", "tcp-",
                "__gclass_name__", C_CHANNEL,
                "__disabled__", 0,
                "connected", 0
    );
    priv->gobj_server_port = gobj_create(
        "server_port",
        C_TCP_S,
        kw_server,
        priv->gobj_input_side
    );
    if(!priv->gobj_server_port) {
        // Error already logged
        return -1;
    }

    priv->gobj_input_channel = gobj_create(
        "tcp-1",
        C_CHANNEL,
        0,
        priv->gobj_input_side
    );
    if(!priv->gobj_input_channel) {
        // Error already logged
        return -1;
    }

    hgobj gobj_prot_raw = gobj_create(
        "tcp-1",
        C_PROT_RAW,
        0,
        priv->gobj_input_channel
    );
    if(!gobj_prot_raw) {
        // Error already logged
        return -1;
    }
    gobj_set_bottom_gobj(priv->gobj_input_channel, gobj_prot_raw);

    return 0;
}

/***************************************************************************
 *  __output_side__: one channel, output, with a C_PROT_RAW whose client
 *  C_TCP is created by set_output_url(), at the first play: a C_TCP is a
 *  client when it is created with a url.
 ***************************************************************************/
PRIVATE int create_output_side(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_iogate = json_pack("{s:I}",
        "subscriber", (json_int_t)(uintptr_t)gobj
    );
    priv->gobj_output_side = gobj_create_service(
        "__output_side__",
        C_IOGATE,
        kw_iogate,
        gobj
    );
    if(!priv->gobj_output_side) {
        // Error already logged
        return -1;
    }

    hgobj gobj_channel = gobj_create(
        "output",
        C_CHANNEL,
        0,
        priv->gobj_output_side
    );
    if(!gobj_channel) {
        // Error already logged
        return -1;
    }

    priv->gobj_output_prot = gobj_create(
        "output",
        C_PROT_RAW,
        0,
        gobj_channel
    );
    if(!priv->gobj_output_prot) {
        // Error already logged
        return -1;
    }
    gobj_set_bottom_gobj(gobj_channel, priv->gobj_output_prot);

    return 0;
}

/***************************************************************************
 *  The client C_TCP of the output side: created with the url at the first
 *  play, its url and crypto written at the next ones (it is stopped then).
 *  A copy of crypto: the C_TCP writes its ssl_server_name in it.
 *  The gateway hears its EV_TX_READY (the back-pressure of the client) and
 *  its EV_STOPPED (a stop that waited for its write in flight).
 ***************************************************************************/
PRIVATE int set_output_url(hgobj gobj, const char *output_url)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_output_tcp) {
        if(strcmp(gobj_read_str_attr(priv->gobj_output_tcp, "url"), output_url)!=0) {
            gobj_write_str_attr(priv->gobj_output_tcp, "url", output_url);
        }
        gobj_write_new_json_attr(priv->gobj_output_tcp, "crypto",
            json_deep_copy(gobj_read_json_attr(gobj, "crypto"))
        );
        return 0;
    }

    json_t *kw_tcp = json_pack("{s:s, s:I, s:I, s:o}",
        "url", output_url,
        "timeout_between_connections",
            (json_int_t)gobj_read_integer_attr(gobj, "timeout_between_connections"),
        "timeout_between_connections_max", (json_int_t)OUTPUT_RECONNECT_MAX_MS,
        "crypto", json_deep_copy(gobj_read_json_attr(gobj, "crypto"))
    );
    priv->gobj_output_tcp = gobj_create_pure_child(
        "output",
        C_TCP,
        kw_tcp,
        priv->gobj_output_prot
    );
    if(!priv->gobj_output_tcp) {
        // Error already logged
        return -1;
    }
    gobj_set_bottom_gobj(priv->gobj_output_prot, priv->gobj_output_tcp);

    gobj_subscribe_event(priv->gobj_output_tcp, EV_TX_READY, NULL, gobj);
    gobj_subscribe_event(priv->gobj_output_tcp, EV_STOPPED, NULL, gobj);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int open_input_side(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start_tree(priv->gobj_input_side);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_input_side(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_input_side && gobj_is_running(priv->gobj_input_side)) {
        gobj_stop_tree(priv->gobj_input_side);
    }

    return 0;
}

/***************************************************************************
 *  The C_TCP is manual start: it is started when a client connects
 *  (connect_output())
 ***************************************************************************/
PRIVATE int open_output_side(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start_tree(priv->gobj_output_side);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int close_output_side(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_output_side && gobj_is_running(priv->gobj_output_side)) {
        gobj_stop_tree(priv->gobj_output_side);
    }

    return 0;
}

/***************************************************************************
 *  The output of the client that connected. A stop still waiting for its
 *  write in flight (ST_WAIT_STOPPED: the client before, or a pause and a
 *  play) has not ended: the output starts at its EV_STOPPED. Up to 7.30.18
 *  a play in that case started nothing, with an ERROR, and the output side
 *  stayed dead.
 ***************************************************************************/
PRIVATE int connect_output(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj tcp = priv->gobj_output_tcp;

    if(gobj_is_running(tcp)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "The output is running with no client: not given to the new one",
            "state",        "%s", gobj_current_state(tcp),
            NULL
        );
        return -1;
    }
    if(!gobj_in_this_state(tcp, ST_STOPPED) && !gobj_in_this_state(tcp, ST_DISCONNECTED)) {
        return 0;   // its stop has not ended: started at its EV_STOPPED
    }

    set_reading(tcp, FALSE);
    priv->output_owner_seq = priv->client_seq;
    if(gobj_start(tcp) < 0) {
        // Error already logged
        return -1;
    }
    return 0;
}

/***************************************************************************
 *  The client left: its output goes too
 ***************************************************************************/
PRIVATE void disconnect_output(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Closed for relaying now: its stop may wait for a write in flight,
     *  and the next client's data must not reach this session meanwhile
     */
    priv->output_side_opened = FALSE;
    if(priv->gobj_output_tcp && gobj_is_running(priv->gobj_output_tcp)) {
        gobj_stop(priv->gobj_output_tcp);
    }
}

/***************************************************************************
 *  Send gbuf (owned) to the side dst. The side it came from (src_tcp) is
 *  read no more until the EV_TX_READY of dst (back-pressure).
 ***************************************************************************/
PRIVATE int relay(hgobj gobj, hgobj dst, gbuffer_t *gbuf, hgobj src_tcp)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_send = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    if(gobj_send_event(dst, EV_SEND_MESSAGE, kw_send, gobj) < 0) {
        return -1;  // Error already logged
    }
    priv->txMsgs++;
    priv->txMsgsec++;

    if(src_tcp) {
        set_reading(src_tcp, TRUE);
    }
    return 0;
}

/***************************************************************************
 *  The flow control of a side: its C_TCP reads, or not
 ***************************************************************************/
PRIVATE void set_reading(hgobj tcp, BOOL paused)
{
    gobj_write_bool_attr(tcp, "rx_paused", paused);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *side_name(hgobj gobj, hgobj side)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(side == priv->gobj_input_side) {
        return "input";
    } else if(side == priv->gobj_output_side) {
        return "output";
    }
    return "???";
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        hgobj tcp = gobj_last_bottom_gobj(priv->gobj_input_channel);
        if(!tcp) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "Input side open without its C_TCP",
                NULL
            );
            KW_DECREF(kw);
            return -1;
        }
        priv->input_side_opened = TRUE;
        priv->input_drop_said = FALSE;
        priv->gobj_input_tcp = tcp;
        priv->client_seq++;

        /*
         *  The client is not read until its output is open: what the read
         *  in flight brings is kept (gbuf_pending)
         */
        set_reading(tcp, TRUE);
        gobj_subscribe_event(tcp, EV_TX_READY, NULL, gobj);
        priv->output_wanted = TRUE;
        connect_output(gobj);   // on failure: logged

    } else if(src == priv->gobj_output_side) {
        priv->output_side_opened = TRUE;
        priv->output_drop_said = FALSE;

        if(priv->gbuf_pending) {
            gbuffer_t *gbuf = priv->gbuf_pending;
            priv->gbuf_pending = 0;
            relay(gobj, priv->gobj_output_side, gbuf, priv->gobj_input_tcp); // on failure: logged
        } else if(priv->gobj_input_tcp) {
            set_reading(priv->gobj_input_tcp, FALSE);
        }

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "EV_ON_OPEN from an unknown source",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }

    if(gobj_trace_level(gobj) & TRACE_CONNECTIONS) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Side open",
            "side",         "%s", side_name(gobj, src),
            "url",          "%s", gobj_read_str_attr(
                gobj, (src == priv->gobj_input_side)? "input_url" : "output_url"
            ),
            NULL
        );
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        priv->input_side_opened = FALSE;
        priv->output_wanted = FALSE;
        if(priv->gobj_input_tcp) {
            gobj_unsubscribe_event(priv->gobj_input_tcp, EV_TX_READY, NULL, gobj);
            priv->gobj_input_tcp = 0;
        }
        if(priv->gbuf_pending) {
            priv->dropMsgs++;   // the client left before its output opened
            GBUFFER_DECREF(priv->gbuf_pending)
        }
        disconnect_output(gobj);

    } else if(src == priv->gobj_output_side) {
        priv->output_side_opened = FALSE;
        if(priv->input_side_opened) {
            /*
             *  Its client goes too. Posted: this runs inside a publish of
             *  the output C_TCP, and the end of the client stops it
             */
            gobj_post_event(gobj, EV_DROP,
                json_pack("{s:I}", "client", priv->output_owner_seq),
                gobj
            );
        }

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "EV_ON_CLOSE from an unknown source",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }

    if(gobj_trace_level(gobj) & TRACE_CONNECTIONS) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "Side closed",
            "side",         "%s", side_name(gobj, src),
            NULL
        );
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  What comes from one side is sent to the other one, if it is open
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj dst;
    hgobj src_tcp;
    BOOL dst_opened;
    BOOL *drop_said;
    if(src == priv->gobj_input_side) {
        dst = priv->gobj_output_side;
        src_tcp = priv->gobj_input_tcp;
        dst_opened = priv->output_side_opened;
        drop_said = &priv->output_drop_said;
    } else if(src == priv->gobj_output_side) {
        dst = priv->gobj_input_side;
        src_tcp = priv->gobj_output_tcp;
        dst_opened = priv->input_side_opened;
        drop_said = &priv->input_drop_said;
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "EV_ON_MESSAGE from an unknown source",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "EV_ON_MESSAGE without gbuffer",
            "side",         "%s", side_name(gobj, src),
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }

    priv->rxMsgs++;
    priv->rxMsgsec++;

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gobj_trace_dump_gbuf(gobj, gbuf, "%s ==> %s",
            side_name(gobj, src),
            side_name(gobj, dst)
        );
    }

    size_t len = gbuffer_leftbytes(gbuf);

    if(!dst_opened && src == priv->gobj_input_side && priv->output_wanted) {
        /*
         *  Its output is connecting: kept, it goes first when it opens. The
         *  client is not read meanwhile, so this is one read at most
         */
        if(!priv->gbuf_pending) {
            priv->gbuf_pending = gbuffer_create(len, gbmem_get_maximum_block());
        }
        if(!priv->gbuf_pending ||
                gbuffer_append(priv->gbuf_pending, gbuffer_cur_rd_pointer(gbuf), len) != len) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_MEMORY,
                "msg",          "%s", "Cannot keep what the client sent before its output opened",
                "bytes",        "%d", (int)len,
                NULL
            );
            priv->dropMsgs++;
        }
        KW_DECREF(kw);
        return 0;
    }

    if(!dst_opened) {
        priv->dropMsgs++;
        if(!*drop_said) {
            *drop_said = TRUE;
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_OPERATIONAL,
                "msg",          "%s", "Side not open, data dropped until it opens",
                "from",         "%s", side_name(gobj, src),
                "to",           "%s", side_name(gobj, dst),
                "bytes",        "%d", (int)len,
                NULL
            );
        }
        KW_DECREF(kw);
        return 0;
    }

    /*
     *  A copy, in a kw of its own: the buffer received is the read buffer
     *  of its C_TCP, read into again once this returns, and the kw carries
     *  the routing (__temp__) of the side it came from
     */
    gbuffer_t *gbuf_copy = gbuffer_create(len, len);
    if(!gbuf_copy) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_MEMORY,
            "msg",          "%s", "Cannot copy the message to relay",
            "bytes",        "%d", (int)len,
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }
    gbuffer_append(gbuf_copy, gbuffer_cur_rd_pointer(gbuf), len);
    relay(gobj, dst, gbuf_copy, src_tcp);   // on failure: logged

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  The side written took everything: the other one is read again
 ***************************************************************************/
PRIVATE int ac_tx_ready(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_tcp) {
        if(priv->gobj_input_tcp) {
            set_reading(priv->gobj_input_tcp, FALSE);
        }
    } else if(src == priv->gobj_input_tcp) {
        set_reading(priv->gobj_output_tcp, FALSE);
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "EV_TX_READY from an unknown source",
            "src",          "%s", gobj_full_name(src),
            NULL
        );
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  The output dropped its client (posted by ac_on_close()): the client of
 *  then, not one that came after
 ***************************************************************************/
PRIVATE int ac_drop_client(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t client = kw_get_int(gobj, kw, "client", 0, KW_REQUIRED);
    if(client == priv->client_seq && priv->input_side_opened) {
        gobj_send_event(priv->gobj_input_channel, EV_DROP, 0, gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  A stop of the output that waited for its write in flight has ended: the
 *  output of a client connected meanwhile starts now. The EV_STOPPED of
 *  the sides themselves says nothing to the gateway.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_output_tcp && priv->output_wanted) {
        connect_output(gobj);   // on failure: logged
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  The rates of the last period
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t maxtxMsgsec = gobj_read_integer_attr(gobj, "maxtxMsgsec");
    json_int_t maxrxMsgsec = gobj_read_integer_attr(gobj, "maxrxMsgsec");
    if(priv->txMsgsec > maxtxMsgsec) {
        gobj_write_integer_attr(gobj, "maxtxMsgsec", priv->txMsgsec);
    }
    if(priv->rxMsgsec > maxrxMsgsec) {
        gobj_write_integer_attr(gobj, "maxrxMsgsec", priv->rxMsgsec);
    }

    gobj_write_integer_attr(gobj, "txMsgsec", priv->txMsgsec);
    gobj_write_integer_attr(gobj, "rxMsgsec", priv->rxMsgsec);

    priv->rxMsgsec = 0;
    priv->txMsgsec = 0;

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *                          FSM
 ***************************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create      = mt_create,
    .mt_destroy     = mt_destroy,
    .mt_start       = mt_start,
    .mt_stop        = mt_stop,
    .mt_play        = mt_play,
    .mt_pause       = mt_pause,
    .mt_writing     = mt_writing,
    .mt_reading     = mt_reading,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_SGATEWAY);

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
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "GClass ALREADY created",
            "gclass",       "%s", gclass_name,
            NULL
        );
        return -1;
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_ON_MESSAGE,         ac_on_message,      0},
        {EV_ON_OPEN,            ac_on_open,         0},
        {EV_ON_CLOSE,           ac_on_close,        0},
        {EV_TX_READY,           ac_tx_ready,        0},
        {EV_DROP,               ac_drop_client,     0},
        {EV_TIMEOUT_PERIODIC,   ac_timeout,         0},
        {EV_STOPPED,            ac_stopped,         0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,             st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_ON_MESSAGE,         0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
        {EV_TX_READY,           0},
        {EV_DROP,               0},
        {EV_TIMEOUT_PERIODIC,   0},
        {EV_STOPPED,            0},
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
        0,  // Local method table (LMT)
        attrs_table,
        sizeof(PRIVATE_DATA),
        authz_table,
        command_table,
        s_user_trace_level,
        0   // GClass flags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_sgateway(void)
{
    return create_gclass(C_SGATEWAY);
}
