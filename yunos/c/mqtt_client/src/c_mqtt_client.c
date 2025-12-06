/***********************************************************************
 *          C_MQTT_CLIENT.C
 *          Mqtt_client GClass.
 *
 *          Mqtt client
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>

#include <g_ev_console.h>
#include <c_editline.h>
#include "c_prot_mqtt2.h" // TODO change when moved to kernel
#include "c_mqtt_client.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define CTRL_A  {1}
#define CTRL_B  {2}
#define CTRL_C  {3}

#define CTRL_D  {4}
#define CTRL_E  {5}
#define CTRL_F  {6}
#define CTRL_H  {8}
#define BACKSPACE {0x7F}
#define TAB     {9}
#define CTRL_K  {11}
#define ENTER   {13}
#define CTRL_N  {14}
#define CTRL_P  {16}
#define CTRL_T  {20}
#define CTRL_U  {21}
#define CTRL_W  {23}
#define CTRL_Y  {25}
#define ESCAPE  {27}

#define MKEY_START              {0x1B, 0x5B, 0x48} // .[H
#define MKEY_END                {0x1B, 0x5B, 0x46} // .[F
#define MKEY_UP                 {0x1B, 0x5B, 0x41} // .[A
#define MKEY_DOWN               {0x1B, 0x5B, 0x42} // .[B
#define MKEY_LEFT               {0x1B, 0x5B, 0x44} // .[D
#define MKEY_RIGHT              {0x1B, 0x5B, 0x43} // .[C

#define MKEY_PREV_PAGE          {0x1B, 0x5B, 0x35, 0x7E} // .[5~
#define MKEY_NEXT_PAGE          {0x1B, 0x5B, 0x36, 0x7E} // .[6~
#define MKEY_INS                {0x1B, 0x5B, 0x32, 0x7E} // .[2~
#define MKEY_DEL                {0x1B, 0x5B, 0x33, 0x7E} // .[3~

#define MKEY_START2             {0x1B, 0x4F, 0x48} // .[H
#define MKEY_END2               {0x1B, 0x4F, 0x46} // .[F
#define MKEY_UP2                {0x1B, 0x4F, 0x41} // .OA
#define MKEY_DOWN2              {0x1B, 0x4F, 0x42} // .OB
#define MKEY_LEFT2              {0x1B, 0x4F, 0x44} // .OD
#define MKEY_RIGHT2             {0x1B, 0x4F, 0x43} // .OC

#define MKEY_ALT_START          {0x1B, 0x5B, 0x31, 0x3B, 0x33, 0x48} // .[1;3H
#define MKEY_ALT_PREV_PAGE      {0x1B, 0x5B, 0x35, 0x3B, 0x33, 0x7E} // .[5;3~
#define MKEY_ALT_NEXT_PAGE      {0x1B, 0x5B, 0x36, 0x3B, 0x33, 0x7E} // .[6;3~
#define MKEY_ALT_END            {0x1B, 0x5B, 0x31, 0x3B, 0x33, 0x46} // .[1;3F

#define MKEY_CTRL_START         {0x1B, 0x5B, 0x31, 0x3B, 0x35, 0x48} // .[1;5H
#define MKEY_CTRL_END           {0x1B, 0x5B, 0x31, 0x3B, 0x35, 0x46} // .[1;5F

#define MKEY_ALT_LEFT           {0x1B, 0x5B, 0x31, 0x3B, 0x33, 0x44} // .[1;3D
#define MKEY_CTRL_LEFT          {0x1B, 0x5B, 0x31, 0x3B, 0x35, 0x44} // .[1;5D
#define MKEY_ALT_RIGHT          {0x1B, 0x5B, 0x31, 0x3B, 0x33, 0x43} // .[1;3C
#define MKEY_CTRL_RIGHT         {0x1B, 0x5B, 0x31, 0x3B, 0x35, 0x43} // .[1;5C

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void try_to_stop_yevents(hgobj gobj);  // IDEMPOTENT
PRIVATE int yev_callback(yev_event_h yev_event);
PRIVATE int on_read_cb(hgobj gobj, gbuffer_t *gbuf);
PRIVATE int cmd_connect_broker(hgobj gobj);
PRIVATE int cmd_connect_mqtt(hgobj gobj);
PRIVATE int do_command(hgobj gobj, const char *command);
PRIVATE int clear_input_line(hgobj gobj);
PRIVATE char *get_history_file(char *bf, int bfsize);
PRIVATE int do_authenticate_task(hgobj gobj);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
#define MAX_KEYS 40
typedef struct keytable_s {
    const char *dst_gobj;
    const char *event;
    uint8_t keycode[8+1];
} keytable_t;
keytable_t keytable[MAX_KEYS] = {0};

PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_help_broker(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_publish(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_subscribe(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_unsubscribe(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "level=1: search in bottoms, level=2: search in all childs"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_publish[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "topic",        0,              "",         "Topic on which this message will be published"),
SDATAPM (DTP_STRING,    "payload",      0,              "",         "Data to send"),
SDATAPM (DTP_INTEGER,   "mid",          0,              "0",        "Message Id, set internally if it's 0"),
SDATAPM (DTP_INTEGER,   "qos",          0,              "0",        "Quality of Service to be used for the message (0,1,2)"),
SDATAPM (DTP_BOOLEAN,   "retain",       0,              "0",        "Set to true to make the message retained"),
SDATAPM (DTP_JSON,      "properties",   0,              0,          "Mqtt5 publish properties"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_subscribe[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_LIST,      "subs",         0,              "[]",       "List of subscription patterns"),
SDATAPM (DTP_INTEGER,   "mid",          0,              "0",        "Message Id, set internally if it's 0"),
SDATAPM (DTP_INTEGER,   "qos",          0,              "0",        "Quality of Service for this subscription (0,1,2)"),
SDATAPM (DTP_INTEGER,   "options",      0,              "0",        "Mqtt5 options to apply to this subscription, OR'd together. See mqtt5_sub_options"),
SDATAPM (DTP_JSON,      "properties",   0,              0,          "Mqtt5 suscribe properties"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_unsubscribe[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_LIST,      "subs",         0,              "[]",       "List of subscription patterns"),
SDATAPM (DTP_INTEGER,   "mid",          0,              "0",        "Message Id, set internally if it's 0"),
SDATAPM (DTP_JSON,      "properties",   0,              0,          "Mqtt5 unsuscribe properties"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name------------alias---items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "help",         a_help, pm_help,        cmd_help,       "Command's help"),
SDATACM (DTP_SCHEMA,    "help-broker",  0,      pm_help,        cmd_help_broker,"Command's help of broker"),
SDATACM (DTP_SCHEMA,    "publish",      0,      pm_publish,     cmd_publish,    "Publish"),
SDATACM (DTP_SCHEMA,    "subscribe",    0,      pm_subscribe,   cmd_subscribe,  "Subscribe"),
SDATACM (DTP_SCHEMA,    "unsubscribe",  0,      pm_unsubscribe, cmd_unsubscribe,"Unsubscribe"),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag--------default---------description---------- */
SDATA (DTP_BOOLEAN,     "print_with_metadata",0,        0,              "Print response with metadata."),
SDATA (DTP_BOOLEAN,     "verbose",          0,          "1",            "Verbose mode."),
SDATA (DTP_STRING,      "command",          0,          "",             "Command."),

