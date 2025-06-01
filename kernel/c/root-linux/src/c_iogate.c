/***********************************************************************
 *          C_IOGATE.C
 *          Iogate GClass.
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2024, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include <inttypes.h>

#include <command_parser.h>
#include "msg_ievent.h"
#include "c_timer.h"
#include "c_channel.h"
#include "c_iogate.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE json_t *channels_opened(hgobj gobj, const char *lmethod, json_t *kw, hgobj src);

/***************************************************************************
 *              Resources
 ***************************************************************************/

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE json_t *cmd_view_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_enable_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_disable_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_trace_on_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_trace_off_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_reset_stats_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t pm_channel[] = {
/*-PM----type-----------name------------flag----------------default-----description---------- */
SDATAPM (DTP_STRING,    "channel_name", 0,                  0,          "Channel name."),
SDATAPM (DTP_BOOLEAN,   "opened",    0,                     0,          "Channel opened"),
SDATA_END()
};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name------------------------alias---items-----------json_fn-----------------description---------- */
SDATACM (DTP_SCHEMA,    "help",                     a_help, pm_help,        cmd_help,               "Available commands or help about a command."),
SDATACM (DTP_SCHEMA,    "",                         0,      0,              0,                      "\nDeploy\n-----------"),
SDATACM (DTP_SCHEMA,    "",                         0,      0,              0,                      "\nOperation\n-----------"),
/*-CMD2---type----------name--------------------flag------------ali-items---------------json_fn-------------description--*/
SDATACM2(DTP_SCHEMA,    "view-channels",        SDF_AUTHZ_X,    0,  pm_channel,     cmd_view_channels,      "View channels."),
SDATACM2(DTP_SCHEMA,    "enable-channel",       SDF_AUTHZ_X,    0,  pm_channel,     cmd_enable_channels,    "Enable channel."),
SDATACM2(DTP_SCHEMA,    "disable-channel",      SDF_AUTHZ_X,    0,  pm_channel,     cmd_disable_channels,   "Disable channel."),
SDATACM2(DTP_SCHEMA,    "trace-on-channel",     SDF_AUTHZ_X,    0,  pm_channel,     cmd_trace_on_channels, "Trace on channel."),
SDATACM2(DTP_SCHEMA,    "trace-off-channel",    SDF_AUTHZ_X,    0,  pm_channel,     cmd_trace_off_channels,"Trace off channel."),
SDATACM2(DTP_SCHEMA,    "reset-stats-channel",  SDF_AUTHZ_X,    0,  pm_channel,     cmd_reset_stats_channels,"Reset stats of channel."),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name----------------flag----------------default-----description---------- */
SDATA (DTP_BOOLEAN,     "persistent_channels",SDF_RD,           0,          "legacy, TODO remove"),
SDATA (DTP_INTEGER,     "send_type",        SDF_RD,             0,          "Send type: 0 one dst, 1 all destinations"),
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",     "Timeout"),
SDATA (DTP_INTEGER,     "txMsgs",           SDF_RD|SDF_RSTATS,  0,          "Messages transmitted"),
SDATA (DTP_INTEGER,     "rxMsgs",           SDF_RD|SDF_RSTATS,  0,          "Messages received"),
SDATA (DTP_INTEGER,     "txMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (DTP_INTEGER,     "rxMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (DTP_INTEGER,     "maxtxMsgsec",      SDF_RD|SDF_RSTATS,  0,          "Max Messages by second"),
SDATA (DTP_INTEGER,     "maxrxMsgsec",      SDF_RD|SDF_RSTATS,  0,          "Max Messages by second"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA (DTP_POINTER,     "subscriber",       0,                  0,          "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_CONNECTION    = 0x0001,
    TRACE_MESSAGES      = 0x0002,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"connection",      "Trace connections of iogates"},
{"messages",        "Trace messages of iogates"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj cur_channel;
    hgobj timer;
    int32_t timeout;

    hgobj subscriber;
    send_type_t send_type;
    json_int_t txMsgs;
    json_int_t rxMsgs;
    json_int_t last_txMsgs;
    json_int_t last_rxMsgs;
    uint64_t last_ms;
} PRIVATE_DATA;

PRIVATE hgclass __gclass__ = 0;




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
    }

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,                   gobj_read_integer_attr)
    SET_PRIV(send_type,                 gobj_read_integer_attr)
    SET_PRIV(subscriber,                gobj_read_pointer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,                 gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(subscriber,            gobj_read_pointer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gobj_start(priv->timer);
    set_timeout_periodic(priv->timer, priv->timeout);

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
    gobj_stop_children(gobj);
    return 0;
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
    }
    return v;
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
 *  CLI usage
 ***************************************************************************/
#define FORMATT_VIEW_CHANNELS "%-22s %-6s %-12s %-12s %-8s %-8s %-8s %-8s %-20s %-20s"
#define FORMATD_VIEW_CHANNELS "%-22s %6d %12" JSON_INTEGER_FORMAT " %12" JSON_INTEGER_FORMAT " %8" JSON_INTEGER_FORMAT " %8" JSON_INTEGER_FORMAT " %8d %8d %20s %20s"
PRIVATE int add_child_to_data(hgobj gobj, json_t *jn_data, hgobj child)
{
    const char *channel_name = gobj_name(child);
    BOOL disabled = gobj_read_bool_attr(child, "__disabled__");
    BOOL opened = gobj_read_bool_attr(child, "opened");
    BOOL traced = gobj_read_integer_attr(child, "__trace_level__");
    const char *sockname = "";
    const char *peername = "";
    json_int_t txMsgs = 0;
    json_int_t rxMsgs = 0;
    json_int_t txMsgsec = 0;
    json_int_t rxMsgsec = 0;
    if(gobj_has_bottom_attr(child, "sockname")) {
        // Only when connected has this attrs.
        sockname = gobj_read_str_attr(child, "sockname");
        if(!sockname) {
            sockname = "";
        }
        peername = gobj_read_str_attr(child, "peername");
        if(!peername) {
            peername = "";
        }
        txMsgs = gobj_read_integer_attr(child, "txMsgs");
        rxMsgs = gobj_read_integer_attr(child, "rxMsgs");
        txMsgsec = gobj_read_integer_attr(child, "txMsgsec");
        rxMsgsec = gobj_read_integer_attr(child, "rxMsgsec");
    }
    return json_array_append_new(
        jn_data,
        json_sprintf(FORMATD_VIEW_CHANNELS,
            channel_name,
            opened,
            txMsgs,
            rxMsgs,
            txMsgsec,
            rxMsgsec,
            traced,
            disabled,
            sockname,
            peername
        )
    );
}
PRIVATE json_t *cmd_view_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_data = json_array();
    json_array_append_new(
        jn_data,
        json_sprintf(FORMATT_VIEW_CHANNELS,
            "  Channel name",
            "Opened",
            "Tx Msgs",
            "Rx Msgs",
            "Tx m/s",
            "Rx m/s",
            "Traced",
            "Disabled",
            "Sockname",
            "Peername"
        )
    );
    json_array_append_new(
        jn_data,
        json_sprintf(FORMATT_VIEW_CHANNELS,
            " =====================",
            "======",
            "============",
            "============",
            "========",
            "========",
            "========",
            "========",
            "====================",
            "===================="
        )
    );

    const char *channel = kw_get_str(gobj, kw, "channel_name", 0, 0);
    BOOL opened = kw_get_bool(gobj, kw, "opened", 0, KW_WILD_NUMBER);
    if(empty_string(channel)) {
        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", C_CHANNEL
        );
        hgobj child = gobj_first_child(gobj);
        while(child) {
            if(gobj_match_gobj(child, json_incref(jn_filter))) {
                if(opened) {
                    if(gobj_read_bool_attr(child, "opened")) {
                        add_child_to_data(gobj, jn_data, child);
                    }
                } else {
                    add_child_to_data(gobj, jn_data, child);
                }
            }
            child = gobj_next_child(child);
        }
        JSON_DECREF(jn_filter)

    } else {
        regex_t _re_name;
        if(regcomp(&_re_name, channel, REG_EXTENDED | REG_NOSUB)!=0) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("regcomp() failed"),
                0,
                jn_data, // owned
                kw  // owned
            );
        }

        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", C_CHANNEL
        );
        hgobj child = gobj_first_child(gobj);
        while(child) {
            if(gobj_match_gobj(child, json_incref(jn_filter))) {
                const char *name = gobj_name(child);
                if(regexec(&_re_name, name, 0, 0, 0)!=0) {
                    continue;
                }
                if(opened) {
                    if(gobj_read_bool_attr(child, "opened")) {
                        add_child_to_data(gobj, jn_data, child);
                    }
                } else {
                    add_child_to_data(gobj, jn_data, child);
                }
            }
            child = gobj_next_child(child);
        }
        JSON_DECREF(jn_filter)
        regfree(&_re_name);
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("%d channels", (int)json_array_size(jn_data) - 2),
        0,
        jn_data, // owned
        kw  // owned
    );
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE json_t *cmd_enable_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *channel = kw_get_str(gobj, kw, "channel_name", 0, 0);
    if(empty_string(channel)) {
        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", C_CHANNEL
        );
        hgobj child = gobj_first_child(gobj);
        while(child) {
            if(gobj_match_gobj(child, json_incref(jn_filter))) {
                if(gobj_is_disabled(child)) {
                    gobj_enable(child);
                }
            }
            child = gobj_next_child(child);
        }
        JSON_DECREF(jn_filter)

    } else {
        regex_t _re_name;
        if(regcomp(&_re_name, channel, REG_EXTENDED | REG_NOSUB)!=0) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("regcomp() failed"),
                0,
                0, // owned
                kw  // owned
            );
        }

        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", C_CHANNEL
        );
        hgobj child = gobj_first_child(gobj);
        while(child) {
            if(gobj_match_gobj(child, json_incref(jn_filter))) {
                const char *name = gobj_name(child);
                if(regexec(&_re_name, name, 0, 0, 0)!=0) {
                    continue;
                }
                if(gobj_is_disabled(child)) {
                    gobj_enable(child);
                }
            }
            child = gobj_next_child(child);
        }
        JSON_DECREF(jn_filter)
        regfree(&_re_name);
    }

    return cmd_view_channels(gobj, cmd, kw, src);
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE json_t *cmd_disable_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *channel = kw_get_str(gobj, kw, "channel_name", 0, 0);
    if(empty_string(channel)) {
        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", C_CHANNEL
        );
        hgobj child = gobj_first_child(gobj);
        while(child) {
            if(gobj_match_gobj(child, json_incref(jn_filter))) {
                if(!gobj_is_disabled(child)) {
                    gobj_disable(child);
                }
            }
            child = gobj_next_child(child);
        }
        JSON_DECREF(jn_filter)

    } else {
        regex_t _re_name;
        if(regcomp(&_re_name, channel, REG_EXTENDED | REG_NOSUB)!=0) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("regcomp() failed"),
                0,
                0, // owned
                kw  // owned
            );
        }

        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", C_CHANNEL
        );
        hgobj child = gobj_first_child(gobj);
        while(child) {
            if(gobj_match_gobj(child, json_incref(jn_filter))) {
                const char *name = gobj_name(child);
                if(regexec(&_re_name, name, 0, 0, 0)!=0) {
                    continue;
                }
                if(!gobj_is_disabled(child)) {
                    gobj_disable(child);
                }
            }
            child = gobj_next_child(child);
        }
        JSON_DECREF(jn_filter)
        regfree(&_re_name);
    }

    return cmd_view_channels(gobj, cmd, kw, src);
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE json_t *cmd_trace_on_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *channel = kw_get_str(gobj, kw, "channel_name", 0, 0);
    if(empty_string(channel)) {
        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", C_CHANNEL
        );
        hgobj child = gobj_first_child(gobj);
        while(child) {
            if(gobj_match_gobj(child, json_incref(jn_filter))) {
                gobj_set_gobj_trace(child, "", true, 0);
            }
            child = gobj_next_child(child);
        }
        JSON_DECREF(jn_filter)

    } else {
        regex_t _re_name;
        if(regcomp(&_re_name, channel, REG_EXTENDED | REG_NOSUB)!=0) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("regcomp() failed"),
                0,
                0, // owned
                kw  // owned
            );
        }

        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", C_CHANNEL
        );
        hgobj child = gobj_first_child(gobj);
        while(child) {
            if(gobj_match_gobj(child, json_incref(jn_filter))) {
                const char *name = gobj_name(child);
                if(regexec(&_re_name, name, 0, 0, 0)!=0) {
                    continue;
                }
                gobj_set_gobj_trace(child, "", true, 0);
            }
            child = gobj_next_child(child);
        }
        JSON_DECREF(jn_filter)
        regfree(&_re_name);
    }

    return cmd_view_channels(gobj, cmd, kw, src);
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE json_t *cmd_trace_off_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *channel = kw_get_str(gobj, kw, "channel_name", 0, 0);
    if(empty_string(channel)) {
        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", C_CHANNEL
        );
        hgobj child = gobj_first_child(gobj);
        while(child) {
            if(gobj_match_gobj(child, json_incref(jn_filter))) {
                gobj_set_gobj_trace(child, "", false, 0);
            }
            child = gobj_next_child(child);
        }
        JSON_DECREF(jn_filter)

    } else {
        regex_t _re_name;
        if(regcomp(&_re_name, channel, REG_EXTENDED | REG_NOSUB)!=0) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("regcomp() failed"),
                0,
                0, // owned
                kw  // owned
            );
        }

        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", C_CHANNEL
        );
        hgobj child = gobj_first_child(gobj);
        while(child) {
            if(gobj_match_gobj(child, json_incref(jn_filter))) {
                const char *name = gobj_name(child);
                if(regexec(&_re_name, name, 0, 0, 0)!=0) {
                    continue;
                }
                gobj_set_gobj_trace(child, "", false, 0);
            }
            child = gobj_next_child(child);
        }
        JSON_DECREF(jn_filter)
        regfree(&_re_name);
    }

    return cmd_view_channels(gobj, cmd, kw, src);
}

