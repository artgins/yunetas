/***********************************************************************
 *          C_EMU_DEVICE.C
 *          Emu_device GClass.
 *
 *          Emulator of device gates
 *
 *          Replays recorded frames (field "frame64" of a timeranger2 topic)
 *          out through __output_side__ at a controlled rate: `window` frames
 *          every `interval` milliseconds. Used to feed device-facing gates /
 *          ingest pipelines without real hardware.
 *
 *          Copyright (c) 2018 Niyamaka.
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include "c_emu_device.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int create_output_side(hgobj gobj);
PRIVATE int collect_record_cb(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *jn_record   // must be owned
);
PRIVATE int send_frame_b64(hgobj gobj, const char *frame64);
PRIVATE int send_window(hgobj gobj);
PRIVATE BOOL frames_exhausted(hgobj gobj);
PRIVATE int finish_replay(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_write_interval(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_write_window(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_read_parameters(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_write_window[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER,   "window",       0,              0,          "Number of messages to send in each interval."),
SDATA_END()
};
PRIVATE sdata_desc_t pm_write_interval[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER,   "interval",      0,              0,         "Interval in miliseconds to send 'window' frames."),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias-----------items---------------json_fn-------------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,         pm_help,            cmd_help,           "Command's help"),
SDATACM (DTP_SCHEMA,    "read-parameters",  0,              0,                  cmd_read_parameters,"View parameters."),
SDATACM (DTP_SCHEMA,    "write-window",     0,              pm_write_window,    cmd_write_window,   "Write window parameter, number of frames to send in each interval."),
SDATACM (DTP_SCHEMA,    "write-interval",   0,              pm_write_interval,  cmd_write_interval, "Write interval parameter, in miliseconds."),
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag------------------------default---------description---------- */
SDATA (DTP_INTEGER,     "window",           SDF_WR|SDF_PERSIST,         "1",              "Number of messages to send in each interval event"),
SDATA (DTP_INTEGER,     "interval",         SDF_WR|SDF_PERSIST,         "1000",           "Interval in miliseconds to send 'window' frames"),
SDATA (DTP_INTEGER,     "txMsgs",           SDF_RD,                     0,              "Messages transmitted by this socket"),
SDATA (DTP_INTEGER,     "rxMsgs",           SDF_RD,                     0,              "Messages received by this socket"),

SDATA (DTP_STRING,      "url",                  SDF_WR|SDF_PERSIST,         0,   "Url of __output_side__ yuno."                 ),
SDATA (DTP_STRING,      "path",                 SDF_WR|SDF_PERSIST,         0,   "Path of database."                 ),
SDATA (DTP_STRING,      "database",             SDF_WR|SDF_PERSIST,         0,   "Database Name."                    ),
SDATA (DTP_STRING,      "topic",                SDF_WR|SDF_PERSIST,         0,   "Database Topic."                   ),
SDATA (DTP_STRING,      "leading",              SDF_WR|SDF_PERSIST,         0,   "Leading data (base64 frame sent on connect)."),
SDATA (DTP_STRING,      "from_t",               SDF_WR|SDF_PERSIST,         0,   "From time."                        ),
SDATA (DTP_STRING,      "to_t",                 SDF_WR|SDF_PERSIST,         0,   "To time."                          ),
SDATA (DTP_STRING,      "from_rowid",           SDF_WR|SDF_PERSIST,         0,   "From rowid."                       ),
SDATA (DTP_STRING,      "to_rowid",             SDF_WR|SDF_PERSIST,         0,   "To rowid."                         ),
SDATA (DTP_STRING,      "user_flag_mask_set",   SDF_WR|SDF_PERSIST,         0,   "Mask of User Flag set."            ),
SDATA (DTP_STRING,      "user_flag_mask_notset",SDF_WR|SDF_PERSIST,         0,   "Mask of User Flag not set."        ),
SDATA (DTP_STRING,      "system_flag_mask_set", SDF_WR|SDF_PERSIST,         0,   "Mask of System Flag set."          ),
SDATA (DTP_STRING,      "system_flag_mask_notset",SDF_WR|SDF_PERSIST,       0,   "Mask of System Flag not set."      ),
SDATA (DTP_STRING,      "key",                  SDF_WR|SDF_PERSIST,         0,   "Key (default: all keys)."          ),
SDATA (DTP_STRING,      "notkey",               SDF_WR|SDF_PERSIST,         0,   "Not key."                          ),