SDATA (DTP_STRING,      "url_mqtt",         SDF_RD,     "mqtt://127.0.0.1:1810", "Url of mqtt port"),
SDATA (DTP_STRING,      "url_broker",       SDF_RD,     "ws://127.0.0.1:1800", "Url of broker port"),
SDATA (DTP_STRING,      "yuno_name",        0,          "",             "Yuno name"),
SDATA (DTP_STRING,      "yuno_role",        0,          "yuneta_agent", "Yuno role"),
SDATA (DTP_STRING,      "yuno_service",     0,          "agent",        "Yuno service"),
SDATA (DTP_STRING,      "auth_system",      0,          "",             "OpenID System(interactive jwt)"),
SDATA (DTP_STRING,      "auth_url",         0,          "",             "OpenID Endpoint(interactive jwt)"),
SDATA (DTP_STRING,      "azp",              0,          "",             "azp (OAuth2 Authorized Party)"),

SDATA (DTP_STRING,      "mqtt_client_id",   0,          "",             "MQTT Client id, used by mqtt client"),
SDATA (DTP_STRING,      "mqtt_protocol",    0,          "mqttv5",       "MQTT Protocol. Can be mqttv5, mqttv311 or mqttv31. Defaults to mqttv5."),

SDATA (DTP_STRING,      "user_id",          0,          "",             "MQTT Username or OAuth2 User Id (interactive jwt)"),
SDATA (DTP_STRING,      "user_passw",       0,          "",             "MQTT Password or OAuth2 User password (interactive jwt)"),

SDATA (DTP_STRING,      "jwt",              0,          "",             "Jwt"),
SDATA (DTP_STRING,      "display_mode",     0,          "table",        "Display mode: table or form"),
SDATA (DTP_STRING,      "editor",           0,          "vim",          "Editor"),
SDATA (DTP_POINTER,     "user_data",        0,          0,              "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,          0,              "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_KB        = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"trace-kb",            "Trace keyboard codes"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int32_t verbose;
    int32_t interactive;

    hgobj gobj_mqtt_connector;
    hgobj gobj_broker_connector;
    hgobj timer;
    hgobj gobj_editline;

    int tty_fd;
    yev_event_h yev_reading;

} PRIVATE_DATA;




                    /******************************
                     *      Framework Methods
                     ******************************/




/*****************************************************************
 *
 *****************************************************************/
PRIVATE void mt_create(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Input screen size
     */
    struct winsize winsz;
    if(ioctl(STDIN_FILENO, TIOCGWINSZ, &winsz)<0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "ioctl() FAILED",
            "errno",        "%d", errno,
            "serrno",       "%s", strerror(errno),
            NULL
        );
    }
    if(winsz.ws_row <= 0) {
        winsz.ws_row = 1;
    }
    if(winsz.ws_col <= 0) {
        winsz.ws_col = 80;
    }

    /*
     *  History filename
     */
    char history_file[PATH_MAX];
    get_history_file(history_file, sizeof(history_file));

    /*
     *  Editline
     */
    json_t *kw_editline = json_pack("{s:s, s:s, s:b, s:i, s:i}",
        "prompt", "mqtt> ",
        "history_file", history_file,
        "use_ncurses", 0,
        "cx", winsz.ws_col,
        "cy", winsz.ws_row
    );

    priv->gobj_editline = gobj_create_pure_child(
        gobj_name(gobj),
        C_EDITLINE,
        kw_editline,
        gobj
    );
    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(verbose,               gobj_read_bool_attr)

    priv->interactive = TRUE;
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    EXEC_AND_RESET(yev_destroy_event, priv->yev_reading)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);

    priv->tty_fd = tty_keyboard_init();
    if(priv->tty_fd < 0) {
        gobj_log_error(0, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "cannot open a tty window",
            NULL
        );
        gobj_set_exit_code(-1);
        gobj_shutdown();
        return -1;
    }

    /*-------------------------------*
     *      Setup reading event
     *-------------------------------*/
    json_int_t rx_buffer_size = 1024;
    if(!priv->yev_reading) {
        priv->yev_reading = yev_create_read_event(
            yuno_event_loop(),
            yev_callback,
            gobj,
            priv->tty_fd,
            gbuffer_create(rx_buffer_size, rx_buffer_size)
        );
    }

    if(priv->yev_reading) {
        yev_set_fd(priv->yev_reading, priv->tty_fd);

        if(!yev_get_gbuf(priv->yev_reading)) {
            yev_set_gbuffer(priv->yev_reading, gbuffer_create(rx_buffer_size, rx_buffer_size));
        } else {
            gbuffer_clear(yev_get_gbuf(priv->yev_reading));
        }

        yev_start_event(priv->yev_reading);
    }

    gobj_start(priv->gobj_editline);

    const char *auth_url = gobj_read_str_attr(gobj, "auth_url");
    const char *user_id = gobj_read_str_attr(gobj, "user_id");
    if(!empty_string(auth_url) && !empty_string(user_id)) {
        /*
         *  HACK if there are user_id and endpoint
         *  then try to authenticate
         *  else use default local connection
         */
        do_authenticate_task(gobj);
    } else {
        cmd_connect_broker(gobj);
        cmd_connect_mqtt(gobj);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    try_to_stop_yevents(gobj);
    clear_timeout(priv->timer);

    gobj_stop_tree(gobj);

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
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw_incref(kw));
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
PRIVATE json_t *cmd_help_broker(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->gobj_broker_connector) {
        if(gobj_read_bool_attr(priv->gobj_broker_connector, "opened")) {
            json_t *webix = gobj_command(priv->gobj_broker_connector, "help", kw_incref(kw), gobj);
            JSON_DECREF(webix)
        }
    }

    KW_DECREF(kw)
    return NULL;
}


