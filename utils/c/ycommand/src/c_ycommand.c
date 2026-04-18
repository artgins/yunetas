/***********************************************************************
 *          C_YCOMMAND.C
 *          YCommand GClass.
 *
 *          Yuneta Command
 *
 *          Copyright (c) 2016 Niyamaka.
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
#include <ctype.h>

#include <g_ev_console.h>
#include <c_editline.h>
#include "c_ycommand.h"

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
#define CTRL_L  {12}
#define ENTER   {13}
#define CTRL_N  {14}
#define CTRL_P  {16}
#define CTRL_R  {18}
#define CTRL_S  {19}
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
PRIVATE int request_commands_cache(hgobj gobj);
PRIVATE void merge_commands_into_cache(hgobj gobj, json_t *jn_raw_data);
PRIVATE BOOL is_commands_list_response(json_t *jn_data);
PRIVATE BOOL line_has_param(const char *buf, const char *pname);
PRIVATE int list_history(hgobj gobj);
PRIVATE void split_commands_into_array(const char *text, json_t *target);
PRIVATE void split_commands_into_queue(hgobj gobj, const char *text);
PRIVATE int exec_one_command(hgobj gobj, const char *cmdline);
PRIVATE void run_next_pending(hgobj gobj);
PRIVATE json_t *cmd_local_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_local_history(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_local_clear_history(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_local_exit(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_local_source(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE void ycommand_completion_cb(
    hgobj gobj, const char *buf, editline_completions_t *lc, void *user_data
);
PRIVATE char *ycommand_hints_cb(
    hgobj gobj, const char *buf, int *out_color, int *out_bold, void *user_data
);
PRIVATE void ycommand_free_hint_cb(char *hint, void *user_data);

/***************************************************************************
 *  Local command table (C_YCOMMAND's own commands).
 *  Invoked with the `!` prefix (following the c_cli convention); the bare
 *  forms `history`, `exit`, `quit` are also still intercepted in
 *  ac_command for backward compatibility.
 ***************************************************************************/
PRIVATE sdata_desc_t local_pm_help[] = {
SDATAPM (DTP_STRING,    "cmd",      0,      0,  "Show help for this specific command"),
SDATAPM (DTP_INTEGER,   "level",    0,      0,  "Depth of help search (1 = bottoms)"),
SDATA_END()
};

PRIVATE const char *a_help[]          = {"h", "?", 0};
PRIVATE const char *a_history[]       = {"hi", 0};
PRIVATE const char *a_clear_history[] = {"clh", 0};
PRIVATE const char *a_exit[]          = {"x", 0};
PRIVATE const char *a_quit[]          = {"q", 0};
PRIVATE const char *a_source[]        = {".", 0};

PRIVATE sdata_desc_t local_pm_source[] = {
SDATAPM (DTP_STRING,    "file",     0,      0,  "Script file path"),
SDATA_END()
};