/***************************************************************************
 *  command
 ***************************************************************************/
PRIVATE json_t *cmd_reset_stats_channels(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *channel = kw_get_str(gobj, kw, "channel_name", 0, 0);
    if(empty_string(channel)) {
        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", C_CHANNEL
        );
        hgobj child = gobj_first_child(gobj);
        while(child) {
            if(gobj_match_gobj(child, json_incref(jn_filter))) {
                json_t *jn_stats = gobj_stats(child, "__reset__", 0, gobj);
                JSON_DECREF(jn_stats)
            }
            child = gobj_next_child(child);
        }
        JSON_DECREF(jn_filter)

    } else {
        regex_t _re_name;
        if(regcomp(&_re_name, channel, REG_EXTENDED | REG_NOSUB)!=0) {
            return msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("regcomp() failed"),
                0,
                0, // owned
                kw  // owned
            );
        }

        json_t *jn_filter = json_pack("{s:s}",
            "__gclass_name__", C_CHANNEL
        );
        hgobj child = gobj_first_child(gobj);
        while(child) {
            if(gobj_match_gobj(child, json_incref(jn_filter))) {
                const char *name = gobj_name(child);
                if(regexec(&_re_name, name, 0, 0, 0)!=0) {
                    continue;
                }
                json_t *jn_stats = gobj_stats(child, "__reset__", 0, gobj);
                JSON_DECREF(jn_stats)
            }
            child = gobj_next_child(child);
        }
        JSON_DECREF(jn_filter)
        regfree(&_re_name);
    }

    return cmd_view_channels(gobj, cmd, kw, src);
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *  Filter channels
 ***************************************************************************/
