/***********************************************************************
 *          C_SGATEWAY.C
 *          Sgateway GClass.
 *
 *          Simple Gateway
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>
#include <stdio.h>
#include "c_sgateway.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int create_input_side(hgobj gobj);
PRIVATE int create_output_side(hgobj gobj);
PRIVATE int open_input_side(hgobj gobj);
PRIVATE int close_input_side(hgobj gobj);
PRIVATE int open_output_side(hgobj gobj);
PRIVATE int close_output_side(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,        cmd_help,       "Command's help"),
SDATA_END()
};


/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag----------------default-----description---------- */
SDATA (DTP_STRING,      "input_url",        SDF_WR|SDF_PERSIST, 0,          "input_side url"),
SDATA (DTP_STRING,      "output_url",       SDF_WR|SDF_PERSIST, 0,          "output_side url"),
SDATA (DTP_INTEGER,     "txMsgs",           SDF_RD|SDF_RSTATS,  0,          "Messages transmitted"),
SDATA (DTP_INTEGER,     "rxMsgs",           SDF_RD|SDF_RSTATS,  0,          "Messages receiveds"),

SDATA (DTP_INTEGER,     "txMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (DTP_INTEGER,     "rxMsgsec",         SDF_RD|SDF_RSTATS,  0,          "Messages by second"),
SDATA (DTP_INTEGER,     "maxtxMsgsec",      SDF_WR|SDF_RSTATS,  0,          "Max Messages by second"),
SDATA (DTP_INTEGER,     "maxrxMsgsec",      SDF_WR|SDF_RSTATS,  0,          "Max Messages by second"),
SDATA (DTP_INTEGER,     "timeout",          SDF_RD,             "1000",     "Timeout"),
SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_MESSAGES = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"messages",        "Trace messages"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int32_t timeout;
    hgobj timer;

    hgobj gobj_output_side;
    hgobj gobj_input_side;
    BOOL input_side_opened;
    BOOL output_side_opened;

    uint64_t txMsgs;
    uint64_t rxMsgs;
    uint64_t txMsgsec;
    uint64_t rxMsgsec;

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

    priv->timer = gobj_create(gobj_name(gobj), C_TIMER, 0, gobj);

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

    gobj_start(priv->timer);

    hgobj agent_client = gobj_find_service("agent_client", FALSE);
    if(!agent_client) {
        gobj_play(gobj);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

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

    hgobj agent_client = gobj_find_service("agent_client", FALSE);

    const char *input_url = gobj_read_str_attr(gobj, "input_url");
    const char *output_url = gobj_read_str_attr(gobj, "output_url");

    if(empty_string(input_url)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "What yuno input url?",
            NULL
        );
        if(agent_client) {
            gobj_send_event(agent_client, EV_PAUSE_YUNO, 0, gobj);
        }
        return -1;
    }

    if(empty_string(output_url)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "What yuno output url?",
            NULL
        );
        if(agent_client) {
            gobj_send_event(agent_client, EV_PAUSE_YUNO, 0, gobj);
        }
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




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_input_side(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_input_side = gobj_create_service(
        "__input_side__",
        C_IOGATE,
        0,
        gobj_yuno()
    );

    json_t *kw1 = json_pack("{s:b, s:s, s:{s:s, s:{s:s, s:s, s:b, s:b}}}",
        "exitOnError", 0,
        "url", gobj_read_str_attr(gobj, "input_url"),
        "child_tree_filter",
            "op", "find",
            "kw",
                "__prefix_gobj_name__", "tcp-",
                "__gclass_name__", "Channel",
                "__disabled__", 0,
                "connected", 0
    );
    gobj_create(
        "server_port",
        C_TCP,
        kw1,
        priv->gobj_input_side
    );

    hgobj gobj_channel = gobj_create(
        "tcp-1",
        C_CHANNEL,
        0,
        priv->gobj_input_side
    );

    hgobj gobj_prot_raw = gobj_create(
        "tcp-1",
        C_PROT_RAW,
        0,
        gobj_channel
    );
    gobj_set_bottom_gobj(gobj_channel, gobj_prot_raw);

    gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);

    return 0;
}

/***************************************************************************
 *
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

    hgobj gobj_channel = gobj_create(
        "output",
        C_CHANNEL,
        0,
        priv->gobj_output_side
    );

    hgobj gobj_prot_raw = gobj_create(
        "output",
        C_PROT_RAW,
        0,
        gobj_channel
    );
    gobj_set_bottom_gobj(gobj_channel, gobj_prot_raw);

    json_t *kw = json_pack("{s:i, s:[s]}",
        "timeout_between_connections", 100,
        "urls", gobj_read_str_attr(gobj, "output_url")
    );

    hgobj gobj_connex = gobj_create_service(
        "output",
        C_TCP,
        kw,
        gobj_prot_raw
    );
    gobj_set_bottom_gobj(gobj_prot_raw, gobj_connex);

    gobj_subscribe_event(priv->gobj_output_side, 0, 0, gobj);

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

    gobj_stop_tree(priv->gobj_input_side);

    return 0;
}

/***************************************************************************
 *
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

    gobj_stop_tree(priv->gobj_output_side);

    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        // TODO mantén conexión abierta, tendría que encolar mensajes si cierro/abro gates
        priv->input_side_opened = TRUE;
        //open_output_side(gobj);
    } else if (src == priv->gobj_output_side) {
        priv->output_side_opened = TRUE;
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(src == priv->gobj_input_side) {
        priv->input_side_opened = TRUE;
        // TODO mantén conexión abierta, tendría que encolar mensajes si cierro/abro gates
        //close_output_side(gobj);
    } else if (src == priv->gobj_output_side) {
        priv->output_side_opened = TRUE;
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);

    priv->rxMsgs++;

    if(src == priv->gobj_input_side) {
        gbuffer_incref(gbuf);
        json_t *kw_send = json_pack("{s:I}",
            "gbuffer", (json_int_t)(uintptr_t)gbuf
        );
        gobj_send_event(priv->gobj_output_side, EV_SEND_MESSAGE, kw_send, gobj);

        priv->txMsgs++;

    } else if (src == priv->gobj_output_side) {
        gbuffer_incref(gbuf);
        json_t *kw_send = json_pack("{s:I}",
            "gbuffer", (json_int_t)(uintptr_t)gbuf
        );
        gobj_send_event(priv->gobj_input_side, EV_SEND_MESSAGE, kw_send, gobj);

        priv->txMsgs++;
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    uint64_t maxtxMsgsec = gobj_read_integer_attr(gobj, "maxtxMsgsec");
    uint64_t maxrxMsgsec = gobj_read_integer_attr(gobj, "maxrxMsgsec");
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
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
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
        {EV_ON_MESSAGE,       ac_on_message,     0},
        {EV_ON_OPEN,          ac_on_open,        0},
        {EV_ON_CLOSE,         ac_on_close,       0},
        {EV_TIMEOUT,          ac_timeout,        0},
        {EV_STOPPED,          0,                 0},
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
        {EV_ON_MESSAGE,       0},
        {EV_ON_OPEN,          0},
        {EV_ON_CLOSE,         0},
        {EV_TIMEOUT,          0},
        {EV_STOPPED,          0},
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
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // Authorization table
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