PRIVATE sdata_desc_t local_command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-----------json_fn---------description---------- */
SDATACM (DTP_SCHEMA,    "",                 0,                  0,              0,              "\nLine-edit shortcuts\n-------------------"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Move start          Home, Ctrl+A"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Move end            End, Ctrl+E"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Move left           Left, Ctrl+B"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Move right          Right, Ctrl+F"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Delete char         Del, Ctrl+D"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Backspace           Backspace, Ctrl+H"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Execute             Enter"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Swap char           Ctrl+T"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Delete to EOL       Ctrl+K (readline)"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Delete line         Ctrl+U, Ctrl+Y"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Delete prev. word   Ctrl+W"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Clear screen        Ctrl+L"),
SDATACM (DTP_SCHEMA,    "",                 0,                  0,              0,              "\nHistory & search\n----------------"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Previous history    Up"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Next history        Down"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Reverse search      Ctrl+R (press again for next older match)"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Forward search      Ctrl+S"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "Exit search         ESC / Ctrl+G (or any edit key to commit)"),
SDATACM (DTP_SCHEMA,    "",                 0,                  0,              0,              "\nCompletion\n----------"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "TAB                 Complete / cycle; lists candidates with descriptions"),
SDATACM (DTP_SCHEMA,    "",                 0,                  0,              0,              "\nLine-level syntax\n-----------------"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "!cmd                Invoke a local ycommand command (below)"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "!N                  Re-run history entry N"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "!!                  Re-run the last command"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "*cmd                Force display-mode form for the response"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "cmd1 ; cmd2 ; ...   Chain commands; each waits for the previous response"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "-cmd                Ignore errors for this command (ybatch convention)"),
SDATACM (DTP_STRING,    "", 0, 0, 0,  "cmd service=__yuno__  Send cmd to the yuno (instead of the default service)"),
SDATACM (DTP_SCHEMA,    "",                 0,                  0,              0,              "\nLocal commands\n--------------"),
SDATACM (DTP_SCHEMA,    "help",             a_help,             local_pm_help,  cmd_local_help,         "Show this help"),
SDATACM (DTP_SCHEMA,    "history",          a_history,          0,              cmd_local_history,      "List command history (also available as `history`)"),
SDATACM (DTP_SCHEMA,    "clear-history",    a_clear_history,    0,              cmd_local_clear_history,"Erase command history"),
SDATACM (DTP_SCHEMA,    "exit",             a_exit,             0,              cmd_local_exit,         "Exit ycommand (also available as `exit` / `quit`)"),
SDATACM (DTP_SCHEMA,    "quit",             a_quit,             0,              cmd_local_exit,         "Alias of exit"),
SDATACM (DTP_SCHEMA,    "source",           a_source,           local_pm_source,cmd_local_source,       "Read commands from a file and run them sequentially"),
SDATA_END()
};

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
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
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

    json_t *commands_cache;         /* flat dict: name -> cmd_obj */
    int pending_cache_fetches;      /* >0 while cache-warmup responses are pending */
    char last_sent_command[128];    /* first token of the user's last command, for did-you-mean */

    /*
     *  Command queue: each line the user submits (either typed, `;`-chained,
     *  sourced from a file, or piped on stdin) is split into one or more
     *  atomic commands and pushed here. run_next_pending() drains it one
     *  command at a time, waiting for the previous response before sending
     *  the next. On error the rest of the queue is dropped — unless the
     *  command was prefixed with '-' (ybatch convention: ignore-fail).
     */
    json_t *pending_commands;       /* json array of strings */
    BOOL current_ignore_fail;       /* true while an async `-cmd` is in flight */
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

    int interactive = gobj_read_bool_attr(gobj, "interactive");

    /*
     *  Input screen size and editline (only needed in interactive mode)
     */
    struct winsize winsz = {0};
    if(interactive) {
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
    }

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
    JSON_DECREF(priv->commands_cache)
    JSON_DECREF(priv->pending_commands)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);

    if(priv->interactive) {
        priv->tty_fd = tty_keyboard_init();
        if(priv->tty_fd < 0) {
            gobj_log_error(0, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_SYSTEM_ERROR,
                "msg",          "%s", "cannot open a tty window",
                NULL
            );
            printf("\n");
            exit(-1);
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
    }

    if(priv->interactive && priv->gobj_editline) {
        gobj_start(priv->gobj_editline);
    }

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
                    printf("\n");
                    exit(0);
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
        printf("\n");
        exit(-1);
    }

    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level & TRACE_URING) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_YEV_LOOP,
            "msg",          "%s", "try_to_stop_yevents",
            "msg2",         "%s", "🟥🟥 try_to_stop_yevents",
            "gobj_",        "%s", gobj?gobj_short_name(gobj):"",
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
        printf("\n");
        exit(-1);
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
            printf("\n");
            exit(-1);
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
            size_t consumed = strlen((const char *)kt->keycode);

            if(strcmp(dst, "editline")==0) {
                gobj_send_event(priv->gobj_editline, event, 0, gobj);
            } else {
                gobj_send_event(gobj, event, 0, gobj);
            }

            /*
             *  If the read batch carried more bytes after the matched key
             *  (e.g. user typed TAB immediately followed by a value), feed
             *  the rest through the normal char path. Otherwise we'd lose
             *  the keystroke that arrived in the same batch.
             */
            for(size_t i = consumed; i < nread; i++) {
                process_key(gobj, base[i]);
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
    'gclass': 'C_IEVENT_CLI',                   \n\
    'priority': 2,                              \n\
    'as_service': true,                         \n\
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

    if(priv->interactive) {
        // Pass it through the event to do replace_cli_vars()
        json_t *kw_line = json_object();
        json_object_set_new(kw_line, "text", json_string(command));
        gobj_send_event(priv->gobj_editline, EV_SETTEXT, kw_line, gobj);
        gobj_send_event(gobj, EV_COMMAND, 0, priv->gobj_editline);
    } else {
        // Non-interactive: bypass editline, pass text directly in kw
        json_t *kw = json_pack("{s:s}", "text", command);
        gobj_send_event(gobj, EV_COMMAND, kw, gobj);
    }

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
     *  Paint Headers (bold cyan so they stand out from the data).
     */
    json_array_foreach(jn_schema, col, jn_col) {
        const char *header = kw_get_str(gobj, jn_col, "header", "", 0);
        int fillspace = (int)kw_get_int(gobj, jn_col, "fillspace", 10, 0);
        if(fillspace && fillspace < strlen(header)) {
            fillspace = (int)strlen(header);
        }
        if(fillspace > 0) {
            gbuffer_printf(gbuf, "%s%-*.*s%s ",
                BCyan, fillspace, fillspace, header, Color_Off);
        }
    }
    gbuffer_printf(gbuf, "\n");

    /*
     *  Paint === (dim) so the header row has visible separation without
     *  pulling attention away from the data.
     */
    json_array_foreach(jn_schema, col, jn_col) {
        const char *header = kw_get_str(gobj, jn_col, "header", "", 0);
        int fillspace = (int)kw_get_int(gobj, jn_col, "fillspace", 10, 0);
        if(fillspace && fillspace < strlen(header)) {
            fillspace = (int)strlen(header);
        }
        if(fillspace > 0) {
            gbuffer_printf(gbuf,
                "%s%*.*s%s ",
                IBlack,
                fillspace,
                fillspace,
                "===========================================================================",
                Color_Off
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
/***************************************************************************
 *  Levenshtein distance for short ASCII strings (command names).
 *  Returns INT_MAX for strings longer than the internal bound.
 ***************************************************************************/
PRIVATE int levenshtein(const char *a, const char *b)
{
    size_t la = strlen(a);
    size_t lb = strlen(b);
    if(la > 128 || lb > 128) {
        return INT_MAX;
    }
    int prev[129], curr[129];
    for(size_t j = 0; j <= lb; j++) {
        prev[j] = (int)j;
    }
    for(size_t i = 1; i <= la; i++) {
        curr[0] = (int)i;
        for(size_t j = 1; j <= lb; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            int del = prev[j] + 1;
            int ins = curr[j-1] + 1;
            int sub = prev[j-1] + cost;
            int m = del < ins ? del : ins;
            curr[j] = m < sub ? m : sub;
        }
        memcpy(prev, curr, (lb + 1) * sizeof(int));
    }
    return prev[lb];
}

/***************************************************************************
 *  did-you-mean: if `typed` is not in the cache, print the closest match
 *  when it's within an edit-distance threshold. No-op on cache miss.
 ***************************************************************************/
PRIVATE void maybe_suggest_command(hgobj gobj, const char *typed)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!priv->commands_cache || empty_string(typed)) {
        return;
    }
    /* Skip if the command exists — error is about something else (auth, args). */
    if(json_object_get(priv->commands_cache, typed)) {
        return;
    }

    size_t typed_len = strlen(typed);
    int threshold = (int)(typed_len / 3);
    if(threshold < 2) threshold = 2;

    int best = INT_MAX;
    const char *best_name = NULL;
    const char *name;
    json_t *jn_cmd;
    json_object_foreach(priv->commands_cache, name, jn_cmd) {
        int d = levenshtein(typed, name);
        if(d < best) {
            best = d;
            best_name = name;
        }
    }

    if(best_name && best <= threshold) {
        printf("Did you mean '%s'?\n", best_name);
    }
}

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
        PRIVATE_DATA *priv = gobj_priv_data(gobj);
        printf("%sERROR %d: %s%s\n", On_Red BWhite, result, comment, Color_Off);
        maybe_suggest_command(gobj, priv->last_sent_command);
    } else {
        if(!empty_string(comment)) {
            /* Success comment in amber so it's not mistaken for data. */
            printf("%s%s%s\n", IYellow, comment, Color_Off);
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
    if(!priv->on_mirror_tty && priv->interactive) {
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
/***************************************************************************
 *  Local command handlers (invoked via the `!` prefix or the bare forms
 *  kept in ac_command).
 ***************************************************************************/
PRIVATE json_t *cmd_local_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    KW_INCREF(kw);
    json_t *jn_resp = gobj_build_cmds_doc(gobj, kw);
    return msg_iev_build_response(gobj, 0, jn_resp, 0, 0, kw);
}

PRIVATE json_t *cmd_local_history(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = gbuffer_create(512, 64 * 1024);
    if(priv->gobj_editline) {
        int n = editline_history_count(priv->gobj_editline);
        for(int i = 1; i <= n; i++) {
            const char *line = editline_history_get(priv->gobj_editline, i);
            if(line && *line) {
                gbuffer_printf(gbuf, "%5d  %s\n", i, line);
            }
        }
    }
    json_t *jn_resp = json_string(gbuffer_cur_rd_pointer(gbuf));
    gbuffer_decref(gbuf);
    return msg_iev_build_response(gobj, 0, jn_resp, 0, 0, kw);
}

PRIVATE json_t *cmd_local_clear_history(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->gobj_editline) {
        gobj_send_event(priv->gobj_editline, EV_CLEAR_HISTORY, 0, gobj);
    }
    return msg_iev_build_response(gobj, 0, json_string("History cleared"), 0, 0, kw);
}

PRIVATE json_t *cmd_local_exit(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->pending_commands) {
        json_array_clear(priv->pending_commands);
    }
    gobj_stop(gobj);
    return msg_iev_build_response(gobj, 0, 0, 0, 0, kw);
}

/***************************************************************************
 *  !source <file>: read `file` line by line, skip blank lines and lines
 *  starting with `#`, push the rest onto the pending queue so they're
 *  dispatched sequentially (each waits for the previous response).
 *  Lines themselves may contain `;` or the `-cmd` ignore-fail prefix.
 ***************************************************************************/
PRIVATE json_t *cmd_local_source(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *file = kw_get_str(gobj, kw, "file", "", 0);
    if(empty_string(file)) {
        return msg_iev_build_response(gobj, -1,
            json_string("Missing 'file' parameter"), 0, 0, kw);
    }
    FILE *fp = fopen(file, "r");
    if(!fp) {
        return msg_iev_build_response(gobj, -1,
            json_sprintf("Cannot open '%s': %s", file, strerror(errno)),
            0, 0, kw);
    }

    /*
     *  Collect the new commands in file order first, then splice them onto
     *  the FRONT of the pending queue so that `!source a.ycmd ; stats` runs
     *  every line of a.ycmd before `stats`, which matches user intuition.
     */
    json_t *new_cmds = json_array();
    char line[4096];
    while(fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while(len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = 0;
        }
        const char *start = line;
        while(*start == ' ' || *start == '\t') start++;
        if(*start == 0 || *start == '#') {
            continue;
        }
        split_commands_into_array(start, new_cmds);
    }
    fclose(fp);

    if(!priv->pending_commands) {
        priv->pending_commands = json_array();
    }
    size_t insert_at = 0;
    size_t i;
    json_t *v;
    json_array_foreach(new_cmds, i, v) {
        json_array_insert(priv->pending_commands, insert_at++, v);
    }
    int queued = (int)json_array_size(new_cmds);
    JSON_DECREF(new_cmds);

    return msg_iev_build_response(gobj, 0,
        json_sprintf("Sourced '%s' (%d command(s))", file, queued),
        0, 0, kw);
}

PRIVATE int list_history(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    /* Print the live in-memory history with 1-based indices that match
     * the !N bang expansion in ac_command. */
    if(priv->gobj_editline) {
        int n = editline_history_count(priv->gobj_editline);
        for(int i = 1; i <= n; i++) {
            const char *line = editline_history_get(priv->gobj_editline, i);
            if(line && *line) {
                printf("%5d  %s\n", i, line);
            }
        }
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
        struct passwd *_pw = yuneta_getpwuid(getuid());
        homedir = _pw ? _pw->pw_dir : "/root";
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
        struct passwd *_pw = yuneta_getpwuid(getuid());
        homedir = _pw ? _pw->pw_dir : "/root";
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
        struct passwd *_pw = yuneta_getpwuid(getuid());
        homedir = _pw ? _pw->pw_dir : "/root";
    }
    snprintf(path, pathsize, "%s/.yuneta/configs/", homedir);
    if(access(path, 0)!=0) {
        mkrdir(path, 0700);
    }
    snprintf(path, pathsize, "%s/.yuneta/configs/%s", homedir, name);

    const char *s = json_string_value(jn_content);
    if(s) {
        gbuffer_t *gbuf_bin = gbuffer_base64_to_binary(s, strlen(s));
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
PRIVATE int ac_on_token(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int result = (int)kw_get_int(gobj, kw, "result", -1, KW_REQUIRED);
    if(result < 0) {
        if(priv->verbose || priv->interactive) {
            const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
            printf("\n%s", comment);
            printf("\nAbort.\n");
        }
        printf("\n");
        exit(-1);
    } else {
        const char *jwt = kw_get_str(gobj, kw, "jwt", "", KW_REQUIRED);
        gobj_write_str_attr(gobj, "jwt", jwt);
        cmd_connect(gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Commands-cache: merge a `list-gobj-commands` response into priv->commands_cache.
 *  Input shape: array of {gclass, commands:[{command,description,parameters:[...]}, ...]}
 *  First sighting of a given name wins — keeps cache stable across repeated
 *  fetches (yuno first, service second).
 ***************************************************************************/
PRIVATE void merge_commands_into_cache(hgobj gobj, json_t *jn_raw_data)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!json_is_array(jn_raw_data)) {
        return;
    }
    if(!priv->commands_cache) {
        priv->commands_cache = json_object();
    }

    size_t idx;
    json_t *jn_gclass;
    json_array_foreach(jn_raw_data, idx, jn_gclass) {
        json_t *jn_commands = json_object_get(jn_gclass, "commands");
        if(!json_is_array(jn_commands)) {
            continue;
        }
        size_t jdx;
        json_t *jn_cmd;
        json_array_foreach(jn_commands, jdx, jn_cmd) {
            const char *name = json_string_value(json_object_get(jn_cmd, "command"));
            if(empty_string(name)) {
                continue;
            }
            if(!json_object_get(priv->commands_cache, name)) {
                json_object_set(priv->commands_cache, name, jn_cmd);
            }
        }
    }
}

/***************************************************************************
 *  Commands-cache: recognize the list-gobj-commands response shape
 ***************************************************************************/
PRIVATE BOOL is_commands_list_response(json_t *jn_data)
{
    if(!json_is_array(jn_data) || json_array_size(jn_data) == 0) {
        return FALSE;
    }
    json_t *first = json_array_get(jn_data, 0);
    return json_is_object(first)
        && json_object_get(first, "gclass") != NULL
        && json_object_get(first, "commands") != NULL;
}

/***************************************************************************
 *  Commands-cache: warm up with the configured service's commands.
 *  Routed through service=__yuno__ because list-gobj-commands is a command
 *  of C_YUNO, not of the target service.
 *
 *  Yuno-level commands (help, stats, list-gobj-commands, services, ...) are
 *  intentionally NOT fetched: invoking them requires adding service=__yuno__
 *  to the line, so having them in the default completion set would be
 *  misleading. Same story for other services inside the yuno
 *  (__input_side__, __output_side__, __top_side__, custom names) — invoke
 *  them manually with service=<name>.
 ***************************************************************************/
PRIVATE int request_commands_cache(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!priv->gobj_connector) {
        return -1;
    }

    const char *wanted_service = gobj_read_str_attr(gobj, "yuno_service");
    if(empty_string(wanted_service)) {
        wanted_service = "__default_service__";
    }
    json_t *kw = json_pack("{s:s, s:s, s:i, s:b}",
        "service", "__yuno__",
        "gobj_name", wanted_service,
        "details", 1,
        "bottoms", 1
    );
    priv->pending_cache_fetches++;
    json_t *webix = gobj_command(priv->gobj_connector, "list-gobj-commands", kw, gobj);
    if(webix) {
        /* Synchronous response: consume here, no display. */
        json_t *jn_data = kw_get_dict_value(gobj, webix, "data", 0, 0);
        if(is_commands_list_response(jn_data)) {
            merge_commands_into_cache(gobj, jn_data);
        }
        priv->pending_cache_fetches--;
        JSON_DECREF(webix)
    }
    return 0;
}

/***************************************************************************
 *  Lookup a local command in local_command_table by name or alias.
 *  Returns the sdata_desc_t entry or NULL.
 ***************************************************************************/
PRIVATE const sdata_desc_t *find_local_command(const char *name, size_t namelen)
{
    for(const sdata_desc_t *p = local_command_table; p->name; p++) {
        if(empty_string(p->name)) {
            continue;
        }
        if(strlen(p->name) == namelen && strncmp(p->name, name, namelen) == 0) {
            return p;
        }
        if(p->alias) {
            for(const char **a = p->alias; *a; a++) {
                if(strlen(*a) == namelen && strncmp(*a, name, namelen) == 0) {
                    return p;
                }
            }
        }
    }
    return NULL;
}

/***************************************************************************
 *  Tab-completion callback: first word -> command names;
 *  after <cmd> <space> ... -> parameter names ("param=")
 *  Handles both remote commands and local '!' commands.
 ***************************************************************************/
PRIVATE void ycommand_completion_cb(
    hgobj editline_gobj,
    const char *buf,
    editline_completions_t *lc,
    void *user_data
) {
    /* user_data is the ycommand gobj registered from ac_on_open. */
    hgobj gobj = (hgobj)user_data;
    if(!gobj || !buf) {
        return;
    }
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    static const char *local_cmds[] = {"help", "history", "exit", "quit", NULL};
    static const char *local_descs[] = {
        "Show local help",
        "Show command history",
        "Exit ycommand",
        "Exit ycommand",
    };
    /* Skip leading spaces and the optional '*' form-mode prefix. */
    const char *start = buf;
    while(*start == ' ' || *start == '\t') start++;
    const char *body = (*start == '*') ? start + 1 : start;
    size_t prefix_len = (size_t)(body - buf);
    char candidate[1024];

    /* Local ('!' prefix) branch: enumerate the local_command_table. */
    if(body[0] == '!') {
        const char *after_bang = body + 1;
        size_t bang_prefix_len = prefix_len + 1;  /* keep the '!' in candidates */
        const char *first_space_local = strchr(after_bang, ' ');

        if(!first_space_local) {
            /* Completing the local command name. */
            size_t token_len = strlen(after_bang);
            for(const sdata_desc_t *p = local_command_table; p->name; p++) {
                if(empty_string(p->name)) {
                    continue;
                }
                if(strncmp(p->name, after_bang, token_len) == 0) {
                    snprintf(candidate, sizeof(candidate), "%.*s%s",
                        (int)bang_prefix_len, buf, p->name);
                    editline_add_completion(lc, candidate, p->description);
                }
            }
            return;
        }

        /* "!cmd <space> ..." → walk the command's schema for parameter names. */
        size_t cmd_len = (size_t)(first_space_local - after_bang);
        const sdata_desc_t *cmd_entry = find_local_command(after_bang, cmd_len);
        if(!cmd_entry || !cmd_entry->schema) {
            return;
        }
        const char *last_token = strrchr(buf, ' ');
        last_token = last_token ? last_token + 1 : buf;
        size_t ltoken_len = strlen(last_token);
        size_t head_len = (size_t)(last_token - buf);

        const char *eq = strchr(last_token, '=');
        if(eq) {
            /* Value completion: look up the param's type in the schema and
             * offer known values (booleans for now). */
            size_t pname_len = (size_t)(eq - last_token);
            if(pname_len == 0 || pname_len >= 64) {
                return;
            }
            char pname[64];
            memcpy(pname, last_token, pname_len);
            pname[pname_len] = 0;
            const char *vprefix = eq + 1;
            size_t vlen = strlen(vprefix);
            size_t up_to_eq = head_len + pname_len + 1;
            for(const sdata_desc_t *pp = cmd_entry->schema; pp->name; pp++) {
                if(strcmp(pp->name, pname) == 0) {
                    if(DTP_IS_BOOLEAN(pp->type)) {
                        static const char *bool_vals[] = {"true", "false", NULL};
                        for(int v = 0; bool_vals[v]; v++) {
                            if(strncmp(bool_vals[v], vprefix, vlen) == 0) {
                                snprintf(candidate, sizeof(candidate), "%.*s%s",
                                    (int)up_to_eq, buf, bool_vals[v]);
                                editline_add_completion(lc, candidate, NULL);
                            }
                        }
                    }
                    break;
                }
            }
            return;
        }

        for(const sdata_desc_t *pp = cmd_entry->schema; pp->name; pp++) {
            if(empty_string(pp->name)) {
                continue;
            }
            if(line_has_param(body, pp->name)) {
                continue;
            }
            if(strncmp(pp->name, last_token, ltoken_len) == 0) {
                snprintf(candidate, sizeof(candidate), "%.*s%s=",
                    (int)head_len, buf, pp->name);
                editline_add_completion(lc, candidate, pp->description);
            }
        }
        return;
    }

    /* Remote command branch (existing behaviour). */
    const char *first_space = strchr(body, ' ');

    if(!first_space) {
        /* Completing the command name itself. */
        size_t token_len = strlen(body);

        for(int i = 0; local_cmds[i]; i++) {
            if(strncmp(local_cmds[i], body, token_len) == 0) {
                snprintf(candidate, sizeof(candidate), "%.*s%s",
                    (int)prefix_len, buf, local_cmds[i]);
                editline_add_completion(lc, candidate, local_descs[i]);
            }
        }
        if(priv->commands_cache) {
            const char *name;
            json_t *jn_cmd;
            json_object_foreach(priv->commands_cache, name, jn_cmd) {
                if(strncmp(name, body, token_len) == 0) {
                    const char *desc = json_string_value(
                        json_object_get(jn_cmd, "description"));
                    snprintf(candidate, sizeof(candidate), "%.*s%s",
                        (int)prefix_len, buf, name);
                    editline_add_completion(lc, candidate, desc);
                }
            }
        }
        return;
    }

    /* We have "<cmd> ...". Lookup the command in the cache. */
    if(!priv->commands_cache) {
        return;
    }
    size_t cmd_len = (size_t)(first_space - body);
    char cmd_name[128];
    if(cmd_len == 0 || cmd_len >= sizeof(cmd_name)) {
        return;
    }
    memcpy(cmd_name, body, cmd_len);
    cmd_name[cmd_len] = 0;

    json_t *jn_cmd = json_object_get(priv->commands_cache, cmd_name);
    if(!jn_cmd) {
        return;
    }
    json_t *jn_params = json_object_get(jn_cmd, "parameters");
    if(!json_is_array(jn_params)) {
        return;
    }

    const char *last_token = strrchr(buf, ' ');
    last_token = last_token ? last_token + 1 : buf;
    size_t token_len = strlen(last_token);
    size_t head_len = (size_t)(last_token - buf);

    const char *eq = strchr(last_token, '=');
    if(eq) {
        /* Value completion: look up the param's type and offer known
         * values (booleans for now). */
        size_t pname_len = (size_t)(eq - last_token);
        if(pname_len == 0 || pname_len >= 64) {
            return;
        }
        char pname[64];
        memcpy(pname, last_token, pname_len);
        pname[pname_len] = 0;
        const char *vprefix = eq + 1;
        size_t vlen = strlen(vprefix);
        size_t up_to_eq = head_len + pname_len + 1;

        size_t idx;
        json_t *jn_p;
        json_array_foreach(jn_params, idx, jn_p) {
            const char *ppname = json_string_value(
                json_object_get(jn_p, "parameter"));
            if(!ppname || strcmp(ppname, pname) != 0) {
                continue;
            }
            const char *ptype = json_string_value(
                json_object_get(jn_p, "type"));
            if(ptype && strcmp(ptype, "boolean") == 0) {
                static const char *bool_vals[] = {"true", "false", NULL};
                for(int v = 0; bool_vals[v]; v++) {
                    if(strncmp(bool_vals[v], vprefix, vlen) == 0) {
                        snprintf(candidate, sizeof(candidate), "%.*s%s",
                            (int)up_to_eq, buf, bool_vals[v]);
                        editline_add_completion(lc, candidate, NULL);
                    }
                }
            }
            break;
        }
        return;
    }

    size_t idx;
    json_t *jn_p;
    json_array_foreach(jn_params, idx, jn_p) {
        const char *pname = json_string_value(json_object_get(jn_p, "parameter"));
        if(empty_string(pname)) {
            continue;
        }
        /* Skip params already present on the line (same filter as the hint). */
        if(line_has_param(body, pname)) {
            continue;
        }
        if(strncmp(pname, last_token, token_len) == 0) {
            const char *pdesc = json_string_value(
                json_object_get(jn_p, "description"));
            snprintf(candidate, sizeof(candidate), "%.*s%s=",
                (int)head_len, buf, pname);
            editline_add_completion(lc, candidate, pdesc);
        }
    }
}

/***************************************************************************
 *  Map a yuno sdata type to a short label for the hint.
 *  The remote command cache returns types as strings ("string", "integer",
 *  ...); the local command_table uses raw DTP_* bytes. One helper per form.
 ***************************************************************************/
PRIVATE const char *short_type_label(const char *type)
{
    if(empty_string(type))                return "";
    if(strcmp(type, "string") == 0)       return "str";
    if(strcmp(type, "integer") == 0)      return "int";
    if(strcmp(type, "boolean") == 0)      return "bool";
    return type;
}

PRIVATE const char *short_dtp_label(uint8_t dtp)
{
    if(DTP_IS_STRING(dtp))  return "str";
    if(DTP_IS_BOOLEAN(dtp)) return "bool";
    if(DTP_IS_INTEGER(dtp)) return "int";
    if(DTP_IS_REAL(dtp))    return "real";
    if(DTP_IS_JSON(dtp))    return "json";
    return "";
}

/***************************************************************************
 *  Return TRUE if `buf` already contains a "<pname>=" token. The buffer
 *  always starts with the command name, so the param name must be preceded
 *  by whitespace — this avoids matches that collide with the command name
 *  or with the value side of another param.
 ***************************************************************************/
PRIVATE BOOL line_has_param(const char *buf, const char *pname)
{
    size_t plen = strlen(pname);
    const char *p = buf;
    while((p = strstr(p, pname)) != NULL) {
        BOOL preceded_by_space = (p != buf) && (p[-1] == ' ' || p[-1] == '\t');
        if(preceded_by_space && p[plen] == '=') {
            return TRUE;
        }
        p += plen;
    }
    return FALSE;
}

/***************************************************************************
 *  Hints callback: once the user has typed "<cmd> ", show the remaining
 *  parameters in gray (required as <name=type>, optional as [name=type]).
 *  Parameters already present on the line are skipped.
 ***************************************************************************/
PRIVATE char *ycommand_hints_cb(
    hgobj editline_gobj,
    const char *buf,
    int *out_color,
    int *out_bold,
    void *user_data
) {
    hgobj gobj = (hgobj)user_data;
    if(!gobj || empty_string(buf)) {
        return NULL;
    }
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /* Skip leading spaces and an optional '*' form-mode prefix. */
    const char *body = buf;
    while(*body == ' ' || *body == '\t') body++;
    if(*body == '*') body++;

    BOOL is_local = (body[0] == '!');
    if(is_local) {
        body++;  /* skip '!' for the command-name / param scan */
    }

    /* Hint only after the command name has been completed (first space seen). */
    const char *first_space = strchr(body, ' ');
    if(!first_space) {
        return NULL;
    }
    size_t cmd_len = (size_t)(first_space - body);
    if(cmd_len == 0) {
        return NULL;
    }

    gbuffer_t *gbuf = gbuffer_create(128, 4 * 1024);

    if(is_local) {
        const sdata_desc_t *cmd_entry = find_local_command(body, cmd_len);
        if(!cmd_entry || !cmd_entry->schema) {
            gbuffer_decref(gbuf);
            return NULL;
        }
        for(const sdata_desc_t *pp = cmd_entry->schema; pp->name; pp++) {
            if(empty_string(pp->name)) {
                continue;
            }
            if(line_has_param(body, pp->name)) {
                continue;
            }
            BOOL required = (pp->flag & SDF_REQUIRED) ? TRUE : FALSE;
            gbuffer_printf(gbuf, " %c%s=%s%c",
                required ? '<' : '[',
                pp->name,
                short_dtp_label(pp->type),
                required ? '>' : ']'
            );
        }
    } else {
        if(!priv->commands_cache) {
            gbuffer_decref(gbuf);
            return NULL;
        }
        char cmd_name[128];
        if(cmd_len >= sizeof(cmd_name)) {
            gbuffer_decref(gbuf);
            return NULL;
        }
        memcpy(cmd_name, body, cmd_len);
        cmd_name[cmd_len] = 0;
        json_t *jn_cmd = json_object_get(priv->commands_cache, cmd_name);
        if(!jn_cmd) {
            gbuffer_decref(gbuf);
            return NULL;
        }
        json_t *jn_params = json_object_get(jn_cmd, "parameters");
        if(!json_is_array(jn_params) || json_array_size(jn_params) == 0) {
            gbuffer_decref(gbuf);
            return NULL;
        }

        /* Build "[name=type] <req=type> ..." for params not yet on the line. */
        size_t idx;
        json_t *jn_p;
        json_array_foreach(jn_params, idx, jn_p) {
            const char *pname = json_string_value(json_object_get(jn_p, "parameter"));
            if(empty_string(pname)) {
                continue;
            }
            if(line_has_param(body, pname)) {
                continue;
            }
            const char *ptype = json_string_value(json_object_get(jn_p, "type"));
            const char *flag = json_string_value(json_object_get(jn_p, "flag"));
            BOOL required = (flag && strstr(flag, "SDF_REQUIRED") != NULL);
            gbuffer_printf(gbuf, " %c%s=%s%c",
                required ? '<' : '[',
                pname,
                short_type_label(ptype),
                required ? '>' : ']'
            );
        }
    }

    if(gbuffer_leftbytes(gbuf) == 0) {
        gbuffer_decref(gbuf);
        return NULL;
    }

    /* Caller (c_editline) gives the returned pointer to free_cb — strdup it. */
    char *hint = gbmem_strdup(gbuffer_cur_rd_pointer(gbuf));
    gbuffer_decref(gbuf);
    if(out_color) *out_color = 90;  /* bright black / gray */
    if(out_bold)  *out_bold  = 0;
    return hint;
}

PRIVATE void ycommand_free_hint_cb(char *hint, void *user_data)
{
    gbmem_free(hint);
}

/***************************************************************************
 *  Execute batch of input parameters when the route is opened.
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *yuno_role = kw_get_str(gobj, kw, "remote_yuno_role", "", 0);
    const char *yuno_name = kw_get_str(gobj, kw, "remote_yuno_name", "", 0);

    if(priv->verbose || priv->interactive) {
        printf("Connected to '%s^%s', url:'%s'.\n",
            yuno_role,
            yuno_name,
            gobj_read_str_attr(gobj, "url")
        );
    }
    gobj_write_pointer_attr(gobj, "gobj_connector", src);

    if(priv->interactive && priv->gobj_editline) {
        /*
         *  Update the prompt to reflect the peer we're talking to, so the
         *  user can tell at a glance where their commands will land (useful
         *  when hopping between agent / controlcenter / other yunos).
         *  Plain text only — ANSI colors would fool the prompt-width logic
         *  in refreshLine (strlen counts the escape bytes).
         */
        char new_prompt[128];
        if(empty_string(yuno_name)) {
            snprintf(new_prompt, sizeof(new_prompt), "%s> ", yuno_role);
        } else {
            snprintf(new_prompt, sizeof(new_prompt), "%s^%s> ",
                yuno_role, yuno_name);
        }
        gobj_write_str_attr(priv->gobj_editline, "prompt", new_prompt);

        editline_set_completion_callback(
            priv->gobj_editline, ycommand_completion_cb, gobj
        );
        editline_set_hints_callback(
            priv->gobj_editline,
            ycommand_hints_cb,
            ycommand_free_hint_cb,
            gobj
        );
        /*
         *  ycommand can hop between different yunos (agent, controlcenter, ...)
         *  and each exposes its own command set, so we always ask the peer at
         *  connect time and keep the map only in memory for this session.
         */
        request_commands_cache(gobj);
    }

    const char *command = gobj_read_str_attr(gobj, "command");
    if(priv->interactive) {
        if(!empty_string(command)) {
            do_command(gobj, command);
        } else {
            printf("Type '!help' for local help, '!history' for history, !N to repeat.\n");
            clear_input_line(gobj);
        }
    } else {
        if(empty_string(command)) {
            /*
             *  Non-interactive with no -c/positional: if stdin is a pipe,
             *  read commands from it one per line (blank / '#' lines are
             *  skipped) and drain them sequentially — each waits for the
             *  previous response, stop on error unless the line was
             *  prefixed with '-'. This gives ycommand basic pipe-style
             *  scripting:  `cat batch.ycmd | ycommand -u ws://...`
             */
            if(!isatty(STDIN_FILENO)) {
                char line[4096];
                while(fgets(line, sizeof(line), stdin)) {
                    size_t len = strlen(line);
                    while(len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
                        line[--len] = 0;
                    }
                    const char *s = line;
                    while(*s == ' ' || *s == '\t') s++;
                    if(*s == 0 || *s == '#') {
                        continue;
                    }
                    split_commands_into_queue(gobj, s);
                }
                if(priv->pending_commands
                   && json_array_size(priv->pending_commands) > 0) {
                    run_next_pending(gobj);
                } else {
                    gobj_stop(priv->gobj_remote_agent);
                }
            } else {
                printf("What command?\n");
                gobj_stop(priv->gobj_remote_agent);
            }
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
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_write_pointer_attr(gobj, "gobj_connector", 0);
    if(priv->verbose || priv->interactive) {
        const char *comment = kw_get_str(gobj, kw, "comment", 0, 0);
        if(comment) {
            printf("\nIdentity card refused, cause: %s", comment);
        }
        printf("\nDisconnected.\n");
    }

    try_to_stop_yevents(gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Split `text` into atomic commands on unquoted, top-level `;` and append
 *  each non-empty chunk to the `target` json array. Brace depth and quote
 *  state are tracked so `;` inside "...", '...' or {...} is treated as
 *  literal (needed for JSON-valued parameters like kw={"a":1}).
 ***************************************************************************/
PRIVATE void split_commands_into_array(const char *text, json_t *target)
{
    if(!text || !target) {
        return;
    }
    const char *start = text;
    int brace_depth = 0;
    char in_quote = 0;
    for(const char *p = text; ; p++) {
        if(in_quote) {
            if(*p == 0) break;
            if(*p == '\\' && p[1]) { p++; continue; }
            if(*p == in_quote) in_quote = 0;
            continue;
        }
        if(*p == '"' || *p == '\'') {
            in_quote = *p;
            continue;
        }
        if(*p == '{') {
            brace_depth++;
            continue;
        }
        if(*p == '}' && brace_depth > 0) {
            brace_depth--;
            continue;
        }
        if((*p == ';' && brace_depth == 0) || *p == 0) {
            const char *s = start;
            const char *e = p;
            while(s < e && (*s == ' ' || *s == '\t')) s++;
            while(e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;
            if(e > s) {
                size_t n = (size_t)(e - s);
                char *tmp = gbmem_malloc(n + 1);
                if(tmp) {
                    memcpy(tmp, s, n);
                    tmp[n] = 0;
                    json_array_append_new(target, json_string(tmp));
                    gbmem_free(tmp);
                }
            }
            if(*p == 0) break;
            start = p + 1;
        }
    }
}

PRIVATE void split_commands_into_queue(hgobj gobj, const char *text)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!priv->pending_commands) {
        priv->pending_commands = json_array();
    }
    split_commands_into_array(text, priv->pending_commands);
}

/***************************************************************************
 *  Drain priv->pending_commands in order. Each entry goes through
 *  exec_one_command(); on sync success we loop to the next, on sync error
 *  we clear the queue (unless the entry was prefixed with '-') and on
 *  async dispatch we return and wait for ac_command_answer to resume us.
 ***************************************************************************/
PRIVATE void run_next_pending(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    while(priv->pending_commands && json_array_size(priv->pending_commands) > 0) {
        json_t *jn_cmd = json_array_get(priv->pending_commands, 0);
        char *cmd_str = gbmem_strdup(json_string_value(jn_cmd));
        json_array_remove(priv->pending_commands, 0);
        int ret = exec_one_command(gobj, cmd_str);
        gbmem_free(cmd_str);
        if(ret == 1) {
            /* Async dispatch: ac_command_answer will call us again. */
            return;
        }
        if(ret == -1) {
            /* Sync error and not ignore-fail: drop the rest of the queue. */
            json_array_clear(priv->pending_commands);
            return;
        }
        /* ret == 0: sync success, continue to next. */
    }
}

/***************************************************************************
 *  Run a single command line end-to-end. Handles the bang/history
 *  expansion, the bare `exit` / `quit` / `history` intercepts, local-table
 *  vs remote routing and the sync/async response split.
 *  Returns 0 (sync done), 1 (async pending, wait for EV_MT_COMMAND_ANSWER)
 *  or -1 (sync error; caller should drop remaining queue unless this
 *  command was prefixed with '-').
 ***************************************************************************/
PRIVATE int exec_one_command(hgobj gobj, const char *cmdline)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    while(*cmdline == ' ' || *cmdline == '\t') cmdline++;
    if(empty_string(cmdline)) {
        return 0;
    }

    /* ybatch convention: a leading '-' on the command keeps the batch
     * going when this one fails. */
    BOOL ignore_fail = FALSE;
    if(*cmdline == '-') {
        ignore_fail = TRUE;
        cmdline++;
        while(*cmdline == ' ' || *cmdline == '\t') cmdline++;
    }

    /* History expansion (bash-style) — only for purely numeric/`!` forms. */
    char expanded[1024] = {0};
    if(cmdline[0] == '!' && cmdline[1] && priv->gobj_editline) {
        const char *spec = cmdline + 1;
        BOOL is_history_expansion = FALSE;
        const char *found = NULL;
        int n = editline_history_count(priv->gobj_editline);
        if(spec[0] == '!' && spec[1] == 0) {
            is_history_expansion = TRUE;
            if(n > 0) found = editline_history_get(priv->gobj_editline, n);
        } else if(isdigit((unsigned char)spec[0])) {
            is_history_expansion = TRUE;
            int idx = atoi(spec);
            if(idx > 0) found = editline_history_get(priv->gobj_editline, idx);
        }
        if(is_history_expansion) {
            if(!found) {
                printf("%s%s: event not found%s\n",
                    On_Red BWhite, cmdline, Color_Off);
                clear_input_line(gobj);
                return ignore_fail ? 0 : -1;
            }
            printf("%s\n", found);
            snprintf(expanded, sizeof(expanded), "%s", found);
            cmdline = expanded;
        }
    }

    /* Bare local intercepts kept for muscle memory. */
    if(strcasecmp(cmdline, "exit") == 0 || strcasecmp(cmdline, "quit") == 0) {
        if(priv->pending_commands) {
            json_array_clear(priv->pending_commands);
        }
        gobj_stop(gobj);
        return 0;
    }
    if(strcasecmp(cmdline, "history") == 0) {
        list_history(gobj);
        return 0;
    }

    char comment[512] = {0};
    gbuffer_t *gbuf_parsed_command = replace_cli_vars(cmdline, comment, sizeof(comment));
    if(!gbuf_parsed_command) {
        printf("%s%s%s\n", On_Red BWhite, cmdline, Color_Off);
        clear_input_line(gobj);
        return ignore_fail ? 0 : -1;
    }
    char *xcmd = gbuffer_cur_rd_pointer(gbuf_parsed_command);
    json_t *kw_command = json_object();
    if(*xcmd == '*') {
        xcmd++;
        kw_set_subdict_value(gobj, kw_command, "__md_iev__",
            "display_mode", json_string("form"));
    }
    if(strstr(xcmd, "open-console")) {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        int cx = w.ws_col;
        int cy = w.ws_row;
        kw_set_dict_value(gobj, kw_command, "cx", json_integer(cx));
        kw_set_dict_value(gobj, kw_command, "cy", json_integer(cy));
    }

    BOOL local_cmd = FALSE;
    if(*xcmd == '!') {
        xcmd++;
        while(*xcmd == ' ' || *xcmd == '\t') xcmd++;
        local_cmd = TRUE;
    }

    {
        const char *sp = xcmd;
        while(*sp && *sp != ' ' && *sp != '\t') sp++;
        size_t cmd_len = (size_t)(sp - xcmd);
        if(cmd_len >= sizeof(priv->last_sent_command)) {
            cmd_len = sizeof(priv->last_sent_command) - 1;
        }
        memcpy(priv->last_sent_command, xcmd, cmd_len);
        priv->last_sent_command[cmd_len] = 0;
    }

    json_t *webix = 0;
    if(local_cmd) {
        webix = gobj_command(gobj, xcmd, kw_command, gobj);
    } else if(gobj_in_this_state(gobj, ST_CONNECTED)) {
        webix = gobj_command(priv->gobj_connector, xcmd, kw_command, gobj);
    } else {
        printf("\n%s%s%s\n", On_Red BWhite, "No connection", Color_Off);
        clear_input_line(gobj);
        JSON_DECREF(kw_command)
        gbuffer_decref(gbuf_parsed_command);
        return ignore_fail ? 0 : -1;
    }
    gbuffer_decref(gbuf_parsed_command);

    if(webix) {
        int result = (int)kw_get_int(gobj, webix, "result", 0, 0);
        display_webix_result(gobj, webix);  /* consumes webix */
        if(result < 0) {
            return ignore_fail ? 0 : -1;
        }
        return 0;
    }

    /* Async: the response will arrive via EV_MT_COMMAND_ANSWER. */
    priv->current_ignore_fail = ignore_fail;
    printf("\n"); fflush(stdout);
    return 1;
}

/***************************************************************************
 *  HACK Este evento solo puede venir de GCLASS_EDITLINE
 *
 *  Just read the text off the source (editline or kw in non-interactive
 *  mode), split it into atomic commands on top-level ';' and start draining
 *  the queue. exec_one_command() does the real work per command.
 ***************************************************************************/
PRIVATE int ac_command(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_input_command = json_object();
    if(src != gobj) {
        gobj_send_event(src, EV_GETTEXT, json_incref(kw_input_command), gobj); // HACK EV_GETTEXT is EVF_KW_WRITING
    } else {
        // Non-interactive: text is passed directly in kw
        json_object_set_new(kw_input_command, "text",
            json_string(kw_get_str(gobj, kw, "text", "", 0)));
    }
    const char *command = kw_get_str(gobj, kw_input_command, "text", 0, 0);

    if(empty_string(command)) {
        clear_input_line(gobj);
        KW_DECREF(kw_input_command);
        KW_DECREF(kw);
        return 0;
    }
    if(priv->on_mirror_tty) {
        KW_DECREF(kw_input_command);
        KW_DECREF(kw);
        return 0;
    }

    split_commands_into_queue(gobj, command);
    run_next_pending(gobj);

    KW_DECREF(kw_input_command);
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Command received.
 ***************************************************************************/
PRIVATE int ac_command_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Intercept the silent list-gobj-commands responses used to warm the cache.
     *  While at least one warm-up is pending, swallow anything that looks like a
     *  list-gobj-commands answer (success merges, error is ignored). Unrelated
     *  answers — e.g. a user command's response racing the warm-up — still go
     *  to the normal display path below.
     */
    if(priv->pending_cache_fetches > 0) {
        json_t *jn_data = kw_get_dict_value(gobj, kw, "data", 0, 0);
        BOOL is_cache = is_commands_list_response(jn_data);
        int result = (int)kw_get_int(gobj, kw, "result", 0, 0);
        /* A pure error response has no data but is still the warm-up's answer. */
        if(is_cache || (result != 0 && jn_data == NULL)) {
            if(is_cache) {
                merge_commands_into_cache(gobj, jn_data);
            }
            priv->pending_cache_fetches--;
            if(priv->interactive) {
                clear_input_line(gobj);
            }
            KW_DECREF(kw);
            return 0;
        }
    }

    /*
     *  Before running the normal display paths, capture the result+flag
     *  into locals so we can resume (or drop) the queue after display.
     *  ownership of `kw` is passed to display_webix_result.
     */
    int __answer_result = (int)kw_get_int(gobj, kw, "result", 0, 0);
    BOOL __ignore_fail = priv->current_ignore_fail;
    priv->current_ignore_fail = FALSE;

    /*
     *  Unified rendering: use display_webix_result() in both modes so the
     *  non-interactive output also honours the schema → table pipeline
     *  (and the '*' prefix still forces raw-JSON form via
     *  __md_iev__.display_mode, set in exec_one_command).
     */
    int r = display_webix_result(gobj, kw);  /* consumes kw */

    if(priv->pending_commands && json_array_size(priv->pending_commands) > 0) {
        if(__answer_result < 0 && !__ignore_fail) {
            json_array_clear(priv->pending_commands);
        } else {
            run_next_pending(gobj);
        }
    }

    if(!priv->interactive) {
        gobj_set_exit_code(__answer_result);
        /* Schedule the shutdown timeout only when we're actually done —
         * if more queued commands are still being drained, wait for them. */
        if(!priv->pending_commands
           || json_array_size(priv->pending_commands) == 0) {
            set_timeout(priv->timer, priv->wait * 1000);
        }
    }

    return r;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_edit_config(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int result = (int)kw_get_int(gobj, kw, "result", -1, 0);
    const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
    if(result != 0) {
        printf("%sERROR %d: %s%s\n", On_Red BWhite, result, comment, Color_Off);
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }
    json_t *record = json_array_get(kw_get_dict_value(gobj, kw, "data", 0, 0), 0);
    if(!record) {
        printf("%sERROR %d: %s:%s%s\n",
            On_Red BWhite, result, __FUNCTION__, "Internal error, no data", Color_Off
        );
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }

    const char *id = kw_get_str(gobj, record, "id", 0, 0);
    if(!id) {
        printf("%sERROR %d: %s:%s%s\n",
            On_Red BWhite, result, __FUNCTION__, "Internal error, no id", Color_Off
        );
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }

    json_t *jn_content = kw_get_dict_value(gobj, record, "zcontent", 0, 0);
    if(!jn_content) {
        printf("%sERROR %d: %s:%s%s\n",
            On_Red BWhite, result, __FUNCTION__, "Internal error, no content", Color_Off
        );
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }

    JSON_INCREF(jn_content);
    char path[NAME_MAX];

    save_local_json(gobj, path, sizeof(path), id, jn_content);
    //log_debug_printf("save_local_json %s", path);
    edit_json(gobj, path);

    size_t flags = 0;
    json_error_t error;
    json_t *jn_new_content = json_load_file(path, flags, &error);
    if(!jn_new_content) {
        printf("%sERROR %d: Bad json format in '%s' source. Line %d, Column %d, Error '%s'%s\n",
            On_Red BWhite,
            result,
            path,
            error.line,
            error.column,
            error.text,
            Color_Off
        );
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }
    JSON_DECREF(jn_new_content);

    char upgrade_command[512];
    snprintf(upgrade_command, sizeof(upgrade_command),
        "create-config id='%s' content64=$$(%s) ",
        id,
        path
    );

    json_t *kw_line = json_object();
    json_object_set_new(kw_line, "text", json_string(upgrade_command));
    gobj_send_event(priv->gobj_editline, EV_SETTEXT, kw_line, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_view_config(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = (int)kw_get_int(gobj, kw, "result", -1, 0);
    const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
    if(result != 0) {
        printf("%sERROR %d: %s%s\n", On_Red BWhite, result, comment, Color_Off);
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }
    json_t *record = json_array_get(kw_get_dict_value(gobj, kw, "data", 0, 0), 0);
    if(!record) {
        printf("%sERROR %d: %s:%s%s\n",
            On_Red BWhite, result, __FUNCTION__, "Internal error, no data", Color_Off
        );
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }

    json_t *jn_content = kw_get_dict_value(gobj, record, "zcontent", 0, 0);
    if(!jn_content) {
        printf("%sERROR %d: %s:%s%s\n",
            On_Red BWhite, result, __FUNCTION__, "Internal error, no content", Color_Off
        );
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }

    const char *name = kw_get_str(gobj, record, "name", "__temporal__", 0);
    JSON_INCREF(jn_content);
    char path[NAME_MAX];

    save_local_json(gobj, path, sizeof(path), name, jn_content);
    //log_debug_printf("save_local_json %s", path);
    edit_json(gobj, path);

    clear_input_line(gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_read_json(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = (int)kw_get_int(gobj, kw, "result", -1, 0);
    const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
    if(result != 0) {
        printf("%sERROR %d: %s%s\n", On_Red BWhite, result, comment, Color_Off);
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }
    json_t *record = kw_get_dict_value(gobj, kw, "data", 0, 0);
    if(!record) {
        printf("%sERROR %d: %s:%s%s\n",
            On_Red BWhite, result, __FUNCTION__, "Internal error, no data", Color_Off
        );
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }

    json_t *jn_content = kw_get_dict_value(gobj, record, "zcontent", 0, 0);
    if(!jn_content) {
        printf("%sERROR %d: %s:%s%s\n",
            On_Red BWhite, result, __FUNCTION__, "Internal error, no content", Color_Off
        );
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }

    const char *name = kw_get_str(gobj, record, "name", "__temporal__", 0);
    JSON_INCREF(jn_content);
    char path[NAME_MAX];

    save_local_json(gobj, path, sizeof(path), name, jn_content);
    //log_debug_printf("save_local_json %s", path);
    edit_json(gobj, path);

    clear_input_line(gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_read_file(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = (int)kw_get_int(gobj, kw, "result", -1, 0);
    const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
    if(result != 0) {
        printf("%sERROR %d: %s%s\n", On_Red BWhite, result, comment, Color_Off);
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }
    json_t *record = kw_get_dict_value(gobj, kw, "data", 0, 0);
    if(!record) {
        printf("%sERROR %d: %s:%s%s\n",
            On_Red BWhite, result, __FUNCTION__, "Internal error, no data", Color_Off
        );
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }

    json_t *jn_content = kw_get_dict_value(gobj, record, "zcontent", 0, 0);
    if(!jn_content) {
        printf("%sERROR %d: %s:%s%s\n",
            On_Red BWhite, result, __FUNCTION__, "Internal error, no content", Color_Off
        );
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }

    const char *name = kw_get_str(gobj, record, "name", "__temporal__", 0);
    JSON_INCREF(jn_content);
    char path[NAME_MAX];

    save_local_string(gobj, path, sizeof(path), name, jn_content);
    //log_debug_printf("save_local_json %s", path);
    edit_json(gobj, path);

    clear_input_line(gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_read_binary_file(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = (int)kw_get_int(gobj, kw, "result", -1, 0);
    const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
    if(result != 0) {
        printf("%sERROR %d: %s%s\n", On_Red BWhite, result, comment, Color_Off);
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }
    json_t *record = kw_get_dict_value(gobj, kw, "data", 0, 0);
    if(!record) {
        printf("%sERROR %d: %s:%s%s\n",
            On_Red BWhite, result, __FUNCTION__, "Internal error, no data", Color_Off
        );
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }

    json_t *jn_content = kw_get_dict_value(gobj, record, "content64", 0, 0);
    if(!jn_content) {
        printf("%sERROR %d: %s:%s%s\n",
            On_Red BWhite, result, __FUNCTION__, "Internal error, no content", Color_Off
        );
        clear_input_line(gobj);
        if(!priv->interactive) {
            gobj_set_exit_code(result);
            set_timeout(priv->timer, priv->wait * 1000);
        }
        KW_DECREF(kw);
        return 0;
    }

    const char *name = kw_get_str(gobj, record, "name", "__temporal__", 0);
    JSON_INCREF(jn_content);
    char path[NAME_MAX];
    save_local_base64(gobj, path, sizeof(path), name, jn_content);

    clear_input_line(gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_screen_ctrl(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    SWITCHS(event) {
        CASES(EV_CLRSCR)
            /* Clear + position cursor at top-left; otherwise the prompt
             * is redrawn below whatever was on the last line. */
            printf(Clear_Screen);
            printf(Cursor_Position, 1, 1);
            fflush(stdout);
            if(priv->gobj_editline) {
                gobj_send_event(priv->gobj_editline, EV_PAINT, 0, gobj);
            }
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
PRIVATE int ac_tty_mirror_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_data = kw_get_dict(gobj, kw, "data", 0, KW_REQUIRED);
    const char *tty_name = kw_get_str(gobj, jn_data, "name", "", KW_REQUIRED);
    snprintf(priv->mirror_tty_name, sizeof(priv->mirror_tty_name), "%s", tty_name);

    const char *tty_uuid = kw_get_str(gobj, jn_data, "uuid", "", KW_REQUIRED);
    snprintf(priv->mirror_tty_uuid, sizeof(priv->mirror_tty_uuid), "%s", tty_uuid);

    priv->on_mirror_tty = TRUE;

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_mirror_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    snprintf(priv->mirror_tty_name, sizeof(priv->mirror_tty_name), "%s", "");
    snprintf(priv->mirror_tty_uuid, sizeof(priv->mirror_tty_uuid), "%s", "");
    priv->on_mirror_tty = FALSE;
    clear_input_line(gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_mirror_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    const char *agent_name = gobj_name(gobj_read_pointer_attr(src, "user_data"));

    json_t *jn_data = kw_get_dict(gobj, kw, "data", 0, KW_REQUIRED);
    if(jn_data) {
        const char *tty_name = kw_get_str(gobj, jn_data, "name", 0, 0);
        char mirror_tty_name[NAME_MAX];
        snprintf(mirror_tty_name, sizeof(mirror_tty_name), "%s(%s)", agent_name, tty_name);

        const char *content64 = kw_get_str(gobj, jn_data, "content64", 0, 0);
        if(empty_string(content64)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "content64 empty",
                NULL
            );
            JSON_DECREF(jn_data);
            JSON_DECREF(kw);
            return -1;
        }

        gbuffer_t *gbuf = gbuffer_base64_to_binary(content64, strlen(content64));
        char *p = gbuffer_cur_rd_pointer(gbuf);
        int len = (int)gbuffer_leftbytes(gbuf);

        if(gobj_trace_level(gobj) & TRACE_KB) {
            gobj_trace_dump(gobj, p, len,  "write_tty");
        }
        fwrite(p, len, 1, stdout);
        fflush(stdout);
        GBUFFER_DECREF(gbuf);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    printf("\n");
    exit(gobj_get_exit_code());

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
    .mt_writing     = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_YCOMMAND);

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
        {"editline",    EV_EDITLINE_REVERSE_SEARCH,   CTRL_R},
        {"editline",    EV_EDITLINE_FORWARD_SEARCH,   CTRL_S},
        {"editline",    EV_EDITLINE_PREV_HIST,        MKEY_UP},
        {"editline",    EV_EDITLINE_PREV_HIST,        MKEY_UP2},
        {"editline",    EV_EDITLINE_NEXT_HIST,        MKEY_DOWN},
        {"editline",    EV_EDITLINE_NEXT_HIST,        MKEY_DOWN2},
        {"editline",    EV_EDITLINE_SWAP_CHAR,        CTRL_T},
        {"editline",    EV_EDITLINE_DEL_EOL,          CTRL_K},
        {"editline",    EV_EDITLINE_DEL_LINE,         CTRL_U},
        {"editline",    EV_EDITLINE_DEL_LINE,         CTRL_Y},
        {"editline",    EV_EDITLINE_DEL_PREV_WORD,    CTRL_W},

        {"screen",      EV_CLRSCR,                    CTRL_L},
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
    ev_action_t st_stopped[] = {
        {EV_ON_CLOSE,               ac_on_close,            0},
        {0,0,0}
    };

    ev_action_t st_wait_stopped[] = {
        {EV_ON_CLOSE,               ac_on_close,            0},
        {0,0,0}
    };

    ev_action_t st_disconnected[] = {
        {EV_ON_TOKEN,               ac_on_token,            0},
        {EV_ON_OPEN,                ac_on_open,             ST_CONNECTED},
        {EV_ON_CLOSE,               ac_on_close,            0},
        {EV_ON_ID_NAK,              ac_on_close,            0},
        {EV_COMMAND,                ac_command,             0}, // avoid traces
        {EV_STOPPED,                0,                      0},
        {0,0,0}
    };

    ev_action_t st_connected[] = {
        {EV_COMMAND,                ac_command,             0},
        {EV_MT_COMMAND_ANSWER,      ac_command_answer,      0},
        {EV_MT_STATS_ANSWER,        ac_command_answer,      0},
        {EV_EDIT_CONFIG,            ac_edit_config,         0},
        {EV_VIEW_CONFIG,            ac_view_config,         0},
        {EV_EDIT_YUNO_CONFIG,       ac_edit_config,         0},
        {EV_VIEW_YUNO_CONFIG,       ac_view_config,         0},
        {EV_READ_JSON,              ac_read_json,           0},
        {EV_READ_FILE,              ac_read_file,           0},
        {EV_READ_BINARY_FILE,       ac_read_binary_file,    0},
        {EV_TTY_OPEN,               ac_tty_mirror_open,     0},
        {EV_TTY_CLOSE,              ac_tty_mirror_close,    0},
        {EV_TTY_DATA,               ac_tty_mirror_data,     0},
        {EV_CLRSCR,                 ac_screen_ctrl,         0},
        {EV_SCROLL_PAGE_UP,         ac_screen_ctrl,         0},
        {EV_SCROLL_PAGE_DOWN,       ac_screen_ctrl,         0},
        {EV_SCROLL_LINE_UP,         ac_screen_ctrl,         0},
        {EV_SCROLL_LINE_DOWN,       ac_screen_ctrl,         0},
        {EV_SCROLL_TOP,             ac_screen_ctrl,         0},
        {EV_SCROLL_BOTTOM,          ac_screen_ctrl,         0},
        {EV_ON_CLOSE,               ac_on_close,            ST_WAIT_STOPPED},
        {EV_TIMEOUT,                ac_timeout,             0},
        {EV_STOPPED,                0,                      0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,           st_disconnected},
        {ST_STOPPED,                st_stopped},
        {ST_WAIT_STOPPED,           st_wait_stopped},
        {ST_CONNECTED,              st_connected},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_MT_COMMAND_ANSWER,      EVF_PUBLIC_EVENT},
        {EV_MT_STATS_ANSWER,        EVF_PUBLIC_EVENT},
        {EV_EDIT_CONFIG,            EVF_PUBLIC_EVENT},
        {EV_VIEW_CONFIG,            EVF_PUBLIC_EVENT},
        {EV_EDIT_YUNO_CONFIG,       EVF_PUBLIC_EVENT},
        {EV_VIEW_YUNO_CONFIG,       EVF_PUBLIC_EVENT},
        {EV_READ_JSON,              EVF_PUBLIC_EVENT},
        {EV_READ_FILE,              EVF_PUBLIC_EVENT},
        {EV_READ_BINARY_FILE,       EVF_PUBLIC_EVENT},
        {EV_TTY_OPEN,               EVF_PUBLIC_EVENT},
        {EV_TTY_CLOSE,              EVF_PUBLIC_EVENT},
        {EV_TTY_DATA,               EVF_PUBLIC_EVENT},
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
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // Authorization table
        local_command_table,
        s_user_trace_level,
        0   // GClass flags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *          Public access
 ***************************************************************************/
PUBLIC int register_c_ycommand(void)
{
    return create_gclass(C_YCOMMAND);
}