PRIVATE hgobj get_next_destination(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t count = gobj_child_size(gobj);

    json_t *jn_filter = json_pack("{s:s, s:b, s:b}",
        "__gclass_name__", C_CHANNEL,
        "opened", 1,
        "__disabled__", 0
    );

    hgobj child = priv->cur_channel;
    if(!child) {
        child = gobj_first_child(gobj);
    }

    size_t i = 0;
    while(i<count) {
        if(gobj_match_gobj(child, json_incref(jn_filter))) {
            priv->cur_channel = child;
            JSON_DECREF(jn_filter)
            return priv->cur_channel;
        }
        child = gobj_next_child(child);
        if(!child) {
            child = gobj_first_child(gobj);
        }
        i++;
    }

    JSON_DECREF(jn_filter)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_one_rotate(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *channel = kw_get_str(gobj, kw, "__temp__`channel", "", 0);
    hgobj channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, 0);
    if(!channel_gobj) {
        if(!empty_string(channel)) {
            channel_gobj = gobj_child_by_name(gobj, channel);
        } else {
            channel_gobj = get_next_destination(gobj);
        }
        if(!channel_gobj) {
            static int repeated = 0;

            if(repeated%1000 == 0) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "No channel FOUND to send",
                    "channel",      "%s", channel?channel:"???",
                    "event",        "%s", event,
                    "repeated",     "%d", repeated*1000,
                    NULL
                );
            }
            KW_DECREF(kw)
            return -1;
        }
    }

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
        if(gbuf) {
            gobj_trace_dump_gbuf(
                gobj,
                gbuf, // not own
                "gbuffer_t %s ==> %s",
                gobj_short_name(gobj),
                gobj_short_name(channel_gobj)
            );
        } else {
            gobj_trace_json(
                gobj,
                kw, // not own,
                "KW %s ==> %s",
                gobj_short_name(gobj),
                gobj_short_name(channel_gobj)
            );
        }
    }

    int ret = gobj_send_event(channel_gobj, event, kw, gobj); // reuse kw
    if(ret == 0) {
        priv->txMsgs++;
    }

    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int send_all(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int some = 0;

    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", C_CHANNEL
    );
    hgobj child = gobj_first_child(gobj);
    while(child) {
        if(gobj_match_gobj(child, json_incref(jn_filter))) {
            if(gobj_read_bool_attr(child, "opened")) {
                if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
                    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
                    if(gbuf) {
                        gobj_trace_dump_gbuf(
                            gobj,
                            gbuf, // not own
                            "gbuffer_t %s ==> %s",
                            gobj_short_name(gobj),
                            gobj_short_name(child)
                        );
                    } else {
                        gobj_trace_json(
                            gobj,
                            kw, // not own,
                            "KW %s ==> %s",
                            gobj_short_name(gobj),
                            gobj_short_name(child)
                        );
                    }
                }

                int ret = gobj_send_event(child, event, json_incref(kw), gobj); // reuse kw
                if(ret == 0) {
                    some++;
                    priv->txMsgs++;
                }
            }
        }
        child = gobj_next_child(child);
    }
    JSON_DECREF(jn_filter)

    if(!some) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "No channel FOUND to send",
            "event",        "%s", event,
            NULL
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  How many channel opened?
 ***************************************************************************/