/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_publish(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    const char *topic = kw_get_str(gobj, kw, "topic", "", 0);
    const char *payload = kw_get_str(gobj, kw, "payload", "", 0);
    int mid = (int)kw_get_int(gobj, kw, "mid", 0, 0);
    int qos = (int)kw_get_int(gobj, kw, "qos", 0, 0);
    BOOL retain = kw_get_bool(gobj, kw, "retain", 0, 0);
    json_t *properties = kw_get_dict(gobj, kw, "properties", 0, 0);

    size_t payloadlen = strlen(payload);
    gbuffer_t *gbuf_payload = gbuffer_create(payloadlen, payloadlen);
    if(payloadlen) {
        gbuffer_append_string(gbuf_payload, payload);
    }

    if(empty_string(topic)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What topic?"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Let mqtt protocol check all parameters
     */
    json_t *kw_publish = json_pack("{s:s, s:i, s:i, s:b, s:I}",
        "topic", topic,
        "mid", mid,
        "qos", qos,
        "retain", retain,
        "gbuffer", (json_int_t)(uintptr_t)gbuf_payload
    );
    if(properties && json_is_object(properties)) {
        json_object_set(kw_publish, "properties", properties);
    }

    gobj_send_event(gobj, EV_SEND_MQTT_PUBLISH, kw_publish, gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_subscribe(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *subs = kw_get_list(gobj, kw, "subs", 0, 0);
    int mid = (int)kw_get_int(gobj, kw, "mid", 0, 0);
    int qos = (int)kw_get_int(gobj, kw, "qos", 0, 0);
    int options = (int)kw_get_int(gobj, kw, "options", 0, 0);
    json_t *properties = kw_get_dict(gobj, kw, "properties", 0, 0);

    if(!json_is_array(subs)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("subs must be a list of sub patterns"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Let mqtt protocol check all parameters
     */
    json_t *kw_subscribe = json_pack("{s:O, s:i, s:i, s:i}",
        "subs", subs,
        "mid", mid,
        "qos", qos,
        "options", options
    );
    if(properties && json_is_object(properties)) {
        json_object_set(kw_subscribe, "properties", properties);
    }

    gobj_send_event(gobj, EV_SEND_MQTT_SUBSCRIBE, kw_subscribe, gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_unsubscribe(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *subs = kw_get_list(gobj, kw, "subs", 0, 0);
    int mid = (int)kw_get_int(gobj, kw, "mid", 0, 0);
    json_t *properties = kw_get_dict(gobj, kw, "properties", 0, 0);

    if(!json_is_array(subs)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("subs must be a list of sub patterns"),
            0,
            0,
            kw  // owned
        );
    }

    /*
     *  Let mqtt protocol check all parameters
     */
    json_t *kw_unsubscribe = json_pack("{s:O, s:i}",
        "subs", subs,
        "mid", mid
    );
    if(properties && json_is_object(properties)) {
        json_object_set(kw_unsubscribe, "properties", properties);
    }

    gobj_send_event(gobj, EV_SEND_MQTT_UNSUBSCRIBE, kw_unsubscribe, gobj);

    KW_DECREF(kw)
    return 0;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int yev_callback(yev_event_h yev_event)
{
    if(!yev_event) {
        /*
         *  It's the timeout
         */
        return 0;
    }

    hgobj gobj = yev_get_gobj(yev_event);

    uint32_t trace_level = gobj_trace_level(gobj);
    int trace = trace_level & TRACE_URING;
    if(trace) {
        json_t *jn_flags = bits2jn_strlist(yev_flag_strings(), yev_get_flag(yev_event));
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "yev callback",
            "msg2",         "%s", "🌐🌐💥 yev callback",
            "event type",   "%s", yev_event_type_name(yev_event),
            "state",        "%s", yev_get_state_name(yev_event),
            "result",       "%d", yev_get_result(yev_event),
            "sres",         "%s", (yev_get_result(yev_event)<0)? strerror(-yev_get_result(yev_event)):"",
            "flag",         "%j", jn_flags,
            "p",            "%p", yev_event,
            "fd",           "%d", yev_get_fd(yev_event),
            "gbuffer",      "%p", yev_get_gbuf(yev_event),
            NULL
        );
        json_decref(jn_flags);
    }

    yev_state_t yev_state = yev_get_state(yev_event);

    switch(yev_get_type(yev_event)) {
        case YEV_READ_TYPE:
            {
                if(yev_state == YEV_ST_IDLE) {
                    /*
                     *  yev_get_gbuf(yev_event) can be null if yev_stop_event() was called
                     */
                    int ret = 0;

                    ret = on_read_cb(gobj, yev_get_gbuf(yev_event));

                    /*
                     *  Clear buffer, re-arm read
                     *  Check ret is 0 because the EV_RX_DATA could provoke
                     *      stop or destroy of gobj
                     *      or order to disconnect (EV_DROP)
                     *  If try_to_stop_yevents() has been called (mt_stop, EV_DROP,...)
                     *      this event will be in stopped state.
                     *  If it's in idle then re-arm
                     */
                    if(ret == 0 && yev_event_is_idle(yev_event)) {
                        gbuffer_clear(yev_get_gbuf(yev_event));
                        yev_start_event(yev_event);
                    }

                } else {
                    /*
                     *  Disconnected
                     */
                    gobj_log_set_last_message("%s", strerror(-yev_get_result(yev_event)));

                    if(trace) {
                        gobj_log_debug(gobj, 0,
                            "function",     "%s", __FUNCTION__,
                            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                            "msg",          "%s", "read FAILED",
                            "errno",        "%d", -yev_get_result(yev_event),
                            "strerror",     "%s", strerror(-yev_get_result(yev_event)),
                            "p",            "%p", yev_event,
                            "fd",           "%d", yev_get_fd(yev_event),
                            NULL
                        );
                    }
                    gobj_shutdown();
                }
            }
            break;

        default:
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "TCP: event type NOT IMPLEMENTED",
                "msg2",         "%s", "🌐TCP: event type NOT IMPLEMENTED",
                "url_mqtt",     "%s", gobj_read_str_attr(gobj, "url_mqtt"),
                "url_broker",   "%s", gobj_read_str_attr(gobj, "url_broker"),
                "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
                "event_type",   "%s", yev_event_type_name(yev_event),
                "p",            "%p", yev_event,
                NULL
            );
            break;
    }

    return 0;
}

/***************************************************************************
 *  Stop all events, is someone is running go to WAIT_STOPPED else STOPPED
 *  IMPORTANT this is the only place to set ST_WAIT_STOPPED state
 ***************************************************************************/
PRIVATE void try_to_stop_yevents(hgobj gobj)  // IDEMPOTENT
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    BOOL to_wait_stopped = FALSE;

    if(gobj_current_state(gobj)==ST_STOPPED) {
        gobj_set_exit_code(-1);
        gobj_shutdown();
        return;
    }

    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level & TRACE_URING) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "try_to_stop_yevents",
            "msg2",         "%s", "🟥🟥 try_to_stop_yevents",
            NULL
        );
    }

    if(priv->tty_fd > 0) {
        if(trace_level & TRACE_URING) {
            gobj_log_debug(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_YEV_LOOP,
                "msg",          "%s", "close socket fd_clisrv",
                "msg2",         "%s", "💥🟥 close socket fd_clisrv",
                "fd",           "%d", priv->tty_fd ,
                NULL
            );
        }

        close(priv->tty_fd);
        priv->tty_fd = -1;
    }

    if(priv->yev_reading) {
        if(!yev_event_is_stopped(priv->yev_reading)) {
            yev_stop_event(priv->yev_reading);
            if(!yev_event_is_stopped(priv->yev_reading)) {
                to_wait_stopped = TRUE;
            }
        }
    }

    if(to_wait_stopped) {
        gobj_change_state(gobj, ST_WAIT_STOPPED);
    } else {
        gobj_change_state(gobj, ST_STOPPED);
        gobj_set_exit_code(-1);
        gobj_shutdown();
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE keytable_t *event_by_key(keytable_t *keytable, uint8_t kb[8])
{
    for(int i=0; keytable[i].event!=0; i++) {
        if(memcmp(kb, keytable[i].keycode, strlen((const char *)keytable[i].keycode))==0) {
            return &keytable[i];
        }
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int process_key(hgobj gobj, uint8_t kb)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_char = json_pack("{s:i}",
        "char", kb
    );
    return gobj_send_event(priv->gobj_editline, EV_KEYCHAR, kw_char, gobj);
}

/***************************************************************************
 *  on read callback
 ***************************************************************************/
PRIVATE int on_read_cb(hgobj gobj, gbuffer_t *gbuf)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    size_t nread = gbuffer_leftbytes(gbuf);
    char *base = gbuffer_cur_rd_pointer(gbuf);

    if(nread == 0) {
        // Yes, sometimes arrive with nread 0.
        return 0;
    }

    if(base[0] == 3) {
        gobj_shutdown();
        return -1;
    }
    uint8_t b[8] = {0}; // To search keys in keytable
    memmove(b, base, MIN(sizeof(b), nread));

    do {
        struct keytable_s *kt = event_by_key(keytable, b);
        if(kt) {
            const char *dst = kt->dst_gobj;
            const char *event = kt->event;

            if(strcmp(dst, "editline")==0) {
                gobj_send_event(priv->gobj_editline, event, 0, gobj);
            } else {
                gobj_send_event(gobj, event, 0, gobj);
            }
        } else {
            for(int i=0; i<nread; i++) {
                process_key(gobj, base[i]);
            }
        }

    } while(0);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int do_authenticate_task(hgobj gobj)
{
    /*-----------------------------*
     *      Create the task
     *-----------------------------*/
    json_t *kw = json_pack("{s:s, s:s, s:s, s:s, s:s}",
        "auth_system", gobj_read_str_attr(gobj, "auth_system"),
        "auth_url", gobj_read_str_attr(gobj, "auth_url"),
        "user_id", gobj_read_str_attr(gobj, "user_id"),
        "user_passw", gobj_read_str_attr(gobj, "user_passw"),
        "azp", gobj_read_str_attr(gobj, "azp")
    );

    hgobj gobj_task = gobj_create_service(
        "task-authenticate",
        C_TASK_AUTHENTICATE,
        kw,
        gobj
    );
    gobj_subscribe_event(gobj_task, EV_ON_TOKEN, 0, gobj);
    gobj_set_volatil(gobj_task, TRUE); // auto-destroy

    /*-----------------------*
     *      Start task
     *-----------------------*/
    return gobj_start(gobj_task);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char mqtt_broker_config[]= "\
{                                               \n\
    'name': '(^^__url__^^)',                    \n\
    'gclass': 'C_IEVENT_CLI',                   \n\
    'as_service': true,                         \n\
    'kw': {                                     \n\
        'jwt': '(^^__jwt__^^)',                 \n\
        'url': '(^^__url__^^)',                 \n\
        'remote_yuno_name': '(^^__yuno_name__^^)',      \n\
        'remote_yuno_role': '(^^__yuno_role__^^)',      \n\
        'remote_yuno_service': '(^^__yuno_service__^^)' \n\
    },                                                  \n\
    'children': [                                       \n\
        {                                               \n\
            'name': 'mqtt_broker',                      \n\
            'gclass': 'C_IOGATE',                       \n\
            'children': [                               \n\
                {                                       \n\
                    'name': 'mqtt_broker',              \n\
                    'gclass': 'C_CHANNEL',              \n\
                    'children': [                       \n\
                        {                                       \n\
                            'name': 'mqtt_broker',              \n\
                            'gclass': 'C_WEBSOCKET',            \n\
                            'children': [                       \n\
                                {                               \n\
                                    'name': 'mqtt_broker',      \n\
                                    'gclass': 'C_TCP',          \n\
                                    'kw': {                     \n\
                                        'url': '(^^__url__^^)',                 \n\
                                        'cert_pem': '(^^__cert_pem__^^)'        \n\
                                    }                           \n\
                                }                               \n\
                            ]                                   \n\
                        }                                       \n\
                    ]                                   \n\
                }                                       \n\
            ]                                           \n\
        }                                               \n\
    ]                                           \n\
}                                               \n\
";

PRIVATE int cmd_connect_broker(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *jwt = gobj_read_str_attr(gobj, "jwt");
    const char *url = gobj_read_str_attr(gobj, "url_broker");
    if(empty_string(url)) {
        return 0;
    }
    const char *yuno_name = gobj_read_str_attr(gobj, "yuno_name");
    const char *yuno_role = gobj_read_str_attr(gobj, "yuno_role");
    const char *yuno_service = gobj_read_str_attr(gobj, "yuno_service");

    /*
     *  Each display window has a gobj to send the commands (saved in user_data).
     *  For external agents create a filter-chain of gobjs
     */
    json_t * jn_config_variables = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s}",
        "__jwt__", jwt,
        "__url__", url,
        "__cert_pem__", "",
        "__yuno_name__", yuno_name,
        "__yuno_role__", yuno_role,
        "__yuno_service__", yuno_service

    );

    priv->gobj_broker_connector = gobj_create_tree(
        gobj,
        mqtt_broker_config,
        jn_config_variables
    );

    gobj_start_tree(priv->gobj_broker_connector);

    if(priv->verbose || priv->interactive) {
        printf("Connecting to %s...\n", url);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char mqtt_connector_config[]= "\
{                                                       \n\
    'name': 'mqtt_connector',                           \n\
    'as_service': true,                                 \n\
    'gclass': 'C_IOGATE',                               \n\
    'children': [                                       \n\
        {                                               \n\
            'name': 'mqtt_connector',                   \n\
            'gclass': 'C_CHANNEL',                      \n\
            'children': [                                       \n\
                {                                                       \n\
                    'name': 'mqtt_connector',                           \n\
                    'gclass': 'C_PROT_MQTT2',                           \n\
                    'kw': {                                             \n\
                        'mqtt_client_id': '(^^__mqtt_client_id__^^)',   \n\
                        'mqtt_protocol': '(^^__mqtt_protocol__^^)',     \n\
                        'url': '(^^__url__^^)',                         \n\
                        'user_id': '(^^__user_id__^^)',                 \n\
                        'user_passw': '(^^__user_passw__^^)',           \n\
                        'cert_pem': '(^^__cert_pem__^^)',               \n\
                        'jwt': '(^^__jwt__^^)',                         \n\
                        'iamServer': false                              \n\
                    },                                                  \n\
                    'children': [                                       \n\
                        {                                               \n\
                            'name': 'mqtt_connector',                   \n\
                            'gclass': 'C_TCP',                          \n\
                            'kw': {                                     \n\
                                'url': '(^^__url__^^)',                 \n\
                                'cert_pem': '(^^__cert_pem__^^)'        \n\
                            }                                           \n\
                        }                                               \n\
                    ]                                                   \n\
                }                                           \n\
            ]                                               \n\
        }                                                   \n\
    ]                                                       \n\
}                                                           \n\
";

PRIVATE int cmd_connect_mqtt(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *jwt = gobj_read_str_attr(gobj, "jwt");
    const char *url = gobj_read_str_attr(gobj, "url_mqtt");

    const char *yuno_name = gobj_read_str_attr(gobj, "yuno_name");
    const char *yuno_role = gobj_read_str_attr(gobj, "yuno_role");
    const char *yuno_service = gobj_read_str_attr(gobj, "yuno_service");

    const char *mqtt_client_id = gobj_read_str_attr(gobj, "mqtt_client_id");
    const char *mqtt_protocol = gobj_read_str_attr(gobj, "mqtt_protocol");

    const char *user_id = gobj_read_str_attr(gobj, "user_id");
    const char *user_passw = gobj_read_str_attr(gobj, "user_passw");

    /*
     *  Each display window has a gobj to send the commands (saved in user_data).
     *  For external agents create a filter-chain of gobjs
     */
    json_t * jn_config_variables = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "__jwt__", jwt,
        "__url__", url,
        "__cert_pem__", "",
        "__yuno_name__", yuno_name,
        "__yuno_role__", yuno_role,
        "__yuno_service__", yuno_service,
        "__mqtt_client_id__", mqtt_client_id,
        "__mqtt_protocol__", mqtt_protocol,
        "__user_id__", user_id,
        "__user_passw__", user_passw

    );

    /* TODO
     * must be callbacks for: connect, disconnect, publish, message, subscribe, unsubscribe
     */
    priv->gobj_mqtt_connector = gobj_create_tree(
        gobj,
        mqtt_connector_config,
        jn_config_variables
    );

    gobj_set_bottom_gobj(gobj, priv->gobj_mqtt_connector);
    gobj_start_tree(priv->gobj_mqtt_connector);

    if(priv->verbose || priv->interactive) {
        printf("Connecting to %s...\n", url);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int do_command(hgobj gobj, const char *command) // Not used
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // Pass it through the event to do replace_cli_vars()
    json_t *kw_line = json_object();
    json_object_set_new(kw_line, "text", json_string(command));
    gobj_send_event(priv->gobj_editline, EV_SETTEXT, kw_line, gobj);
    gobj_send_event(gobj, EV_COMMAND, 0, priv->gobj_editline);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE gbuffer_t *jsontable2str(json_t *jn_schema, json_t *jn_data)
{
    gbuffer_t *gbuf = gbuffer_create(4*1024, gbmem_get_maximum_block());
    size_t col;
    json_t *jn_col;
    hgobj gobj = 0;

    /*
     *  Paint Headers
     */
    json_array_foreach(jn_schema, col, jn_col) {
        const char *header = kw_get_str(gobj, jn_col, "header", "", 0);
        int fillspace = (int)kw_get_int(gobj, jn_col, "fillspace", 10, 0);
        if(fillspace && fillspace < strlen(header)) {
            fillspace = (int)strlen(header);
        }
        if(fillspace > 0) {
            gbuffer_printf(gbuf, "%-*.*s ", fillspace, fillspace, header);
        }
    }
    gbuffer_printf(gbuf, "\n");

    /*
     *  Paint ===
     */
    json_array_foreach(jn_schema, col, jn_col) {
        const char *header = kw_get_str(gobj, jn_col, "header", "", 0);
        int fillspace = (int)kw_get_int(gobj, jn_col, "fillspace", 10, 0);
        if(fillspace && fillspace < strlen(header)) {
            fillspace = (int)strlen(header);
        }
        if(fillspace > 0) {
            gbuffer_printf(gbuf,
                "%*.*s ",
                fillspace,
                fillspace,
                "==========================================================================="
            );
        }
    }
    gbuffer_printf(gbuf, "\n");

    /*
     *  Paint data
     */
    size_t row;
    json_t *jn_row;
    json_array_foreach(jn_data, row, jn_row) {
        json_array_foreach(jn_schema, col, jn_col) {
            const char *id = kw_get_str(gobj, jn_col, "id", 0, 0);
            int fillspace = (int)kw_get_int(gobj, jn_col, "fillspace", 10, 0);
            const char *header = kw_get_str(gobj, jn_col, "header", "", 0);
            if(fillspace && fillspace < strlen(header)) {
                fillspace = (int)strlen(header);
            }
            if(fillspace > 0) {
                json_t *jn_cell = kw_get_dict_value(gobj, jn_row, id, 0, 0);
                char *text = json2uglystr(jn_cell);
                if(json_is_number(jn_cell) || json_is_boolean(jn_cell)) {
                    //gbuffer_printf(gbuf, "%*s ", fillspace, text);
                    gbuffer_printf(gbuf, "%-*.*s ", fillspace, fillspace, text);
                } else {
                    gbuffer_printf(gbuf, "%-*.*s ", fillspace, fillspace, text);
                }
                GBMEM_FREE(text);
            }
        }
        gbuffer_printf(gbuf, "\n");
    }
    gbuffer_printf(gbuf, "\nTotal: %d\n", (int)row);

    return gbuf;
}

/***************************************************************************
 *  Print json response in display list window
 ***************************************************************************/
PRIVATE int display_webix_result(
    hgobj gobj,
    json_t *webix)
{
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = (int)kw_get_int(gobj, webix, "result", -1, 0);
    const char *comment = kw_get_str(gobj, webix, "comment", "", 0);
    json_t *jn_schema = kw_get_dict_value(gobj, webix, "schema", 0, 0);
    json_t *jn_data = kw_get_dict_value(gobj, webix, "data", 0, 0);

    const char *display_mode = gobj_read_str_attr(gobj, "display_mode");
    json_t *jn_display_mode = kw_get_subdict_value(gobj, webix, "__md_iev__", "display_mode", 0, 0);
    if(jn_display_mode) {
        display_mode = json_string_value(jn_display_mode);
    }
    BOOL mode_form = FALSE;
    if(!empty_string(display_mode)) {
        if(strcasecmp(display_mode, "form")==0)  {
            mode_form = TRUE;
        }
    }

    if(json_is_array(jn_data)) {
        if (mode_form) {
            char *data = json2str(jn_data);
            printf("%s\n", data);
            gbmem_free(data);
        } else {
            /*
             *  display as table
             */
            if(jn_schema && json_array_size(jn_schema)) {
                gbuffer_t *gbuf = jsontable2str(jn_schema, jn_data);
                if(gbuf) {
                    char *p = gbuffer_cur_rd_pointer(gbuf);
                    printf("%s\n", p);
                    gbuffer_decref(gbuf);
                }
            } else {
                char *text = json2str(jn_data);
                if(text) {
                    printf("%s\n", text);
                    gbmem_free(text);
                }
            }
        }
    } else if(json_is_object(jn_data)) {
        char *data = json2str(jn_data);
        printf("%s\n", data);
        gbmem_free(data);
    }

    if(result < 0) {
        printf("%sERROR %d: %s%s\n", On_Red BWhite, result, comment, Color_Off);
    } else {
        if(!empty_string(comment)) {
            printf("%s\n", comment);
        }
    }

    clear_input_line(gobj);

    JSON_DECREF(webix);
    return 0;
}

/***************************************************************************
 *  Clear input line
 ***************************************************************************/
PRIVATE int clear_input_line(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    printf("\n");
    json_t *kw_line = json_object();
    json_object_set_new(kw_line, "text", json_string(""));
    gobj_send_event(priv->gobj_editline, EV_SETTEXT, kw_line, gobj);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *get_history_file(char *bf, int bfsize)
{
    char *home = getenv("HOME");
    memset(bf, 0, bfsize);
    if(home) {
        snprintf(bf, bfsize, "%s/.yuneta", home);
        mkdir(bf, 0700);
        strcat(bf, "/mqtt.txt");
    }
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int list_history(hgobj gobj)
{
    char history_file[PATH_MAX];
    get_history_file(history_file, sizeof(history_file));

    FILE *file = fopen(history_file, "r");
    if(file) {
        char temp[4*1024];
        while(fgets(temp, sizeof(temp), file)) {
            left_justify(temp);
            if(strlen(temp)>0) {
                printf("%s\n", temp);
            }
        }
        fclose(file);
    }
    clear_input_line(gobj);
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_token(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int result = (int)kw_get_int(gobj, kw, "result", -1, KW_REQUIRED);
    if(result < 0) {
        if(priv->verbose || priv->interactive) {
            const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
            printf("\n%s", comment);
            printf("\nAbort.\n");
        }
        gobj_set_exit_code(-1);
        gobj_shutdown();
    } else {
        const char *jwt = kw_get_str(gobj, kw, "jwt", "", KW_REQUIRED);
        gobj_write_str_attr(gobj, "jwt", jwt);
        cmd_connect_broker(gobj);
        cmd_connect_mqtt(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Execute batch of input parameters when the route is opened.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->verbose || priv->interactive) {
        printf("Connected to '%s':\n",
            gobj_read_str_attr(src, "peername")
        );
        if(json_size(kw)) {
            kw_delete_metadata_keys(kw);
            json_dumpf(kw, stdout, JSON_INDENT(4)|JSON_ENCODE_ANY);
            printf("\n");
        }
    }

    const char *command = gobj_read_str_attr(gobj, "command");
    // command="h"; // TODO TEST
    if(priv->interactive) {
        if(!empty_string(command)) {
            do_command(gobj, command);
        } else {
            printf("Type Ctrl+c for exit, 'help' for help, 'history' for show history\n");
            clear_input_line(gobj);
        }
    } else {
        if(empty_string(command)) {
            printf("What command?\n");
        } else {
            do_command(gobj, command);
        }
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

    if(priv->verbose || priv->interactive) {
        if(!gobj_is_shutdowning()) {
            const char *comment = kw_get_str(gobj, kw, "comment", 0, 0);
            printf("\nDisconnected from: %s. %s\n", gobj_read_str_attr(src, "url"), comment);
        }
    }

    if(!gobj_is_running(gobj)) {
        try_to_stop_yevents(gobj);
        printf("\n");
    } else {
        clear_input_line(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Send publish command to the broker
 ***************************************************************************/
PRIVATE int ac_send_mqtt_publish(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Using C_CHANNEL it must be a IEV
     */
    json_t *kw_iev = iev_create(
        gobj,
        EV_MQTT_PUBLISH,
        kw // owned
    );

    return gobj_send_event(priv->gobj_mqtt_connector, EV_SEND_IEV, kw_iev, gobj);
}

/***************************************************************************
 *  Send subscribe command to the broker
 ***************************************************************************/
PRIVATE int ac_send_mqtt_subscribe(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Using C_CHANNEL it must be a IEV
     */
    json_t *kw_iev = iev_create(
        gobj,
        EV_MQTT_SUBSCRIBE,
        kw // owned
    );

    return gobj_send_event(priv->gobj_mqtt_connector, EV_SEND_IEV, kw_iev, gobj);
}

/***************************************************************************
 *  Send unsubscribe command to the broker
 ***************************************************************************/
PRIVATE int ac_send_mqtt_unsubscribe(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Using C_CHANNEL it must be a IEV
     */
    json_t *kw_iev = iev_create(
        gobj,
        EV_MQTT_UNSUBSCRIBE,
        kw // owned
    );

    return gobj_send_event(priv->gobj_mqtt_connector, EV_SEND_IEV, kw_iev, gobj);
}

/***************************************************************************
 *  Receive message from the broker
 ***************************************************************************/
PRIVATE int ac_mqtt_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    kw_delete_metadata_keys(kw);
    printf("Message from broker:\n");

    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);
    if(gbuf) {
        const char *bf = gbuffer_cur_rd_pointer(gbuf);
        size_t len = gbuffer_chunk(gbuf);
        json_t *data = anystring2json(bf, len, FALSE);
        if(!data) {
            data = tdump2json((const uint8_t *)bf, len);
        }
        json_object_set_new(kw, "payload (gbuffer content)", data);
    }
    json_dumpf(kw, stdout, JSON_INDENT(4)|JSON_ENCODE_ANY);

    clear_input_line(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Receive puback from the broker
 ***************************************************************************/
PRIVATE int ac_mqtt_puback(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    kw_delete_metadata_keys(kw);
    printf("Publish ack\n");
    json_dumpf(kw, stdout, JSON_INDENT(4)|JSON_ENCODE_ANY);
    clear_input_line(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Receive suback from the broker
 ***************************************************************************/
PRIVATE int ac_mqtt_suback(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    kw_delete_metadata_keys(kw);
    printf("Subscribe ack\n");
    json_dumpf(kw, stdout, JSON_INDENT(4)|JSON_ENCODE_ANY);
    clear_input_line(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Receive unsuback from the broker
 ***************************************************************************/
PRIVATE int ac_mqtt_unsuback(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    kw_delete_metadata_keys(kw);
    printf("Unsubscribe ack\n");
    json_dumpf(kw, stdout, JSON_INDENT(4)|JSON_ENCODE_ANY);
    clear_input_line(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  HACK Este evento solo puede venir de GCLASS_EDITLINE
 ***************************************************************************/
PRIVATE int ac_command(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_input_command = json_object();
    gobj_send_event(src, EV_GETTEXT, json_incref(kw_input_command), gobj); // HACK EV_GETTEXT is EVF_KW_WRITING
    const char *command = kw_get_str(gobj, kw_input_command, "text", 0, 0);

    if(empty_string(command)) {
        clear_input_line(gobj);
        KW_DECREF(kw_input_command);
        KW_DECREF(kw);
        return 0;
    }

    printf("\n");

    if(strcasecmp(command, "exit")==0 || strcasecmp(command, "quit")==0) {
        gobj_stop(gobj);
        KW_DECREF(kw_input_command);
        KW_DECREF(kw);
        return 0;
    }

    if(strcasecmp(command, "history")==0) {
        list_history(gobj);
        KW_DECREF(kw_input_command);
        KW_DECREF(kw);
        return 0;
    }

    char comment[512]={0};
    gbuffer_t *gbuf_parsed_command = replace_cli_vars(command, comment, sizeof(comment));
    if(!gbuf_parsed_command) {
        printf("%s%s%s\n", On_Red BWhite, command, Color_Off);
        clear_input_line(gobj);
        KW_DECREF(kw_input_command);
        KW_DECREF(kw);
        return 0;
    }
    char *xcmd = gbuffer_cur_rd_pointer(gbuf_parsed_command);
    json_t *kw_command = json_object();
    if(*xcmd == '*') {
        xcmd++;
        kw_set_subdict_value(gobj, kw_command, "__md_iev__", "display_mode", json_string("form"));
    }

    json_t *webix = 0;
    if(gobj_command_desc(gobj, command, FALSE)) {
        webix = gobj_command(gobj, xcmd, kw_command, gobj);
    } else {
        if(gobj_read_bool_attr(priv->gobj_broker_connector, "opened")) {
            // gobj_command_desc
            webix = gobj_command(priv->gobj_broker_connector, xcmd, kw_command, gobj);
        } else {
            printf("\n%s%s%s\n", On_Red BWhite, "No connection with broker", Color_Off);
            clear_input_line(gobj);
            JSON_DECREF(kw_command)
        }
    }

    gbuffer_decref(gbuf_parsed_command);

    /*
     *  Print json response in display window
     */
    if(webix) {
        display_webix_result(
            gobj,
            webix
        );
    } else {
        /* asynchronous responses return 0 */
        printf("\n"); fflush(stdout);
    }
    KW_DECREF(kw_input_command);
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Command received.
 ***************************************************************************/
PRIVATE int ac_command_answer(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    printf("\n");

    if(priv->interactive) {
        return display_webix_result(
            gobj,
            kw
        );
    } else {
        int result = (int)kw_get_int(gobj, kw, "result", -1, 0);
        const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
        if(result != 0){
            printf("%sERROR %d: %s%s\n", On_Red BWhite, result, comment, Color_Off);
        } else {
            json_t *jn_data = kw_get_dict_value(gobj, kw, "data", 0, 0);
            if(json_is_string(jn_data)) {
                const char *data = json_string_value(jn_data);
                printf("%s\n", data);
            } else {
                if(!gobj_read_bool_attr(gobj, "print_with_metadata")) {
                    json_t *jn_data2 = kw_filter_metadata(gobj, json_incref(jn_data));
                    print_json2("", jn_data2);
                    JSON_DECREF(jn_data2);
                } else {
                    print_json2("", jn_data);
                }
            }
            if(!empty_string(comment)) {
                printf("%s\n", comment);
            }
        }
        KW_DECREF(kw);
        gobj_set_exit_code(result);

        set_timeout(priv->timer, 1000);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_screen_ctrl(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    SWITCHS(event) {
        CASES(EV_CLRSCR)
            printf(Clear_Screen);
            fflush(stdout);
            gobj_send_event(priv->gobj_editline, EV_PAINT, 0, gobj);
            break;
        CASES(EV_SCROLL_PAGE_UP)
            break;
        CASES(EV_SCROLL_PAGE_DOWN)
            break;
        CASES(EV_SCROLL_LINE_UP)
            break;
        CASES(EV_SCROLL_LINE_DOWN)
            break;
        CASES(EV_SCROLL_TOP)
            break;
        CASES(EV_SCROLL_BOTTOM)
            break;
        DEFAULTS
            break;
    } SWITCHS_END;

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gobj_shutdown();

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
    .mt_create                  = mt_create,
    .mt_destroy                 = mt_destroy,
    .mt_start                   = mt_start,
    .mt_stop                    = mt_stop,
    .mt_writing                 = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_MQTT_CLIENT);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_SEND_MQTT_SUBSCRIBE);
GOBJ_DEFINE_EVENT(EV_SEND_MQTT_UNSUBSCRIBE);
GOBJ_DEFINE_EVENT(EV_SEND_MQTT_PUBLISH);

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

    keytable_t keytable_[] = {
        {"editline",    EV_EDITLINE_MOVE_START,       CTRL_A},
        {"editline",    EV_EDITLINE_MOVE_START,       MKEY_START},
        {"editline",    EV_EDITLINE_MOVE_START,       MKEY_START2},
        {"editline",    EV_EDITLINE_MOVE_END,         CTRL_E},
        {"editline",    EV_EDITLINE_MOVE_END,         MKEY_END},
        {"editline",    EV_EDITLINE_MOVE_END,         MKEY_END2},
        {"editline",    EV_EDITLINE_MOVE_LEFT,        CTRL_B},
        {"editline",    EV_EDITLINE_MOVE_LEFT,        MKEY_LEFT},
        {"editline",    EV_EDITLINE_MOVE_LEFT,        MKEY_LEFT2},
        {"editline",    EV_EDITLINE_MOVE_RIGHT,       CTRL_F},
        {"editline",    EV_EDITLINE_MOVE_RIGHT,       MKEY_RIGHT},
        {"editline",    EV_EDITLINE_MOVE_RIGHT,       MKEY_RIGHT2},
        {"editline",    EV_EDITLINE_DEL_CHAR,         CTRL_D},
        {"editline",    EV_EDITLINE_DEL_CHAR,         MKEY_DEL},
        {"editline",    EV_EDITLINE_BACKSPACE,        CTRL_H},
        {"editline",    EV_EDITLINE_BACKSPACE,        BACKSPACE},
        {"editline",    EV_EDITLINE_COMPLETE_LINE,    TAB},
        {"editline",    EV_EDITLINE_ENTER,            ENTER},
        {"editline",    EV_EDITLINE_PREV_HIST,        MKEY_UP},
        {"editline",    EV_EDITLINE_PREV_HIST,        MKEY_UP2},
        {"editline",    EV_EDITLINE_NEXT_HIST,        MKEY_DOWN},
        {"editline",    EV_EDITLINE_NEXT_HIST,        MKEY_DOWN2},
        {"editline",    EV_EDITLINE_SWAP_CHAR,        CTRL_T},
        {"editline",    EV_EDITLINE_DEL_LINE,         CTRL_U},
        {"editline",    EV_EDITLINE_DEL_LINE,         CTRL_Y},
        {"editline",    EV_EDITLINE_DEL_PREV_WORD,    CTRL_W},

        {"screen",      EV_CLRSCR,                    CTRL_K},
        {"screen",      EV_SCROLL_PAGE_UP,            MKEY_PREV_PAGE},
        {"screen",      EV_SCROLL_PAGE_DOWN,          MKEY_NEXT_PAGE},
        {"screen",      EV_SCROLL_LINE_UP,            MKEY_ALT_PREV_PAGE},
        {"screen",      EV_SCROLL_LINE_DOWN,          MKEY_ALT_NEXT_PAGE},
        {"screen",      EV_SCROLL_TOP,                MKEY_ALT_START},
        {"screen",      EV_SCROLL_BOTTOM,             MKEY_ALT_END},

        {0}
    };

    for(int i=0; i<MAX_KEYS-1 && i<ARRAY_SIZE(keytable_); i++) {
        if(!keytable_[i].dst_gobj) {
            break;
        }
        keytable[i] = keytable_[i];
    }

    /*------------------------*
     *      States
     *------------------------*/
    ev_action_t st_idle[] = {
        {EV_MQTT_MESSAGE,           ac_mqtt_message,            0}, // Message from broker
        {EV_SEND_MQTT_PUBLISH,      ac_send_mqtt_publish,       0}, // Send publish to broker
        {EV_MQTT_PUBLISH,           ac_mqtt_puback,             0}, // Ack to publish
        {EV_SEND_MQTT_SUBSCRIBE,    ac_send_mqtt_subscribe,     0}, // Send subscribe to broker
        {EV_MQTT_SUBSCRIBE,         ac_mqtt_suback,             0}, // Ack to subscribe
        {EV_SEND_MQTT_UNSUBSCRIBE,  ac_send_mqtt_unsubscribe,   0}, // Send unsubscribe to broker
        {EV_MQTT_UNSUBSCRIBE,       ac_mqtt_unsuback,           0}, // Ack to unsubscribe

        {EV_COMMAND,                ac_command,             0},
        {EV_MT_COMMAND_ANSWER,      ac_command_answer,      0},
        {EV_MT_STATS_ANSWER,        ac_command_answer,      0},
        {EV_ON_OPEN,                ac_on_open,             0},
        {EV_ON_TOKEN,               ac_on_token,            0},
        {EV_ON_ID_NAK,              ac_on_close,            0},
        {EV_ON_CLOSE,               ac_on_close,            0},
        {EV_CLRSCR,                 ac_screen_ctrl,         0},
        {EV_SCROLL_PAGE_UP,         ac_screen_ctrl,         0},
        {EV_SCROLL_PAGE_DOWN,       ac_screen_ctrl,         0},
        {EV_SCROLL_LINE_UP,         ac_screen_ctrl,         0},
        {EV_SCROLL_LINE_DOWN,       ac_screen_ctrl,         0},
        {EV_SCROLL_TOP,             ac_screen_ctrl,         0},
        {EV_SCROLL_BOTTOM,          ac_screen_ctrl,         0},
        {EV_TIMEOUT,                ac_timeout,             0},
        {EV_STOPPED,                0,                      0},
        {0,0,0}
    };

    ev_action_t st_wait_stopped[] = {
        {EV_ON_CLOSE,               ac_on_close,            0},
        {0,0,0}
    };

    ev_action_t st_stopped[] = {
        {EV_ON_CLOSE,               ac_on_close,            0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                   st_idle},
        {ST_WAIT_STOPPED,           st_wait_stopped},
        {ST_STOPPED,                st_stopped},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_MQTT_MESSAGE,           0},     // Message from broker
        {EV_SEND_MQTT_PUBLISH,      0},     // Send publish to broker
        {EV_MQTT_PUBLISH,           0},     // Ack to publish
        {EV_SEND_MQTT_SUBSCRIBE,    0},     // Send subscribe to broker
        {EV_MQTT_SUBSCRIBE,         0},     // Ack to subscribe
        {EV_SEND_MQTT_UNSUBSCRIBE,  0},     // Send unsubscribe to broker
        {EV_MQTT_UNSUBSCRIBE,       0},     // Ack to unsubscribe

        {EV_MT_COMMAND_ANSWER,      EVF_PUBLIC_EVENT},
        {EV_MT_STATS_ANSWER,        EVF_PUBLIC_EVENT},
        {EV_COMMAND,                0},
        {EV_CLRSCR,                 0},
        {EV_SCROLL_PAGE_UP,         0},
        {EV_SCROLL_PAGE_DOWN,       0},
        {EV_SCROLL_LINE_UP,         0},
        {EV_SCROLL_LINE_DOWN,       0},
        {EV_SCROLL_TOP,             0},
        {EV_SCROLL_BOTTOM,          0},
        {EV_ON_TOKEN,               0},
        {EV_ON_OPEN,                0},
        {EV_ON_CLOSE,               0},
        {EV_ON_ID_NAK,              0},
        {EV_TIMEOUT,                0},
        {EV_STOPPED,                0},
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
        0,  // Local methods table
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // Authorization table
        command_table,  // Command table
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
PUBLIC int register_c_mqtt_client(void)
{
    return create_gclass(C_MQTT_CLIENT);
}
