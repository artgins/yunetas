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
PRIVATE int cmd_connect(hgobj gobj);
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

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag--------default---------description---------- */
SDATA (DTP_BOOLEAN,     "print_with_metadata",0,        0,              "Print response with metadata."),
SDATA (DTP_BOOLEAN,     "verbose",          0,          "1",            "Verbose mode."),
SDATA (DTP_BOOLEAN,     "interactive",      0,          0,              "Interactive."),
SDATA (DTP_INTEGER,     "wait",             0,          "2",            "Wait n seconds until exit"),
SDATA (DTP_STRING,      "command",          0,          "",             "Command."),
SDATA (DTP_STRING,      "url",              0,          "ws://127.0.0.1:1991",  "Url to get Statistics. Can be a ip/hostname or a full url"),
SDATA (DTP_STRING,      "yuno_name",        0,          "",             "Yuno name"),
SDATA (DTP_STRING,      "yuno_role",        0,          "yuneta_agent", "Yuno role"),
SDATA (DTP_STRING,      "yuno_service",     0,          "agent",        "Yuno service"),
SDATA (DTP_STRING,      "auth_system",      0,          "",             "OpenID System(interactive jwt)"),
SDATA (DTP_STRING,      "auth_url",         0,          "",             "OpenID Endpoint(interactive jwt)"),
SDATA (DTP_STRING,      "azp",              0,          "",             "azp (OAuth2 Authorized Party)"),
SDATA (DTP_STRING,      "user_id",          0,          "",             "OAuth2 User Id (interactive jwt)"),
SDATA (DTP_STRING,      "user_passw",       0,          "",             "OAuth2 User password (interactive jwt)"),
SDATA (DTP_STRING,      "jwt",              0,          "",             "Jwt"),
SDATA (DTP_POINTER,     "gobj_connector",   0,          0,              "connection gobj"),
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
    uint32_t wait;

    hgobj gobj_connector;
    hgobj timer;
    hgobj gobj_editline;
    hgobj gobj_remote_agent;

    int tty_fd;
    yev_event_h yev_reading;

    BOOL on_mirror_tty;
    char mirror_tty_name[NAME_MAX];
    char mirror_tty_uuid[NAME_MAX];
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
        "prompt", "ycommand> ",
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
    SET_PRIV(gobj_connector,        gobj_read_pointer_attr)
    SET_PRIV(verbose,               gobj_read_bool_attr)
    SET_PRIV(interactive,           gobj_read_bool_attr)
    SET_PRIV(wait,                  gobj_read_integer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(gobj_connector,     gobj_read_pointer_attr)
    END_EQ_SET_PRIV()
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
        cmd_connect(gobj);
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
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
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
        if(!priv->on_mirror_tty) {
            gobj_shutdown();
            return -1;
        }
    }
    uint8_t b[8] = {0}; // To search keys in keytable
    memmove(b, base, MIN(sizeof(b), nread));

    do {
        if(priv->on_mirror_tty) {
            hgobj gobj_cmd = gobj_read_pointer_attr(gobj, "gobj_connector");

            gbuffer_t *gbuf2 = gbuffer_create(nread, nread);
            gbuffer_append(gbuf2, base, nread);
            gbuffer_t *gbuf_content64 = gbuffer_encode_base64(
                gbuf2 // owned
            );
            char *content64 = gbuffer_cur_rd_pointer(gbuf_content64);

            json_t *kw_command = json_pack("{s:s, s:s, s:s}",
                "name", priv->mirror_tty_name,
                "agent_id", priv->mirror_tty_uuid,
                "content64", content64
            );

            json_decref(gobj_command(gobj_cmd, "write-tty", kw_command, gobj));

            GBUFFER_DECREF(gbuf_content64)
            break;
        }

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
PRIVATE char agent_config[]= "\
{                                               \n\
    'name': '(^^__url__^^)',                    \n\
    'gclass': 'IEvent_cli',                     \n\
    'as_service': true,                          \n\
    'kw': {                                     \n\
        'jwt': '(^^__jwt__^^)',                         \n\
        'remote_yuno_name': '(^^__yuno_name__^^)',      \n\
        'remote_yuno_role': '(^^__yuno_role__^^)',      \n\
        'remote_yuno_service': '(^^__yuno_service__^^)' \n\
    },                                          \n\
    'children': [                                 \n\
        {                                               \n\
            'name': 'agent_client',                    \n\
            'gclass': 'C_IOGATE',                         \n\
            'kw': {                                     \n\
            },                                          \n\
            'children': [                                 \n\
                {                                               \n\
                    'name': 'agent_client',                    \n\
                    'gclass': 'C_CHANNEL',                        \n\
                    'kw': {                                     \n\
                    },                                          \n\
                    'children': [                                 \n\
                        {                                               \n\
                            'name': 'agent_client',                    \n\
                            'gclass': 'C_WEBSOCKET',                     \n\
                            'children': [                           \n\
                                {                                   \n\
                                    'name': 'agent_client',         \n\
                                    'gclass': 'C_TCP',              \n\
                                    'kw': {                         \n\
                                        'url':'(^^__url__^^)'       \n\
                                    }                               \n\
                                }                                   \n\
                            ]                                       \n\
                        }                                       \n\
                    ]                                           \n\
                }                                               \n\
            ]                                           \n\
        }                                               \n\
    ]                                           \n\
}                                               \n\
";

PRIVATE int cmd_connect(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *jwt = gobj_read_str_attr(gobj, "jwt");
    const char *url = gobj_read_str_attr(gobj, "url");
    const char *yuno_name = gobj_read_str_attr(gobj, "yuno_name");
    const char *yuno_role = gobj_read_str_attr(gobj, "yuno_role");
    const char *yuno_service = gobj_read_str_attr(gobj, "yuno_service");

    /*
     *  Each display window has a gobj to send the commands (saved in user_data).
     *  For external agents create a filter-chain of gobjs
     */
    json_t * jn_config_variables = json_pack("{s:s, s:s, s:s, s:s, s:s}",
        "__jwt__", jwt,
        "__url__", url,
        "__yuno_name__", yuno_name,
        "__yuno_role__", yuno_role,
        "__yuno_service__", yuno_service
    );

    /*
     *  Get schema to select tls or not
     */
    char schema[20]={0}, host[120]={0}, port[40]={0};
    if(parse_url(
        gobj,
        url,
        schema,
        sizeof(schema),
        host, sizeof(host),
        port, sizeof(port),
        0, 0,
        0, 0,
        FALSE
    )<0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER_ERROR,
            "msg",          "%s", "parse_http_url() FAILED",
            "url",          "%s", url,
            NULL
        );
    }

    priv->gobj_remote_agent = gobj_create_tree(
        gobj,
        agent_config,
        jn_config_variables
    );

    gobj_start_tree(priv->gobj_remote_agent);

    if(priv->verbose || priv->interactive) {
        printf("Connecting to %s...\n", url);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int do_command(hgobj gobj, const char *command)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

//     json_t *jn_resp = gobj_command(priv->gobj_connector, command, 0, gobj);
//     json_decref(jn_resp);

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
    if(!priv->on_mirror_tty) {
        json_t *kw_line = json_object();
        json_object_set_new(kw_line, "text", json_string(""));
        gobj_send_event(priv->gobj_editline, EV_SETTEXT, kw_line, gobj);
    }

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
        strcat(bf, "/history2.txt");
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

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_local_json(hgobj gobj, char *path, int pathsize, const char *name, json_t *jn_content)
{
    const char *homedir;

    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
    snprintf(path, pathsize, "%s/.yuneta/configs/", homedir);
    if(access(path, 0)!=0) {
        mkrdir(path, 0700);
    }
    if(strlen(name) > 5 && strstr(name + strlen(name) - strlen(".json"), ".json")) {
        snprintf(path, pathsize, "%s/.yuneta/configs/%s", homedir, name);
    } else {
        snprintf(path, pathsize, "%s/.yuneta/configs/%s.json", homedir, name);
    }
    json_dump_file(jn_content, path, JSON_ENCODE_ANY | JSON_INDENT(4));
    JSON_DECREF(jn_content);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int edit_json(hgobj gobj, const char *path)
{
    const char *editor = gobj_read_str_attr(gobj, "editor");
    char command[PATH_MAX];
    snprintf(command, sizeof(command), "%s %s", editor, path);

    return pty_sync_spawn(command);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_local_string(
    hgobj gobj, char *path, int pathsize, const char *name, json_t *jn_content
) {
    const char *homedir;

    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
    snprintf(path, pathsize, "%s/.yuneta/configs/", homedir);
    if(access(path, 0)!=0) {
        mkrdir(path, 0700);
    }
    snprintf(path, pathsize, "%s/.yuneta/configs/%s", homedir, name);
    const char *s = json_string_value(jn_content);
    if(s) {
        FILE *file = fopen(path, "w");
        if(file) {
            fwrite(s, strlen(s), 1, file);
            fclose(file);
        }
    }
    JSON_DECREF(jn_content);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int save_local_base64(
    hgobj gobj,
    char *path,
    int pathsize,
    const char *name,
    json_t *jn_content
) {
    const char *homedir;

    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
    snprintf(path, pathsize, "%s/.yuneta/configs/", homedir);
    if(access(path, 0)!=0) {
        mkrdir(path, 0700);
    }
    snprintf(path, pathsize, "%s/.yuneta/configs/%s", homedir, name);

    const char *s = json_string_value(jn_content);
    if(s) {
        gbuffer_t *gbuf_bin = gbuffer_base64_to_string(s, strlen(s));
        if(gbuf_bin) {
            int fp = newfile(path, 0700, TRUE);
            if(fp) {
                ssize_t x = write(
                    fp,
                    gbuffer_cur_rd_pointer(gbuf_bin),
                    gbuffer_leftbytes(gbuf_bin)
                );
                if(x) {} // avoid warning
                close(fp);
            }
        }
    }
    JSON_DECREF(jn_content);
    return 0;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
//    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    printf("Timeout\n");

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

/***************************************************************************
 *          Create the GClass
 ***************************************************************************/
PRIVATE int create_gclass(gclass_name_t gclass_name)
{
    static hgclass __gclass__ = 0;
    if(__gclass__) {
        gobj_log_error(0, 0,
            "function", "%s", __FUNCTION__,
            "msgset",   "%s", MSGSET_INTERNAL_ERROR,
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
        {EV_TIMEOUT,                ac_timeout,              0},
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
        {EV_TIMEOUT,                0},
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
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0, //authz_table,
        0, //command_table,
        s_user_trace_level,
        0 // gcflags
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