PRIVATE json_t *channels_opened(hgobj gobj, const char *lmethod, json_t *kw, hgobj src)
{
    int opened = 0;

    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", C_CHANNEL
    );
    hgobj child = gobj_first_child(gobj);
    while(child) {
        if(gobj_match_gobj(child, json_incref(jn_filter))) {
            if(gobj_read_bool_attr(child, "opened")) {
                opened++;
            }
        }
        child = gobj_next_child(child);
    }
    JSON_DECREF(jn_filter)

    return json_pack("{s:i}",
        "channels_opened", opened
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    if(gobj_trace_level(gobj) & TRACE_CONNECTION) {
        gobj_log_info(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_OPEN_CLOSE,
            "msg",              "%s", "ON_OPEN",
            "channel",          "%s", gobj_short_name(src),
            NULL
        );
    }

    if(!kw) {
        kw = json_object();
    }
    kw_set_subdict_value(
        gobj,
        kw,
        "__temp__",
        "channel",
        json_string(gobj_name(src))
    );
    kw_set_subdict_value(
        gobj,
        kw,
        "__temp__",
        "channel_gobj",
        json_integer((json_int_t)(size_t)src)
    );

    /*
     *  SERVICE subscription model
     */
    if(gobj_is_pure_child(gobj)) {
        return gobj_send_event(gobj_parent(gobj), event, kw, gobj); // reuse kw
    } else {
        return gobj_publish_event(gobj, event, kw); // reuse kw
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    if(gobj_trace_level(gobj) & TRACE_CONNECTION) {
        gobj_log_info(gobj, 0,
            "function",         "%s", __FUNCTION__,
            "msgset",           "%s", MSGSET_OPEN_CLOSE,
            "msg",              "%s", "ON_CLOSE",
            "channel",          "%s", gobj_short_name(src),
            NULL
        );
    }

    if(!kw) {
        kw = json_object();
    }
    kw_set_subdict_value(
        gobj,
        kw,
        "__temp__",
        "channel",
        json_string(gobj_name(src))
    );
    kw_set_subdict_value(
        gobj,
        kw,
        "__temp__",
        "channel_gobj",
        json_integer((json_int_t)(size_t)src)
    );

    /*
     *  SERVICE subscription model
     */
    if(gobj_is_pure_child(gobj)) {
        return gobj_send_event(gobj_parent(gobj), event, kw, gobj); // reuse kw
    } else {
        return gobj_publish_event(gobj, event, kw); // reuse kw
    }
}

/***************************************************************************
 *  Client/Server, receiving a message to publish.
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
        if(gbuf) {
            gobj_trace_dump_gbuf(
                gobj,
                gbuf, // not own
                "gbuffer_t %s <== %s",
                gobj_short_name(gobj),
                gobj_short_name(src)
            );
        } else {
            gobj_trace_json(
                gobj,
                kw, // not own
                "KW %s <== %s",
                gobj_short_name(gobj),
                gobj_short_name(src)
            );
        }
    }

    priv->rxMsgs++;

    if(!kw) {
        kw = json_object();
    }
    kw_set_subdict_value(
        gobj,
        kw,
        "__temp__",
        "channel",
        json_string(gobj_name(src))
    );
    kw_set_subdict_value(
        gobj,
        kw,
        "__temp__",
        "channel_gobj",
        json_integer((json_int_t)(size_t)src)
    );

    /*
     *  SERVICE subscription model
     */
    if(gobj_is_pure_child(gobj)) {
        return gobj_send_event(gobj_parent(gobj), event, kw, gobj); // reuse kw
    } else {
        return gobj_publish_event(gobj, event, kw); // reuse kw
    }
}