SDATA (DTP_INTEGER,     "timeout",          SDF_RD,                     "2000",         "Timeout"),
SDATA (DTP_POINTER,     "user_data",        0,                          0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                          0,              "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_INFO = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"info",        "Trace user information"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    uint32_t interval;
    uint32_t window;
    hgobj timer_interval;
    uint64_t txMsgs;
    uint64_t rxMsgs;
    hgobj gobj_output_side;
    hgobj gobj_output_tcp;  /* the C_TCP bottom; manual_start, so started explicitly */

    json_t *tranger;
    json_t *jn_list;        /* tranger2_open_list handle */
    json_t *match_cond;
    json_t *jn_frames;      /* loaded records (with "frame64"), drained at window/interval rate */
    json_int_t frame_idx;   /* index of the next frame to send */

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

    priv->timer_interval = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    create_output_side(gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(interval,              gobj_read_integer_attr)
    SET_PRIV(window,                gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(interval,         gobj_read_integer_attr)
        if(gobj_find_service("agent_client", FALSE)) {
            gobj_save_persistent_attrs(gobj, 0);
        }
        if(gobj_is_playing(gobj)) {
            set_timeout_periodic(priv->timer_interval, priv->interval);
        }

    ELIF_EQ_SET_PRIV(window,        gobj_read_integer_attr)
        if(gobj_find_service("agent_client", FALSE)) {
            gobj_save_persistent_attrs(gobj, 0);
        }
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer_interval);

    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer_interval);
    gobj_stop(priv->timer_interval);

    return 0;
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj agent_client = gobj_find_service("agent_client", FALSE);

    const char *url = gobj_read_str_attr(gobj, "url");
    const char *path = gobj_read_str_attr(gobj, "path");
    const char *database = gobj_read_str_attr(gobj, "database");
    const char *topic = gobj_read_str_attr(gobj, "topic");

    if(empty_string(url)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "What __output_side__ url?",
            NULL
        );
        if(agent_client) {
            gobj_send_event(agent_client, EV_PAUSE_YUNO, 0, gobj);
            return -1;
        }
        fprintf(stderr, "What __output_side__ url?\n");
        exit(-1);
    }

    if(empty_string(path)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "What TimeRanger path?",
            NULL
        );
        if(agent_client) {
            gobj_send_event(agent_client, EV_PAUSE_YUNO, 0, gobj);
            return -1;
        }
        fprintf(stderr, "What TimeRanger path?\n");
        exit(-1);
    }

    /*
     *  If path points directly at a topic directory (contains topic_desc.json),
     *  derive path/database/topic from it.
     */
    char bftemp[PATH_MAX];
    snprintf(bftemp, sizeof(bftemp), "%s%s%s",
        path,
        (path[strlen(path)-1]=='/')?"":"/",
        "topic_desc.json"
    );
    if(is_regular_file(bftemp)) {
        pop_last_segment(bftemp); // pop topic_desc.json
        topic = pop_last_segment(bftemp);
        database = pop_last_segment(bftemp);
        path = bftemp;
    }

    if(empty_string(database)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "What Database?",
            NULL
        );
        if(agent_client) {
            gobj_send_event(agent_client, EV_PAUSE_YUNO, 0, gobj);
            return -1;
        }
        fprintf(stderr, "What Database? (try: tr2list --list-databases %s)\n", path);
        exit(-1);
    }
    if(empty_string(topic)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "What Topic?",
            NULL
        );
        if(agent_client) {
            gobj_send_event(agent_client, EV_PAUSE_YUNO, 0, gobj);
            return -1;
        }
        fprintf(stderr, "What Topic?\n");
        exit(-1);
    }

    /*----------------------------------*
     *  Build match conditions
     *----------------------------------*/
    const char *from_t = gobj_read_str_attr(gobj, "from_t");
    const char *to_t = gobj_read_str_attr(gobj, "to_t");
    const char *from_rowid = gobj_read_str_attr(gobj, "from_rowid");
    const char *to_rowid = gobj_read_str_attr(gobj, "to_rowid");
    const char *user_flag_mask_set = gobj_read_str_attr(gobj, "user_flag_mask_set");
    const char *user_flag_mask_notset = gobj_read_str_attr(gobj, "user_flag_mask_notset");
    const char *system_flag_mask_set = gobj_read_str_attr(gobj, "system_flag_mask_set");
    const char *system_flag_mask_notset = gobj_read_str_attr(gobj, "system_flag_mask_notset");
    const char *key = gobj_read_str_attr(gobj, "key");
    const char *notkey = gobj_read_str_attr(gobj, "notkey");

    priv->match_cond = json_object();

    if(!empty_string(from_t)) {
        timestamp_t timestamp;
        int offset;
        if(all_numbers(from_t)) {
            timestamp = atoll(from_t);
        } else {
            parse_date_basic(from_t, &timestamp, &offset);
        }
        json_object_set_new(priv->match_cond, "from_t", json_integer(timestamp));
    }
    if(!empty_string(to_t)) {
        timestamp_t timestamp;
        int offset;
        if(all_numbers(to_t)) {
            timestamp = atoll(to_t);
        } else {
            parse_date_basic(to_t, &timestamp, &offset);
        }
        json_object_set_new(priv->match_cond, "to_t", json_integer(timestamp));
    }
    if(!empty_string(from_rowid)) {
        json_object_set_new(priv->match_cond, "from_rowid", json_integer(atoll(from_rowid)));
    }
    if(!empty_string(to_rowid)) {
        json_object_set_new(priv->match_cond, "to_rowid", json_integer(atoll(to_rowid)));
    }
    if(!empty_string(user_flag_mask_set)) {
        json_object_set_new(priv->match_cond, "user_flag_mask_set", json_integer(atol(user_flag_mask_set)));
    }
    if(!empty_string(user_flag_mask_notset)) {
        json_object_set_new(priv->match_cond, "user_flag_mask_notset", json_integer(atol(user_flag_mask_notset)));
    }
    if(!empty_string(system_flag_mask_set)) {
        json_object_set_new(priv->match_cond, "system_flag_mask_set", json_integer(atol(system_flag_mask_set)));
    }
    if(!empty_string(system_flag_mask_notset)) {
        json_object_set_new(priv->match_cond, "system_flag_mask_notset", json_integer(atol(system_flag_mask_notset)));
    }
    if(!empty_string(key)) {
        json_object_set_new(priv->match_cond, "key", json_string(key));
    } else {
        /* No key given: load all keys (rkey "" is equivalent to ".*"). */
        json_object_set_new(priv->match_cond, "rkey", json_string(""));
    }
    if(!empty_string(notkey)) {
        json_object_set_new(priv->match_cond, "notkey", json_string(notkey));
    }

    /*
     *  tranger2_open_list requires the load callback in match_cond; it is
     *  invoked synchronously per matching record during the load below,
     *  collecting frames into priv->jn_frames.
     */
    json_object_set_new(
        priv->match_cond,
        "load_record_callback",
        json_integer((json_int_t)(uintptr_t)collect_record_cb)
    );

    /*----------------------------------*
     *  Startup TimeRanger2 + load frames
     *----------------------------------*/
    priv->jn_frames = json_array();
    priv->frame_idx = 0;

    json_t *jn_tranger = json_pack("{s:s, s:s, s:b}",
        "path", path,
        "database", database,
        "master", 0
    );
    priv->tranger = tranger2_startup(gobj, jn_tranger, 0);
    if(!priv->tranger) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "tranger2_startup() FAILED",
            "path",         "%s", path,
            "database",     "%s", database,
            NULL
        );
        if(agent_client) {
            gobj_send_event(agent_client, EV_PAUSE_YUNO, 0, gobj);
            return -1;
        }
        fprintf(stderr, "Can't startup tranger %s/%s\n", path, database);
        exit(-1);
    }

    if(!tranger2_open_topic(priv->tranger, topic, TRUE)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "tranger2_open_topic() FAILED",
            "topic",        "%s", topic,
            NULL
        );
        if(agent_client) {
            gobj_send_event(agent_client, EV_PAUSE_YUNO, 0, gobj);
            return -1;
        }
        fprintf(stderr, "Can't open topic %s\n", topic);
        exit(-1);
    }

    priv->jn_list = tranger2_open_list(
        priv->tranger,
        topic,
        json_incref(priv->match_cond),  // owned by open_list; priv keeps its ref
        0,                              // extra
        "emu_replay",                   // rt_id
        TRUE,                           // rt_by_disk
        gobj_name(gobj)                 // creator
    );

    if(gobj_trace_level(gobj) & TRACE_INFO) {
        gobj_trace_msg(gobj, "emu_device loaded %d frames from %s",
            (int)json_array_size(priv->jn_frames), topic);
    }

    /*
     *  Start the output side. C_TCP is gcflag_manual_start, so gobj_start_tree
     *  skips it — start the transport explicitly (as every C_TCP client does,
     *  cf. c_prot_http_cl / c_websocket). When it connects it publishes
     *  EV_ON_OPEN, which sends the leading frame and kicks the window/interval
     *  emission.
     */
    gobj_start_tree(priv->gobj_output_side);
    if(priv->gobj_output_tcp) {
        gobj_start(priv->gobj_output_tcp);
    }

    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_find_service("agent_client", FALSE)) {
        gobj_save_persistent_attrs(gobj, 0);
    }

    clear_timeout(priv->timer_interval);

    if(priv->gobj_output_side) {
        gobj_stop_tree(priv->gobj_output_side);
    }

    if(priv->tranger && priv->jn_list) {
        tranger2_close_list(priv->tranger, priv->jn_list);
        priv->jn_list = 0;
    }
    EXEC_AND_RESET(tranger2_shutdown, priv->tranger);

    JSON_DECREF(priv->match_cond);
    JSON_DECREF(priv->jn_frames);
    priv->frame_idx = 0;

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
PRIVATE json_t *cmd_write_interval(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int interval_ = (int)kw_get_int(gobj, kw, "interval", 1000, KW_WILD_NUMBER);
    gobj_write_integer_attr(gobj, "interval", interval_);
    gobj_save_persistent_attrs(gobj, 0);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("interval: %d", priv->interval),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_write_window(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int window_ = (int)kw_get_int(gobj, kw, "window", 1, KW_WILD_NUMBER);
    gobj_write_integer_attr(gobj, "window", window_);
    gobj_save_persistent_attrs(gobj, 0);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("window: %d", priv->window),
        0,
        0,
        kw  // owned
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_read_parameters(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("window: %d\ninterval: %d\nframes sent: %"PRIu64"\nframes loaded: %d",
            priv->window,
            priv->interval,
            (uint64_t)priv->frame_idx,
            (int)json_array_size(priv->jn_frames)
        ),
        0,
        0,
        kw  // owned
    );
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Build the __output_side__ stack: a raw-TCP client connecting to `url`.
 *  Mirrors sgateway's output side; EV_SEND_MESSAGE to the gate routes to it.
 ***************************************************************************/
PRIVATE int create_output_side(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_output_side = gobj_create_service(
        "__output_side__",
        C_IOGATE,
        0,
        gobj_yuno()
    );

    hgobj gobj_channel = gobj_create("output", C_CHANNEL, 0, priv->gobj_output_side);
    hgobj gobj_prot_raw = gobj_create("output", C_PROT_RAW, 0, gobj_channel);
    gobj_set_bottom_gobj(gobj_channel, gobj_prot_raw);

    json_t *kw_connex = json_pack("{s:i, s:s}",
        "timeout_between_connections", 2000,
        "url", gobj_read_str_attr(gobj, "url")    // C_TCP client attr is "url" (singular)
    );
    hgobj gobj_connex = gobj_create_pure_child("output", C_TCP, kw_connex, gobj_prot_raw);
    gobj_set_bottom_gobj(gobj_prot_raw, gobj_connex);
    priv->gobj_output_tcp = gobj_connex;

    gobj_subscribe_event(priv->gobj_output_side, NULL, 0, gobj);

    return 0;
}

/***************************************************************************
 *  Load callback: collect each matching record into priv->jn_frames.
 *  Called synchronously per record during tranger2_open_list().
 ***************************************************************************/
PRIVATE int collect_record_cb(
    json_t *tranger,
    json_t *topic,
    const char *key,
    json_t *list,
    json_int_t rowid,
    md2_record_ex_t *md_record,
    json_t *jn_record   // must be owned
)
{
    hgobj gobj = (hgobj)(size_t)kw_get_int(0, tranger, "gobj", 0, KW_REQUIRED);
    if(!gobj) {
        JSON_DECREF(jn_record)
        return -1;  // Error already logged
    }
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_array_append_new(priv->jn_frames, jn_record);  // takes ownership
    return 0;
}

/***************************************************************************
 *  Base64-decode a frame and send it out through __output_side__.
 ***************************************************************************/
PRIVATE int send_frame_b64(hgobj gobj, const char *frame64)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = gbuffer_base64_to_binary(frame64, strlen(frame64));
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "gbuffer_base64_to_binary() FAILED",
            NULL
        );
        return -1;
    }

    if(gobj_trace_level(gobj) & TRACE_INFO) {
        gobj_trace_msg(gobj, "emu_device sending %d bytes", (int)gbuffer_leftbytes(gbuf));
    }

    json_t *kw_tx = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    gobj_send_event(priv->gobj_output_side, EV_SEND_MESSAGE, kw_tx, gobj);
    priv->txMsgs++;

    return 0;
}