/***************************************************************************
 *  Server, receiving a IEvent inter-event to publish.
 ***************************************************************************/
PRIVATE int ac_iev_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *iev_event_ = kw_get_str(gobj, kw, "event", 0, KW_REQUIRED);

    /*
     *  Need to recover the good pointer, it lost at pass as json string
     *  (pass as integer is a solution, but by now, repeat
     */
    gobj_event_t iev_event = gclass_find_public_event(iev_event_, true);

    json_t *iev_kw = kw_get_dict(gobj, kw, "kw", 0, KW_REQUIRED|KW_EXTRACT);

    if(gobj_trace_level(gobj) & TRACE_MESSAGES) {
        gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
        if(gbuf) {
            gobj_trace_dump_gbuf(
                gobj,
                gbuf, // not own
                "gbuffer_t %s <== %s",
                gobj_short_name(gobj),
                gobj_short_name(src)
            );
        } else {
            gobj_trace_json(
                gobj,
                kw, // not own
                "KW %s <== %s",
                gobj_short_name(gobj),
                gobj_short_name(src)
            );
        }
    }

    priv->rxMsgs++;

    kw_set_subdict_value(
        gobj,
        iev_kw,
        "__temp__",
        "channel",
        json_string(gobj_name(src))
    );
    kw_set_subdict_value(
        gobj,
        iev_kw,
        "__temp__",
        "channel_gobj",
        json_integer((json_int_t)(size_t)src)
    );

    /*
     *  SERVICE subscription model
     */
    int ret;
    if(gobj_is_pure_child(gobj)) {
        ret = gobj_send_event(gobj_parent(gobj), iev_event, iev_kw, gobj);
    } else {
        ret = gobj_publish_event(gobj, iev_event, iev_kw);
    }

    KW_DECREF(kw)
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int send_type = (int)kw_get_int(gobj, kw, "__send_type__", priv->send_type, 0);

    switch(send_type) {
    case TYPE_SEND_ALL:
        send_all(gobj, event, kw_incref(kw), src);
        break;

    case TYPE_SEND_ONE_ROTATED:
    default:
        send_one_rotate(gobj, event, kw_incref(kw), src);
        break;
    }
    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_iev(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *iev_event = kw_get_str(gobj, kw, "event", "", KW_REQUIRED);
    json_t *iev_kw = kw_get_dict(gobj, kw, "kw", 0, KW_REQUIRED|KW_EXTRACT);
    json_t *__temp__ = kw_get_dict(gobj, kw, "__temp__", 0, 0);

    int send_type = (int)kw_get_int(gobj, kw, "__send_type__", priv->send_type, 0);

    switch(send_type) {
    case TYPE_SEND_ALL:
        send_all(gobj, iev_event, iev_kw, src);
        break;

    case TYPE_SEND_ONE_ROTATED:
    default:
        if(__temp__) {
            json_object_set(iev_kw, "__temp__", __temp__);
        }
        send_one_rotate(gobj, iev_event, iev_kw, src);
        break;
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    hgobj channel_gobj = gobj_bottom_gobj(gobj); // See firstly if it's a tube
    if(!channel_gobj) {
        channel_gobj = (hgobj)(size_t)kw_get_int(gobj, kw, "__temp__`channel_gobj", 0, 0);
    }
    if(!channel_gobj) {
        const char *channel = kw_get_str(gobj, kw, "__temp__`channel", "", 0);
        if(!empty_string(channel)) {
            channel_gobj = gobj_child_by_name(gobj, channel);
            if(!channel_gobj) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PARAMETER_ERROR,
                    "msg",          "%s", "channel NOT FOUND",
                    "channel",      "%s", channel,
                    "event",        "%s", event,
                    NULL
                );
                KW_DECREF(kw)
                return -1;
            }
        }
    }

    /*
     *  Only drop one
     */
    if(channel_gobj) {
        if(gobj_read_bool_attr(channel_gobj, "opened")) {
            gobj_send_event(channel_gobj, EV_DROP, 0, gobj);
        }
        KW_DECREF(kw)
        return 0;
    }

    /*
     *  Drop all
     */
    gobj_log_debug(gobj, 0,
        "gobj",         "%s", gobj_full_name(gobj),
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_INFO,
        "msg",          "%s", "Dropping all channels",
        NULL
    );

    json_t *jn_filter = json_pack("{s:s}",
        "__gclass_name__", C_CHANNEL
    );
    hgobj child = gobj_first_child(gobj);
    while(child) {
        if(gobj_match_gobj(child, json_incref(jn_filter))) {
            if(gobj_read_bool_attr(child, "opened")) {
                gobj_send_event(child, EV_DROP, 0, gobj);
            }
        }
        child = gobj_next_child(child);
    }
    JSON_DECREF(jn_filter)

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  This event comes from clisrv TCP gobjs
 *  that haven't found a free server link.
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    JSON_DECREF(kw)

    if(gobj_is_volatil(src)) {
        gobj_destroy(src);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Local stats
     */
    uint64_t ms = time_in_miliseconds_monotonic();
    if(!priv->last_ms) {
        priv->last_ms = ms;
    }
    json_int_t t = (json_int_t)(ms - priv->last_ms);
    if(t>0) {
        json_int_t txMsgsec = priv->txMsgs - priv->last_txMsgs;
        json_int_t rxMsgsec = priv->rxMsgs - priv->last_rxMsgs;

        txMsgsec *= 1000;
        rxMsgsec *= 1000;
        txMsgsec /= t;
        rxMsgsec /= t;

        json_int_t maxtxMsgsec = gobj_read_integer_attr(gobj, "maxtxMsgsec");
        json_int_t maxrxMsgsec = gobj_read_integer_attr(gobj, "maxrxMsgsec");
        if(txMsgsec > maxtxMsgsec) {
            gobj_write_integer_attr(gobj, "maxtxMsgsec", txMsgsec);
        }
        if(rxMsgsec > maxrxMsgsec) {
            gobj_write_integer_attr(gobj, "maxrxMsgsec", rxMsgsec);
        }

        gobj_write_integer_attr(gobj, "txMsgsec", txMsgsec);
        gobj_write_integer_attr(gobj, "rxMsgsec", rxMsgsec);
    }
    priv->last_ms = ms;
    priv->last_txMsgs = priv->txMsgs;
    priv->last_rxMsgs = priv->rxMsgs;

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
    .mt_create = mt_create,
    .mt_writing = mt_writing,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_reading = mt_reading,
};