/***************************************************************************
 *  Send up to `window` frames from the loaded list, advancing frame_idx.
 ***************************************************************************/
PRIVATE int send_window(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t total = json_array_size(priv->jn_frames);

    for(uint32_t i=0; i<priv->window; i++) {
        if((size_t)priv->frame_idx >= total) {
            break;
        }
        json_t *rec = json_array_get(priv->jn_frames, (size_t)priv->frame_idx);
        priv->frame_idx++;

        const char *frame64 = kw_get_str(gobj, rec, "frame64", "", 0);
        if(empty_string(frame64)) {
            continue;
        }
        send_frame_b64(gobj, frame64);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL frames_exhausted(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    return ((size_t)priv->frame_idx >= json_array_size(priv->jn_frames))?TRUE:FALSE;
}

/***************************************************************************
 *  All frames replayed: stop the periodic emission. In standalone (CLI)
 *  mode the process has nothing left to do, so it exits.
 ***************************************************************************/
PRIVATE int finish_replay(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer_interval);

    if(gobj_trace_level(gobj) & TRACE_INFO) {
        gobj_trace_msg(gobj, "emu_device finished, sent %"PRIu64" frames", priv->txMsgs);
    }

    if(!gobj_find_service("agent_client", FALSE)) {
        exit(0);
    }

    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  Output side connected: send the leading frame (if any) then start
 *  emitting `window` frames per `interval`.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_INFO) {
        gobj_trace_msg(gobj, "emu_device connected");
    }

    if(gobj_is_playing(gobj)) {
        const char *leading = gobj_read_str_attr(gobj, "leading");
        if(!empty_string(leading)) {
            send_frame_b64(gobj, leading);
        }

        send_window(gobj);

        if(frames_exhausted(gobj)) {
            finish_replay(gobj);
        } else {
            set_timeout_periodic(priv->timer_interval, priv->interval);
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Output side disconnected.
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_INFO) {
        gobj_trace_msg(gobj, "emu_device disconnected");
    }
    clear_timeout(priv->timer_interval);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Data coming back from the peer (counted, not used).
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->rxMsgs++;

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Periodic tick: send the next window of frames.
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    send_window(gobj);

    if(frames_exhausted(gobj)) {
        finish_replay(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Child stopped
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
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
    .mt_writing     = mt_writing
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_EMU_DEVICE);

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
    ev_action_t st_idle[] = {
        {EV_ON_MESSAGE,         ac_on_message,  0},
        {EV_ON_OPEN,            ac_on_open,     0},
        {EV_ON_CLOSE,           ac_on_close,    0},
        {EV_TIMEOUT_PERIODIC,   ac_timeout,     0},
        {EV_STOPPED,            ac_stopped,     0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE, st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_ON_MESSAGE,         0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
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
        0,  // lmt
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // acl
        command_table,
        s_user_trace_level,
        0   // gcflag
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_emu_device(void)
{
    return create_gclass(C_EMU_DEVICE);
}