/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {"channels_opened",         channels_opened,   0},
    {0, 0, 0}
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_IOGATE);

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
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
        {EV_ON_MESSAGE,         ac_on_message,      0},
        {EV_SEND_MESSAGE,       ac_send_message,    0},
        {EV_ON_IEV_MESSAGE,     ac_iev_message,     0},
        {EV_SEND_IEV,           ac_send_iev,        0},
        {EV_ON_COMMAND,         ac_on_message,      0},
        {EV_ON_ID,              ac_on_message,      0},
        {EV_ON_ID_NAK,          ac_on_message,      0},
        {EV_DROP,               ac_drop,            0},
        {EV_ON_OPEN,            ac_on_open,         0},
        {EV_ON_CLOSE,           ac_on_close,        0},
        {EV_TIMEOUT_PERIODIC,   ac_timeout,         0},
        {EV_STOPPED,            ac_stopped,         0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_SEND_MESSAGE,           0},
        {EV_SEND_IEV,               0},
        {EV_DROP,                   0},

        {EV_ON_MESSAGE,             EVF_OUTPUT_EVENT},
        {EV_ON_ID,                  EVF_OUTPUT_EVENT},
        {EV_ON_ID_NAK,              EVF_OUTPUT_EVENT},
        {EV_ON_OPEN,                EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,               EVF_OUTPUT_EVENT},

        {EV_ON_IEV_MESSAGE,         0},
        {EV_ON_COMMAND,             0},

        // internal
        {EV_STOPPED,                0},
        {EV_TIMEOUT_PERIODIC,       0},

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
        lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        command_table,  // command_table,
        s_user_trace_level,
        gcflag_no_check_output_events   // gcflag_t
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
PUBLIC int register_c_iogate(void)
{
    return create_gclass(C_IOGATE);
}
