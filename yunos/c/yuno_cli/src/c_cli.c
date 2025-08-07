/***********************************************************************
 *          C_CLI.C
 *          Cli GClass.
 *
 *          Yuneta Command Line Interface
 *
 *          Copyright (c) 2015 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include <ncurses/ncurses.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <g_ev_console.h>
#include <c_editline.h>
#include "c_wn_box.h"
#include "c_wn_stsline.h"
#include "c_wn_toolbar.h"
#include "c_wn_layout.h"
#include "c_wn_stdscr.h"
#include "c_cli.h"

#include "c_wn_list.h"
#include "c_wn_static.h"


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

#define MKEY_ALT_UP             {0x1B, 0x5B, 0x31, 0x3B, 0x33, 0x41} // .[1;3A
#define MKEY_CTRL_UP            {0x1B, 0x5B, 0x31, 0x3B, 0x35, 0x41} // .[1;5A
#define MKEY_ALT_DOWN           {0x1B, 0x5B, 0x31, 0x3B, 0x33, 0x42} // .[1;3B
#define MKEY_CTRL_DOWN          {0x1B, 0x5B, 0x31, 0x3B, 0x35, 0x42} // .[1;5B

#define MKEY_CTRLALT_UP         {0x1B, 0x5B, 0x31, 0x3B, 0x37, 0x41} // .[1;7A
#define MKEY_CTRLALT_DOWN       {0x1B, 0x5B, 0x31, 0x3B, 0x37, 0x42} // .[1;7B

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE void try_to_stop_yevents(hgobj gobj);  // IDEMPOTENT
PRIVATE int yev_callback(yev_event_h yev_event);
PRIVATE int on_read_cb(hgobj gobj, gbuffer_t *gbuf);

PRIVATE int create_display_framework(hgobj gobj);
PRIVATE void do_close(hgobj gobj);
PRIVATE hgobj create_display_window(hgobj gobj, const char* name, json_t* kw_display_window);
PRIVATE hgobj get_display_window(hgobj gobj, const char *name);
PRIVATE int destroy_display_window(hgobj gobj, const char *name);
PRIVATE hgobj create_static(hgobj gobj, const char* name, json_t* kw_static);
PRIVATE int destroy_static(hgobj gobj, const char *name);
PRIVATE int set_top_window(hgobj gobj, const char *name);
PUBLIC int msg2display(hgobj gobj, const char *fmt, ...) JANSSON_ATTRS((format(printf, 2, 3)));
PRIVATE int msg2statusline(hgobj gobj, BOOL error, const char *fmt, ...) JANSSON_ATTRS((format(printf, 3, 4)));
PRIVATE char *get_primary_ip(hgobj gobj, char *bf, int bfsize);
PRIVATE int ac_agent_response(hgobj gobj, const char *event, json_t *kw, hgobj src);
PRIVATE char *get_history_file(char *bf, int bfsize);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
hgobj __top_display_window__ = 0;

typedef struct keytable_s {
    const char *dst_gobj;
    const char *event;
    uint8_t keycode[8+1];
} keytable_t;

keytable_t keytable1[] = {
{"cli",             "EV_PREVIOUS_WINDOW",           MKEY_ALT_LEFT},
{"cli",             "EV_PREVIOUS_WINDOW",           MKEY_CTRL_LEFT},
{"cli",             "EV_PREVIOUS_WINDOW",           CTRL_P},
{"cli",             "EV_NEXT_WINDOW",               MKEY_ALT_RIGHT},
{"cli",             "EV_NEXT_WINDOW",               MKEY_CTRL_RIGHT},
{"cli",             "EV_NEXT_WINDOW",               CTRL_N},
{0}
};

keytable_t keytable2[] = {
{"editline",    "EV_EDITLINE_MOVE_START",       CTRL_A},
{"editline",    "EV_EDITLINE_MOVE_START",       MKEY_START},
{"editline",    "EV_EDITLINE_MOVE_START",       MKEY_START2},
{"editline",    "EV_EDITLINE_MOVE_END",         CTRL_E},
{"editline",    "EV_EDITLINE_MOVE_END",         MKEY_END},
{"editline",    "EV_EDITLINE_MOVE_END",         MKEY_END2},
{"editline",    "EV_EDITLINE_MOVE_LEFT",        CTRL_B},
{"editline",    "EV_EDITLINE_MOVE_LEFT",        MKEY_LEFT},
{"editline",    "EV_EDITLINE_MOVE_LEFT",        MKEY_LEFT2},
{"editline",    "EV_EDITLINE_MOVE_RIGHT",       CTRL_F},
{"editline",    "EV_EDITLINE_MOVE_RIGHT",       MKEY_RIGHT},
{"editline",    "EV_EDITLINE_MOVE_RIGHT",       MKEY_RIGHT2},
{"editline",    "EV_EDITLINE_DEL_CHAR",         CTRL_D},
{"editline",    "EV_EDITLINE_DEL_CHAR",         MKEY_DEL},
{"editline",    "EV_EDITLINE_BACKSPACE",        CTRL_H},
{"editline",    "EV_EDITLINE_BACKSPACE",        BACKSPACE},
{"editline",    "EV_EDITLINE_COMPLETE_LINE",    TAB},
{"editline",    "EV_EDITLINE_ENTER",            ENTER},
{"editline",    "EV_EDITLINE_PREV_HIST",        MKEY_UP},
{"editline",    "EV_EDITLINE_PREV_HIST",        MKEY_UP2},
//{"editline",    "EV_EDITLINE_PREV_HIST",        MKEY_ALT_UP},
//{"editline",    "EV_EDITLINE_PREV_HIST",        MKEY_CTRL_UP},

{"editline",    "EV_EDITLINE_NEXT_HIST",        MKEY_DOWN},
{"editline",    "EV_EDITLINE_NEXT_HIST",        MKEY_DOWN2},
//{"editline",    "EV_EDITLINE_NEXT_HIST",        MKEY_ALT_DOWN},
//{"editline",    "EV_EDITLINE_NEXT_HIST",        MKEY_CTRL_DOWN},

{"editline",    "EV_EDITLINE_SWAP_CHAR",        CTRL_T},
{"editline",    "EV_EDITLINE_DEL_LINE",         CTRL_U},
{"editline",    "EV_EDITLINE_DEL_LINE",         CTRL_Y},
{"editline",    "EV_EDITLINE_DEL_PREV_WORD",    CTRL_W},

{"__top_display_window__",    "EV_CLRSCR",                    CTRL_K},
{"__top_display_window__",    "EV_SCROLL_PAGE_UP",            MKEY_PREV_PAGE},
{"__top_display_window__",    "EV_SCROLL_PAGE_DOWN",          MKEY_NEXT_PAGE},
{"__top_display_window__",    "EV_SCROLL_LINE_UP",            MKEY_ALT_PREV_PAGE},
{"__top_display_window__",    "EV_SCROLL_LINE_DOWN",          MKEY_ALT_NEXT_PAGE},
{"__top_display_window__",    "EV_SCROLL_TOP",                MKEY_CTRL_START},
{"__top_display_window__",    "EV_SCROLL_BOTTOM",             MKEY_CTRL_END},

{"__top_display_window__",    "EV_SCROLL_PAGE_UP",            MKEY_CTRL_UP},
{"__top_display_window__",    "EV_SCROLL_PAGE_DOWN",          MKEY_CTRL_DOWN},
{"__top_display_window__",    "EV_SCROLL_LINE_UP",            MKEY_ALT_UP},
{"__top_display_window__",    "EV_SCROLL_LINE_DOWN",          MKEY_ALT_DOWN},
{"__top_display_window__",    "EV_SCROLL_TOP",                MKEY_CTRLALT_UP},
{"__top_display_window__",    "EV_SCROLL_BOTTOM",             MKEY_CTRLALT_DOWN},

{0}
};

PRIVATE json_t *cmd_help(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_quit(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_connect(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
//PRIVATE json_t *cmd_disconnect(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_open_log(hgobj gobj, const char *command, json_t *kw, hgobj src);
PRIVATE json_t *cmd_display_mode(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_editor(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_refresh(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_open_output(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_close_output(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_add_shortkey(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_remove_shortkey(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_shortkey(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_list_history(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_clear_history(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
PRIVATE json_t *cmd_do_authenticate_task(hgobj gobj, const char *cmd, json_t *kw, hgobj src);

PRIVATE sdata_desc_t pm_help[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "cmd",          0,              0,          "command about you want help."),
SDATAPM (DTP_INTEGER,   "level",        0,              0,          "command search level in childs"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_connect[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "url",          0,              "ws://127.0.0.1:1991",  "Agent's url to connect. Can be a ip/hostname or a full url"),
SDATAPM (DTP_STRING,    "yuno_name",    0,              "",                     "Yuno name"),
SDATAPM (DTP_STRING,    "yuno_role",    0,              "yuneta_agent",         "Yuno role"),
SDATAPM (DTP_STRING,    "service",      0,              "agent",                "Yuno service"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_log[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "url",          0,              "",         "Url of log server. Ip must bind to a local ip"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_quit[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_INTEGER,   "timeout",      0,              0,          "delay exit in x seconds"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_display_mode[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "mode",         0,              "",         "Display mode: table or form"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_editor[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "editor",       0,              "",         "Editor (cannot be a console editor)"),
SDATA_END()
};

PRIVATE sdata_desc_t pm_save[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "filename",     0,              "",         "Filename to save display output"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_shortkey[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "key",          0,              0,          "Shortkey"),
SDATAPM (DTP_STRING,    "command",      0,              0,          "Command"),
SDATA_END()
};
PRIVATE sdata_desc_t pm_authenticate[] = {
/*-PM----type-----------name------------flag------------default-----description---------- */
SDATAPM (DTP_STRING,    "auth_system",  0,              "",         "OpenID System(interactive jwt)"),
SDATAPM (DTP_STRING,    "auth_url",     0,              "",         "OpenID Endpoint (interactive jwt)"),
SDATAPM (DTP_STRING,    "azp",          0,              "",         "azp (OAuth2 Authorized Party)"),
SDATAPM (DTP_STRING,    "user_id",      0,              "",         "OAuth2 User Id (interactive jwt)"),
SDATAPM (DTP_STRING,    "user_passw",   0,              "",         "OAuth2 User password (interactive jwt)"),
SDATA_END()
};

PRIVATE const char *a_help[] = {"h", "?", 0};
PRIVATE const char *a_quit[] = {"q", 0};
PRIVATE const char *a_conn[] = {"c", 0};
PRIVATE const char *a_log[] = {"l", 0};

PRIVATE sdata_desc_t command_table[] = {
/*-CMD---type-----------name----------------alias---------------items-------json_fn-----description---------- */
SDATACM (DTP_SCHEMA,    "",                 0,                  0,          0,          "\nEdit line shortcuts\n-------------------"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Move start             -> Home, Ctrl+a"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Move end               -> End, Ctrl+e"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Move left              -> Left, Ctrl+b"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Move right             -> Right, Ctrl+f"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Delete char            -> Del, Ctrl+d"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Backspace (del)        -> Backspace, Ctrl+h"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Execute command        -> Enter"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Previous history       -> Up"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Next history           -> Down"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Swap char              -> Ctrl+t"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Delete line            -> Ctrl+u"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Delete previous word   -> Ctrl+w"),
SDATACM (DTP_SCHEMA,    "",                 0,                  0,          0,          "\nOutput window shortcuts\n-----------------------"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Previous Window        -> Alt+Left, Ctrl+p"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Next Window            -> Alt+Right, Ctrl+n"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Scroll Bottom          -> Ctrl+End"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Clear Screen           -> Ctrl+k"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Scroll Line up         -> Ctrl+Prev.Page, Alt+Up"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Scroll Line down       -> Ctrl+Next.Page, Alt+Down"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Scroll Page up         -> Prev.Page, Ctrl+Up"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Scroll Page down       -> Next.Page, Ctrl+Down"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Scroll Top             -> Ctrl+Home, Ctrl+Alt+Up"),
SDATACM (DTP_STRING,    "", 0,  0,  0,  "Scroll Bottom          -> Ctrl+End, Ctrl+Alt+Down"),

SDATACM (DTP_SCHEMA,    "",                 0,                  0,          0,          "\nConsole commands\n----------------"),
SDATACM (DTP_SCHEMA,    "help",             a_help,             pm_help,    cmd_help,   "Command's help"),
SDATACM (DTP_STRING,    "connect",          a_conn,             pm_connect, cmd_connect,"Connect to some yuno and service (by default to Agent/agent)."),
SDATACM (DTP_STRING,    "open-log",         a_log,              pm_log,     cmd_open_log,"Open a log console (Use command-yuno service=__yuno__ yuno_role=logcenter command=add-log-handler name=test type=udp url=udp://???:1994)."),
SDATACM (DTP_STRING,    "quit",             a_quit,             pm_quit,    cmd_quit,   "Quit of Yuneta."),
SDATACM (DTP_STRING,    "display-mode",     0,                  pm_display_mode, cmd_display_mode,"Change display mode: table or form."),
SDATACM (DTP_STRING,    "editor",           0,                  pm_editor,  cmd_editor, "Change editor."),
SDATACM (DTP_STRING,    "refresh",          0,                  0,          cmd_refresh,"Refresh console display."),
SDATACM (DTP_STRING,    "save-output",      0,                  pm_save,    cmd_open_output, "Open file to save display output."),
SDATACM (DTP_STRING,    "close-output",     0,                  0,          cmd_close_output, "Close file saving display output."),
SDATACM (DTP_STRING,    "add-shortkey",     0,                  pm_shortkey,cmd_add_shortkey, "Add new shortkey."),
SDATACM (DTP_STRING,    "remove-shortkey",  0,                  pm_shortkey,cmd_remove_shortkey, "Remove shortkey."),
SDATACM (DTP_STRING,    "shortkeys",        0,                  0,          cmd_list_shortkey, "List shortkeys."),
SDATACM (DTP_STRING,    "history",          0,                  0,          cmd_list_history, "List command history."),
SDATACM (DTP_STRING,    "clear-history",    0,                  0,          cmd_clear_history, "Delete command history."),
SDATACM (DTP_STRING,    "authenticate",     0,                  pm_authenticate,cmd_do_authenticate_task, "Authenticate to get a jwt to do remote connection."),
SDATACM (DTP_STRING,    "",                 0,                  0,          0,          ""),
SDATACM (DTP_STRING,    "",                 0,                  0,          0,          "You can execute console commands in connection windows with ! prefix."),
SDATACM (DTP_STRING,    "",                 0,                  0,          0,          "You can force display mode form with * prefix."),
SDATACM (DTP_STRING,    "",                 0,                  0,          0,          ""),
SDATA_END()
};

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
/*-ATTR-type------------name----------------flag----------------default-----description---------- */
SDATA (DTP_STRING,      "jwt",              0,                  "",         "Jwt"),
SDATA (DTP_STRING,      "display_mode",     SDF_WR|SDF_PERSIST, "table",    "Display mode: table or form"),
SDATA (DTP_STRING,      "editor",           SDF_WR|SDF_PERSIST, "vim",      "Editor"),
SDATA (DTP_JSON,        "shortkeys",        SDF_WR|SDF_PERSIST, 0,          "Shortkeys. A dict {key: command}."),
// TODO set to "1"
SDATA (DTP_BOOLEAN,     "use_ncurses",      0,                  "0",        "True to use ncurses, set False for easy testing"),

SDATA (DTP_POINTER,     "user_data",        0,                  0,          "user data"),
SDATA (DTP_POINTER,     "user_data2",       0,                  0,          "more user data"),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_KB = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"trace-kb",            "Trace keyboard codes"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    hgobj timer;
    hgobj gwin_stdscr;
    hgobj gobj_toptoolbar;
    hgobj gobj_workareabox;
    hgobj gobj_editbox;
    hgobj gobj_editline;
    hgobj gobj_bottomtoolbarbox;
    hgobj gobj_stsline;

    int tty_fd;
    yev_event_h yev_reading;

    BOOL on_mirror_tty;
    char mirror_tty_name[NAME_MAX];
    char mirror_tty_uuid[NAME_MAX];

    FILE *file_saving_output;
    json_t *jn_shortkeys;

    json_t *jn_window_counters;
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

    priv->timer = gobj_create("", C_TIMER, 0, gobj);
    priv->jn_shortkeys = gobj_read_json_attr(gobj, "shortkeys");
    if(!priv->jn_shortkeys) {
        json_t *jn_dict = json_object();
        gobj_write_json_attr(gobj, "shortkeys", jn_dict);
        priv->jn_shortkeys = gobj_read_json_attr(gobj, "shortkeys");
        JSON_DECREF(jn_dict);
    }
    priv->jn_window_counters = json_object();

    if(gobj_read_bool_attr(gobj, "use_ncurses")) {
        create_display_framework(gobj);
    }

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
     *  History filename, for editline
     */
    char history_file[PATH_MAX];
    get_history_file(history_file, sizeof(history_file));

    /*
     *  Editline
     */
    json_t *kw_editline = json_pack("{s:s, s:s, s:b, s:s, s:s, s:i, s:i, s:I}",
        "prompt", "> ",
        "history_file", history_file,
        "use_ncurses", gobj_read_bool_attr(gobj, "use_ncurses"),
        "bg_color", "gray",
        "fg_color", "black",
        "cx", winsz.ws_col,
        "cy", winsz.ws_row,
        "subscriber", (json_int_t)(uintptr_t)gobj
    );
    priv->gobj_editline = gobj_create_service(
        "editline",
        C_EDITLINE,
        kw_editline,
        priv->gobj_editbox?priv->gobj_editbox:gobj
    );

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);
//
//     IF_EQ_SET_PRIV(timeout,         gobj_read_integer_attr)
//     END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Free data
     */
    // growbf_reset(&priv->bfinput); TODO
    JSON_DECREF(priv->jn_window_counters);
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    if(priv->gwin_stdscr) {
        create_display_framework(gobj);
        gobj_start(priv->gwin_stdscr);
    }

    priv->tty_fd = tty_init();
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

    SetDefaultFocus(priv->gobj_editline);
    msg2statusline(gobj, 0, "Wellcome to Yuneta. Type help for assistance.");

    /*
     *  Create display window of console
     */
    hgobj wn_disp = create_display_window(gobj, "console", 0);
    /*
     *  Each display window has a gobj to send the commands (saved in user_data).
     *  For console window his gobj is this gobj
     */
    gobj_write_pointer_attr(wn_disp, "user_data", gobj);
    gobj_write_pointer_attr(gobj, "user_data", wn_disp);

    /*
     *  Create button window of console (right now implemented as static window)
     */
    create_static(gobj, "console", 0);
    set_top_window(gobj, "console");

#ifdef TEST_KDEVELOP_DIE
        set_timeout(priv->timer, 1000);
#endif
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    // TODO destroy agent's windows
    destroy_display_window(gobj, "console");
    destroy_static(gobj, "console");

    if(priv->file_saving_output) {
        fclose(priv->file_saving_output);
        priv->file_saving_output = 0;
    }

    try_to_stop_yevents(gobj);
    do_close(gobj);

    gobj_stop_tree(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method inject_event
 ***************************************************************************/
PRIVATE int mt_inject_event(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    // TODO esto se usa??
    // Por aquí entran eventos que no están definidos en el automata
    // En ac_agent_response se displaya kw
    return ac_agent_response(gobj, event, kw, src);
}




            /***************************
             *      Commands
             ***************************/




PRIVATE char agent_config[]= "\
{                                               \n\
    'name': '(^^__url__^^)',                    \n\
    'gclass': 'IEvent_cli',                     \n\
    'as_service': true,                          \n\
    'kw': {                                     \n\
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
                            'kw': {                                     \n\
                                'kw_connex': {                              \n\
                                    'url':'(^^__url__^^)'   \n\
                                }                                   \n\
                            }                                   \n\
                        }                                       \n\
                    ]                                           \n\
                }                                               \n\
            ]                                           \n\
        }                                               \n\
    ]                                           \n\
}                                               \n\
";

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_connect(hgobj gobj, const char *command, json_t *kw, hgobj src)
{
    const char *url = kw_get_str(gobj, kw, "url", "", 0);
    const char *jwt = gobj_read_str_attr(gobj, "jwt");
    const char *yuno_name = kw_get_str(gobj, kw, "yuno_name", "", 0);
    const char *yuno_role = kw_get_str(gobj, kw, "yuno_role", "", 0);
    const char *yuno_service = kw_get_str(gobj, kw, "service", "", 0);

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

    hgobj gobj_remote_agent = gobj_create_tree(
        gobj,
        agent_config,
        jn_config_variables
    );

    gobj_start_tree(gobj_remote_agent);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Connecting to %s...", url),
        0,
        0,
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
/*
PRIVATE json_t *cmd_disconnect(hgobj gobj, const char *command, json_t *kw)
{
    const char *agent = kw_get_str(gobj, kw, "agent", 0, 0);

    hgobj gobj_router = gobj_find_service("router", FALSE);
    json_t *kw_route = json_pack("{s:s}",
        "name", agent
    );
    json_t *jn_resp;
    if(gobj_send_event(gobj_router, EV_DEL_STATIC_ROUTE, kw_route, gobj)<0) {
         jn_resp = json_sprintf("Agent '%s' NOT FOUND.", agent);
    } else {
         jn_resp = json_sprintf("Disconnecting from %s...", agent);
    }

    KW_DECREF(kw);
    return jn_resp;
}
*/

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE char *get_primary_ip(hgobj gobj, char *bf, int bfsize)
{
    json_t *webix = gobj_stats(gobj_yuno(), "ifs", 0, gobj);
    int result = (int)kw_get_int(gobj, webix, "result", -1, 0);
    json_t *jn_data = kw_get_list(gobj, webix, "data", 0, 0);

    if(result!=0 || json_array_size(jn_data)==0) {
        snprintf(bf, bfsize, "%s", "?.?.?.?");
        JSON_DECREF(webix);
        return bf;
    }
    int idx;
    json_t *jn_value;
    json_array_foreach(jn_data, idx, jn_value) {
        BOOL is_internal = kw_get_bool(gobj, jn_value, "is_internal", 1, 0);
        if(is_internal) {
            continue;
        }
        const char *ip_v4 = kw_get_str(gobj, jn_value, "ip-v4", "", 0);
        if(empty_string(ip_v4)) {
            continue;
        }
        snprintf(bf, bfsize, "%s", ip_v4);
        break;
    }
    JSON_DECREF(webix);
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_open_log(hgobj gobj, const char *command, json_t *kw, hgobj src)
{
    const char *url = kw_get_str(gobj, kw, "url", "", 0);
    char temp[80];
    if(empty_string(url)) {
        char ip[30];
        snprintf(temp, sizeof(temp), "udp://%s:1994", get_primary_ip(gobj, ip, sizeof(ip)));
        url = temp;
    }
    /*
     *  Create display window of external agent
     */
    hgobj wn_disp = get_display_window(gobj, url);
    if(wn_disp) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Log server at %s already opened.", url),
            0,
            0,
            kw
        );
    }
    wn_disp = create_display_window(gobj, url, 0);

    json_t *kw_gss_udps = json_pack("{s:s}",
        "url", url
    );
    hgobj gobj_gss_udp_s = gobj_create("", C_GSS_UDP_S, kw_gss_udps, wn_disp);
    if(!gobj_gss_udp_s) {
        gobj_destroy(wn_disp);
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Log server at %s failed to open.", url),
            0,
            0,
            kw
        );
    }
    gobj_start(gobj_gss_udp_s);

    /*
     *  Create button window of console (right now implemented as static window)
     */
    create_static(gobj, url, 0);
    set_top_window(gobj, url);

    gobj_write_pointer_attr(wn_disp, "user_data", gobj_gss_udp_s);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Log server at %s.", url),
        0,
        0,
        kw
    );
}

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
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_quit(hgobj gobj, const char *command, json_t *kw, hgobj src)
{
    KW_INCREF(kw);
    gobj_send_event(gobj, EV_QUIT, kw, gobj);
    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Exiting..."),
        0,
        0,
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_display_mode(hgobj gobj, const char *command, json_t *kw, hgobj src)
{
    const char *_mode[] = {"table", "form", 0};
    const char *display_mode = kw_get_str(gobj, kw, "mode", "", 0);
    if(!empty_string(display_mode)) {
        if(str_in_list(_mode, display_mode, TRUE)) {
            gobj_write_str_attr(gobj, "display_mode", display_mode);
            gobj_save_persistent_attrs(gobj, json_string("display_mode"));
        }
    }

    display_mode = gobj_read_str_attr(gobj, "display_mode");

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Mode '%s'.", display_mode),
        0,
        0,
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_editor(hgobj gobj, const char *command, json_t *kw, hgobj src)
{
    const char *editor = kw_get_str(gobj, kw, "editor", "", 0);
    if(!empty_string(editor)) {
        gobj_write_str_attr(gobj, "editor", editor);
        gobj_save_persistent_attrs(gobj, json_string("editor"));
    }

    editor = gobj_read_str_attr(gobj, "editor");

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Editor '%s'.", editor),
        0,
        0,
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_refresh(hgobj gobj, const char *command, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_send_event_to_children_tree(priv->gwin_stdscr, EV_PAINT, 0, gobj);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Done!"),
        0,
        0,
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_open_output(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *filename = kw_get_str(gobj, kw, "filename", "", 0);
    if(!empty_string(filename)) {
        if(priv->file_saving_output) {
            fclose(priv->file_saving_output);
        }
        priv->file_saving_output = fopen(filename, "w");
    } else {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What file?"),
            0,
            0,
            kw
        );
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Done!"),
        0,
        0,
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t* cmd_close_output(hgobj gobj, const char* cmd, json_t* kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->file_saving_output) {
        fclose(priv->file_saving_output);
        priv->file_saving_output = 0;
    }

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Done!"),
        0,
        0,
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_add_shortkey(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!priv->jn_shortkeys) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Shortkeys dict NULL"),
            0,
            0,
            kw
        );
    }

    const char *key = kw_get_str(gobj, kw, "key", 0, 0);
    const char *command = kw_get_str(gobj, kw, "command", 0, 0);
    if(empty_string(key)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What key?"),
            0,
            0,
            kw
        );
    }
    if(empty_string(command)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What command?"),
            0,
            0,
            kw
        );
    }

    json_object_set(priv->jn_shortkeys, key, json_string(command));
    gobj_save_persistent_attrs(gobj,0);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Done! \"%s\": \"%s\"", key, command),
        0,
        0,
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_remove_shortkey(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!priv->jn_shortkeys) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Shortkeys dict NULL"),
            0,
            0,
            kw
        );
    }

    const char *key = kw_get_str(gobj, kw, "key", 0, 0);
    if(empty_string(key)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What key?"),
            0,
            0,
            kw
        );
    }

    if(!kw_has_key(priv->jn_shortkeys, key)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Key '%s' not found.", key),
            0,
            0,
            kw
        );
    } else {
        json_object_del(priv->jn_shortkeys, key);
    }
    gobj_save_persistent_attrs(gobj, 0);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("Done!"),
        0,
        0,
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_shortkey(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(!priv->jn_shortkeys) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("Shortkeys dict NULL"),
            0,
            0,
            kw
        );
    }

    JSON_INCREF(priv->jn_shortkeys);
    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        priv->jn_shortkeys,
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_list_history(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    json_t *jn_data = json_array();

    char history_file[PATH_MAX];
    get_history_file(history_file, sizeof(history_file));

    FILE *file = fopen(history_file, "r");
    if(file) {
        char temp[1024];
        while(fgets(temp, sizeof(temp), file)) {
            left_justify(temp);
            if(strlen(temp)>0) {
                json_array_append_new(jn_data, json_string(temp));
            }
        }
        fclose(file);
    }

    return msg_iev_build_response(
        gobj,
        0,
        0,
        0,
        jn_data,
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_clear_history(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gobj_send_event(priv->gobj_editline, EV_CLEAR_HISTORY, 0, gobj);

    return msg_iev_build_response(
        gobj,
        0,
        json_sprintf("History cleared.\n"),
        0,
        0,
        kw
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE json_t *cmd_do_authenticate_task(hgobj gobj, const char *cmd, json_t *kw, hgobj src)
{
    /*-----------------------------*
     *      Get parameters
     *-----------------------------*/
    const char *auth_system = kw_get_str(gobj, kw, "auth_system", "", 0); // "keycloak" by default
    const char *auth_url = kw_get_str(gobj, kw, "auth_url", "", 0);
    const char *user_id = kw_get_str(gobj, kw, "user_id", "", 0);
    const char *user_passw = kw_get_str(gobj, kw, "user_passw", "", 0);
    const char *azp = kw_get_str(gobj, kw, "azp", "", 0);

    if(empty_string(auth_url)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What auth_url? (OpenID Endpoint)"),
            0,
            0,
            kw
        );
    }
    if(empty_string(azp)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What 'azp'? (Oauth2 Authorized Party, client_id in keycloak)"),
            0,
            0,
            kw
        );
    }
    if(empty_string(user_id)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What user_id? (OAuth2 User Id)"),
            0,
            0,
            kw
        );
    }
    if(empty_string(user_passw)) {
        return msg_iev_build_response(
            gobj,
            -1,
            json_sprintf("What user_passw? (OAuth2 User Password)"),
            0,
            0,
            kw
        );
    }

    /*-----------------------------*
     *      Create the task
     *-----------------------------*/
    hgobj gobj_task = gobj_find_service("task-authenticate", FALSE);
    if(!gobj_task) {
        json_t *kw_task = json_pack("{s:s, s:s, s:s, s:s, s:s}",
            "auth_system", auth_system,
            "auth_url", auth_url,
            "user_id", user_id,
            "user_passw", user_passw,
            "azp", azp
        );
        gobj_task = gobj_create_service("task-authenticate", C_TASK_AUTHENTICATE, kw_task, gobj);
    } else {
        gobj_write_str_attr(gobj_task, "auth_system", auth_system);
        gobj_write_str_attr(gobj_task, "auth_url", auth_url);
        gobj_write_str_attr(gobj_task, "user_id", user_id);
        gobj_write_str_attr(gobj_task, "user_passw", user_passw);
        gobj_write_str_attr(gobj_task, "azp", azp);
    }
    if(gobj_task) {
        gobj_subscribe_event(gobj_task, "EV_ON_TOKEN", 0, gobj);
        gobj_set_volatil(gobj_task, TRUE); // auto-destroy

        /*-----------------------*
         *      Start task
         *-----------------------*/
        gobj_start(gobj_task);
    }

    KW_DECREF(kw);
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
            "msg2",         "%s", "TCP 🌐🌐💥 yev callback",
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
                            "msg",          "%s", "TCP: read FAILED",
                            "msg2",         "%s", "🌐TCP: read FAILED",
                            "url",          "%s", gobj_read_str_attr(gobj, "url"),
                            "remote-addr",  "%s", gobj_read_str_attr(gobj, "peername"),
                            "local-addr",   "%s", gobj_read_str_attr(gobj, "sockname"),
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
// PRIVATE int process_key(hgobj gobj, uint8_t kb) TODO ???
// {
//     PRIVATE_DATA *priv = gobj_priv_data(gobj);
//
//     if(kb >= 0x20 && kb <= 0x7f) {
//         json_t *kw_char = json_pack("{s:i}",
//             "char", kb
//         );
//         gobj_send_event(priv->gobj_editline, EV_KEYCHAR, kw_char, gobj);
//     }
//
//     return 0;
// }

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
    if(nread > 8) {
        // It's must be the mouse cursor
        char *p = strchr(base+1, 0x1B);
        if(p) {
            *p = 0;
            nread = (int)(p - base);
            if(gobj_trace_level(gobj) & (TRACE_KB)) {
                gobj_trace_dump(
                    gobj,
                    base,
                    nread,
                    "REDUCE!"
                );
            }
        }
    }
    uint8_t b[8];
    memset(b, 0, sizeof(b));
    memmove(b, base, MIN(8, nread));

    do {
        if(priv->on_mirror_tty) {
            hgobj gobj_cmd = gobj_read_pointer_attr(gobj, "gobj_connector");

            gbuffer_t *gbuf2 = gbuffer_create(nread, nread);
            gbuffer_append(gbuf2, base, nread);
            gbuffer_t *gbuf_content64 = gbuffer_encode_base64(gbuf2);
            char *content64 = gbuffer_cur_rd_pointer(gbuf_content64);

            json_t *kw_command = json_pack("{s:s, s:s, s:s}",
                "name", priv->mirror_tty_name,
                "agent_id", priv->mirror_tty_uuid,
                "content64", content64
            );

            json_decref(gobj_command(gobj_cmd, "write-tty", kw_command, gobj));

            GBUFFER_DECREF(gbuf_content64)
            GBUFFER_DECREF(gbuf2)
            break;
        }

        /*
         *  Level 1, window management functions
         */
        struct keytable_s *kt = event_by_key(keytable1, b);
        if(kt) {
            const char *dst = kt->dst_gobj;
            const char *event = kt->event;

            hgobj dst_gobj = gobj_find_service(dst, FALSE);
            if(!dst_gobj) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "service gobj NOT FOUND",
                    "service",      "%s", dst,
                    NULL
                );
            } else {
                gobj_send_event(dst_gobj, event, 0, gobj);
            }
            return 0;
        }

        /*
         *  Level 2, top window or editline functions
         */
        kt = event_by_key(keytable2, b);
        if(kt) {
            const char *dst = kt->dst_gobj;
            const char *event = kt->event;

            hgobj dst_gobj;
            if(!empty_string(dst)) {
                if(strcmp(dst, "__top_display_window__")==0) {
                    dst_gobj = __top_display_window__;
                } else {
                    dst_gobj = gobj_find_service(dst, FALSE);
                }
            } else {
                dst_gobj = GetFocus();
            }
            if(!dst_gobj) {
                gobj_log_error(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                    "msg",          "%s", "service gobj NOT FOUND",
                    "service",       "%s", dst,
                    NULL
                );
            } else {
                if(gobj_has_event(dst_gobj, event, 0)) {
                    gobj_send_event(dst_gobj, event, 0, gobj);
                    if(strcmp(event, "EV_EDITLINE_DEL_LINE")==0) {
                        msg2statusline(gobj, 0, "%s", "");
                    }
                }
            }
            return 0;
        }

        /*
         *  Level 3, chars to window with focus
         */
        if(base[0] >= 0x20 && base[0] <= 0x7f) {
            // No pases escapes ni utf8
            gbuffer_t *gbuf2 = gbuffer_create(nread, nread);
            gbuffer_append(gbuf, base, nread);

            json_t *kw_keychar = json_pack("{s:I}",
                "gbuffer", (json_int_t)(uintptr_t)gbuf2
            );
            gobj_send_event(GetFocus(), EV_KEYCHAR, kw_keychar, gobj);
        }

    } while(0);

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
        strcat(bf, "/history.txt");
    }
    return bf;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int create_display_framework(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gwin_stdscr = gobj_create("cli", C_WN_STDSCR, 0, gobj);

    /*---------------------------------*
     *  Layout stdscr
     *---------------------------------*/
    json_t *kw_layout  = json_pack(
        "{s:s, s:s, s:s}",
        "layout_type", "vertical",
        "bg_color", "black",
        "fg_color", "white"
    );
    hgobj gobj_layout = gobj_create(
        "layout",
        C_WN_LAYOUT,
        kw_layout,
        priv->gwin_stdscr
    );

    /*---------------------------------*
     *  Top toolbar
     *---------------------------------*/
    json_t *kw_toptoolbarbox  = json_pack(
        "{s:s, s:s, s:i, s:i}",
        "bg_color", "read",
        "fg_color", "white",
        "w", 0,
        "h", 1
    );
    hgobj gobj_toptoolbarbox = gobj_create(
        "toptoolbarbox",
        C_WN_BOX,
        kw_toptoolbarbox,
        gobj_layout
    );
    json_t *kw_toptoolbar  = json_pack(
        "{s:s, s:s, s:s}",
        "layout_type", "horizontal",
        "bg_color", "cyan",
        "fg_color", "black"
    );
    priv->gobj_toptoolbar = gobj_create(
        "toptoolbar",
        C_WN_TOOLBAR,
        kw_toptoolbar,
        gobj_toptoolbarbox
    );

    /*---------------------------------*
     *  Work area
     *---------------------------------*/
    json_t *kw_workareabox = json_pack(
        "{s:s, s:s, s:i, s:i}",
        "bg_color", "black",
        "fg_color", "white",
        "w", 0,
        "h", -1
    );
    priv->gobj_workareabox = gobj_create(
        "workareabox",
        C_WN_BOX,
        kw_workareabox,
        gobj_layout
    );

    /*---------------------------------*
     *  Edit line
     *---------------------------------*/
    json_t *kw_editbox = json_pack(
        "{s:s, s:s, s:i, s:i}",
        "bg_color", "gray",
        "fg_color", "black",
        "w", 0,
        "h", 1
    );
    priv->gobj_editbox = gobj_create(
        "editbox",
        C_WN_BOX,
        kw_editbox,
        gobj_layout
    );

    /*---------------------------------*
     *  Bottom toolbar
     *---------------------------------*/
    json_t *kw_bottomtoolbarbox  = json_pack(
        "{s:s, s:s, s:i, s:i}",
        "bg_color", "yellow",
        "fg_color", "black",
        "w", 0,
        "h", 1
    );
    priv->gobj_bottomtoolbarbox = gobj_create(
        "bottomtoolbarbox",
        C_WN_BOX,
        kw_bottomtoolbarbox,
        gobj_layout
    );
    json_t *kw_stsline = json_pack(
        "{s:s, s:s}",
        "bg_color", "yellow",
        "fg_color", "black"
    );
    priv->gobj_stsline = gobj_create(
        "stsline",
        C_WN_STSLINE,
        kw_stsline,
        priv->gobj_bottomtoolbarbox
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE hgobj create_display_window(hgobj gobj, const char *name, json_t *kw_display_window)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!kw_display_window) {
        kw_display_window = json_pack(
            "{s:s, s:s}",
            "bg_color", "black",
            "fg_color", "white"
        );
    }
    hgobj wn_display = gobj_create(
        name,
        C_WN_LIST,
        kw_display_window,
        priv->gobj_workareabox
    );
    gobj_start(wn_display);

    return wn_display;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE hgobj get_display_window(hgobj gobj, const char* name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj gobj_display = gobj_child_by_name(priv->gobj_workareabox, name);
    return gobj_display;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int destroy_display_window(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_destroy_named_childs(
        priv->gobj_workareabox,
        name
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE hgobj create_static(hgobj gobj, const char *name, json_t *kw_static)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char sname[32];
    snprintf(sname, sizeof(sname), " %s ", name);

    if(!kw_static) {
        kw_static = json_pack(
            "{s:s, s:s, s:s, s:i, s:i}",
            "bg_color", "cyan",
            "fg_color", "black",
            "text", sname,
            "w", strlen(sname),
            "h", 1
        );
    }
    hgobj wn_static = gobj_create(
        name,
        C_WN_STATIC,
        kw_static,
        priv->gobj_toptoolbar
    );
    gobj_start(wn_static);

    return wn_static;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int destroy_static(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_destroy_named_childs(
        priv->gobj_toptoolbar,
        name
    );

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int set_top_window(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj gobj_display = gobj_child_by_name(priv->gobj_workareabox, name);
    if(gobj_display) {
        gobj_send_event(gobj_display, EV_SET_TOP_WINDOW, 0, gobj);
        __top_display_window__ = gobj_display;

        json_t *kw_sel = json_pack("{s:s}",
            "selected", name
        );
        gobj_send_event(priv->gobj_toptoolbar, EV_SET_SELECTED_BUTTON, kw_sel, gobj);

        char prompt[32];
        snprintf(prompt, sizeof(prompt), "%s> ", name);
        gobj_write_str_attr(priv->gobj_editline, "prompt", prompt);

        SetFocus(gobj_display);

    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,
            "msg",          "%s", "gobj_child_by_name() FAILED",
            "name",         "%s", name,
            NULL
        );
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE hgobj get_top_display_window(hgobj gobj)
{
    return __top_display_window__;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE gbuffer_t *jsontable2str(json_t *jn_schema, json_t *jn_data)
{
    gbuffer_t *gbuf = gbuffer_create(4*1024, gbmem_get_maximum_block());
    hgobj gobj = 0;
    size_t col;
    json_t *jn_col;
    /*
     *  Paint Headers
     */
    json_array_foreach(jn_schema, col, jn_col) {
        const char *header = kw_get_str(gobj, jn_col, "header", "", 0);
        int fillspace = (int)kw_get_int(gobj, jn_col, "fillspace", 10, KW_WILD_NUMBER);
        if(fillspace && fillspace < strlen(header)) {
            fillspace = strlen(header);
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
        int fillspace = (int)kw_get_int(gobj, jn_col, "fillspace", 10, KW_WILD_NUMBER);
        if(fillspace && fillspace < strlen(header)) {
            fillspace = strlen(header);
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
            int fillspace = (int)kw_get_int(gobj, jn_col, "fillspace", 20, KW_WILD_NUMBER);
            const char *header = kw_get_str(gobj, jn_col, "header", "", 0);
            const char *type = kw_get_str(gobj, jn_col, "type", "", 0);
            if(fillspace && fillspace < strlen(header)) {
                fillspace = (int)strlen(header);
            }
            if(fillspace > 0) {
                json_t *jn_cell = kw_get_dict_value(gobj, jn_row, id, 0, 0);
                char *text = json2uglystr(jn_cell);
                if(strcmp(type, "time")==0) {
                    GBMEM_FREE(text);
                    char stime[90];
                    t2timestamp(stime, sizeof(stime), json_integer_value(jn_cell), TRUE);
                    text = stime;
                    gbuffer_printf(gbuf, "%-*.*s ", fillspace, fillspace, text);
                    continue;
                }

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
    hgobj display_window,
    json_t *webix)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int result = kw_get_int(gobj, webix, "result", -1, 0);
    const char *comment = kw_get_str(gobj, webix, "comment", "", 0);
    json_t *jn_schema = kw_get_dict_value(gobj, webix, "schema", 0, 0);
    json_t *jn_data = kw_get_dict_value(gobj, webix, "data", 0, 0);
    if(!jn_data) {
        jn_data = webix;
        if(empty_string(comment)) {
            comment = "Not a webix response";
        }
    }

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

    if(result < 0) {
        json_t *jn_error = json_sprintf("ERROR %d: %s", result, comment);
        json_t *jn_text = json_pack("{s:o, s:s}",
            "text", jn_error,
            "bg_color", "red"
        );
        gobj_send_event(display_window, EV_SETTEXT, jn_text, gobj);
        // Pinta error en statusline, ya que no puedo pintar colores en la ventana-lista.
        msg2statusline(gobj, result, "Be careful! Response with ERROR.");
        if(priv->file_saving_output) {
            fprintf(priv->file_saving_output, "ERROR %d: %s\n", result, comment);
        }
        comment = 0; // Avoid re-display below
    } else {
        msg2statusline(gobj, 0, "%s", "");
    }

    if(json_is_array(jn_data)) {
        if (mode_form) {
            { // XXX if(json_array_size(jn_data)>0) {
                char *data = json2str(jn_data);
                json_t *jn_text = json_pack("{s:s}",
                    "text", data
                );
                gobj_send_event(display_window, EV_SETTEXT, jn_text, gobj);
                if(priv->file_saving_output) {
                    fprintf(priv->file_saving_output, "%s\n", data);
                }
                gbmem_free(data);
            }
        } else {
            /*
             *  display as table
             */
            if(jn_schema && json_array_size(jn_schema)) {
                gbuffer_t *gbuf = jsontable2str(jn_schema, jn_data);
                if(gbuf) {
                    char *p = gbuffer_cur_rd_pointer(gbuf);
                    json_t *jn_text = json_pack("{s:s}",
                        "text", p
                    );
                    gobj_send_event(display_window, EV_SETTEXT, jn_text, gobj);
                    if(priv->file_saving_output) {
                        fprintf(priv->file_saving_output, "%s\n", p);
                    }
                    gbuffer_decref(gbuf);
                }
            } else {
                { // XXX Display [] if empty array if(json_array_size(jn_data)>0) {
                    char *text = json2str(jn_data);
                    if(text) {
                        json_t *jn_text = json_pack("{s:s}",
                            "text", text
                        );
                        gobj_send_event(display_window, EV_SETTEXT, jn_text, gobj);
                        if(priv->file_saving_output) {
                            fprintf(priv->file_saving_output, "%s\n", text);
                        }
                        gbmem_free(text);
                    }
                }
            }
        }
    } else if(json_is_object(jn_data)) {
        char *data = json2str(jn_data);
        json_t *jn_text = json_pack("{s:s}",
            "text", data
        );
        gobj_send_event(display_window, EV_SETTEXT, jn_text, gobj);
        if(priv->file_saving_output) {
            fprintf(priv->file_saving_output, "%s\n", data);
        }
        gbmem_free(data);
    } else if(json_is_string(jn_data)) {
        const char *data = json_string_value(jn_data);
        json_t *jn_text = json_pack("{s:s}",
            "text", data
        );
        gobj_send_event(display_window, EV_SETTEXT, jn_text, gobj);
        if(priv->file_saving_output) {
            fprintf(priv->file_saving_output, "%s\n", data);
        }
    }

    if(!empty_string(comment)) {
        json_t *jn_text = json_pack("{s:s}",
            "text", comment
        );
        gobj_send_event(display_window, EV_SETTEXT, jn_text, gobj);
        if(priv->file_saving_output) {
            fprintf(priv->file_saving_output, "%s\n", comment);
        }
    }

    if(priv->file_saving_output) {
        fflush(priv->file_saving_output);
    }

    JSON_DECREF(webix);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int msg2display(hgobj gobj, const char *fmt, ...)
{
    va_list ap;
    char temp[4*1024];


    va_start(ap, fmt);
    vsnprintf(temp, sizeof(temp), fmt, ap);

    json_t *jn_text = json_pack("{s:s}",
        "text", temp
    );
    hgobj wn_display = get_top_display_window(gobj);
    gobj_send_event(wn_display, EV_SETTEXT, jn_text, gobj);

    va_end(ap);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int msg2statusline(hgobj gobj, BOOL error, const char *fmt, ...)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    va_list ap;
    char temp[120];

    va_start(ap, fmt);
    vsnprintf(temp, sizeof(temp), fmt, ap);
    if(error) {
        SetTextColor(priv->gobj_stsline, "red");
    } else {
        SetTextColor(priv->gobj_stsline, "black");
    }
    DrawText(priv->gobj_stsline, 0, 0, temp);
    va_end(ap);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void do_close(hgobj gobj)
{
    // PRIVATE_DATA *priv = gobj_priv_data(gobj);

    // if(!priv->uv_handler_active) {
    //     gobj_log_error(gobj, 0,
    //         "function",     "%s", __FUNCTION__,
    //         "msgset",       "%s", MSGSET_OPERATIONAL_ERROR,
    //         "msg",          "%s", "UV handler NOT ACTIVE!",
    //         NULL
    //     );
    //     return;
    // }
    // if(priv->uv_read_active) {
    //     uv_read_stop((uv_stream_t *)&priv->uv_tty);
    //     priv->uv_read_active = 0;
    // }
    //
    // if(gobj_trace_level(gobj) & TRACE_UV) {
    //     gobj_trace_msg(gobj, ">>> uv_close tty p=%p", &priv->uv_tty);
    // }
    // uv_close((uv_handle_t *)&priv->uv_tty, on_close_cb);
}

/***************************************************************************
 *  // new line
 ***************************************************************************/
PRIVATE void new_line(hgobj gobj, hgobj wn_display)
{
    json_t *jn_text = json_pack("{s:s}",
        "text", ""
    );
    gobj_send_event(wn_display, EV_SETTEXT, jn_text, gobj);
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
PRIVATE int save_local_string(hgobj gobj, char *path, int pathsize, const char *name, json_t *jn_content)
{
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
PRIVATE int save_local_base64(hgobj gobj, char *path, int pathsize, const char *name, json_t *jn_content)
{
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
                write(fp, gbuffer_cur_rd_pointer(gbuf_bin), gbuffer_leftbytes(gbuf_bin));
                close(fp);
            }
        }
    }
    JSON_DECREF(jn_content);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int edit_json(hgobj gobj, const char *path)
{
    const char *editor = gobj_read_str_attr(gobj, "editor");
    char command[NAME_MAX];
    snprintf(command, sizeof(command), "%s %s", editor, path);

    savetty();
    def_prog_mode();
    int ret = pty_sync_spawn(command);
    resetty();
    reset_prog_mode();
    doupdate();
    refresh();
    flushinp();
    return ret;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE const char *get_yunetas_base(void)
{
    // Define the default value
    const char* default_value = "/yuneta/development/outputs/yunos";

    // Get the value of the environment variable YUNETAS_YUNOS
    const char* yunetas_base = getenv("YUNETAS_YUNOS");

    // Return the environment variable value if it's set, otherwise the default value
    return yunetas_base ? yunetas_base : default_value;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE gbuffer_t *source2base64_for_yuneta(const char *source, char **comment)
{
    /*------------------------------------------------*
     *          Check source
     *  Frequently, You want install the output
     *  of your yuno's make install command.
     *------------------------------------------------*/
    if(empty_string(source)) {
        *comment = "source not found";
        return 0;
    }

    char path[NAME_MAX];
    if(access(source, 0)==0 && is_regular_file(source)) {
        snprintf(path, sizeof(path), "%s", source);
    } else {
        snprintf(path, sizeof(path), "/yuneta/development/output/yunos/%s", source);
    }

    if(access(path, 0)!=0) {
        *comment = "source not found";
        return 0;
    }
    if(!is_regular_file(path)) {
        *comment = "source is not a regular file";
        return 0;
    }
    gbuffer_t *gbuf_b64 = gbuffer_file2base64(path);
    if(!gbuf_b64) {
        *comment = "conversion to base64 failed";
    }
    return gbuf_b64;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE gbuffer_t *source2base64_for_yunetas(const char *source, char **comment)
{
    /*------------------------------------------------*
     *          Check source
     *  Frequently, You want install the output
     *  of your yuno's make install command.
     *------------------------------------------------*/
    if(empty_string(source)) {
        *comment = "source not found";
        return 0;
    }

    char path[NAME_MAX];
    if(access(source, 0)==0 && is_regular_file(source)) {
        snprintf(path, sizeof(path), "%s", source);
    } else {
        const char *yunetas_base = get_yunetas_base();
        build_path(path, sizeof(path), yunetas_base, source, NULL);
    }

    if(access(path, 0)!=0) {
        *comment = "source not found";
        return 0;
    }
    if(!is_regular_file(path)) {
        *comment = "source is not a regular file";
        return 0;
    }
    gbuffer_t *gbuf_b64 = gbuffer_file2base64(path);
    if(!gbuf_b64) {
        *comment = "conversion to base64 failed";
    }
    return gbuf_b64;
}

/***************************************************************************
 *  Used in yuneta classic
 *  $$ interfere with bash, use ^^ as alternative
 ***************************************************************************/
PRIVATE gbuffer_t * replace_cli_vars_for_yuneta(hgobj gobj, const char *command, char **comment)
{
    gbuffer_t *gbuf = gbuffer_create(4*1024, gbmem_get_maximum_block());
    char *command_ = gbmem_strdup(command);
    char *p = command_;

    const char *prefix = "$$";  // default
    if(strstr(p, "^^")) {
        prefix = "^^";
    }

    char *n, *f;
    while((n=strstr(p, prefix))) {
        *n = 0;
        gbuffer_append(gbuf, p, strlen(p));

        n += 2;
        if(*n == '(') {
            f = strchr(n, ')');
        } else {
            gbuffer_decref(gbuf);
            gbmem_free(command_);
            *comment = "Bad format of $$: use $$(...) or ^^(...)";
            return 0;
        }
        if(!f) {
            gbuffer_decref(gbuf);
            gbmem_free(command_);
            *comment = "Bad format of $$: use $$(...) or ^^(...)";
            return 0;
        }
        *n = 0;
        n++;
        *f = 0;
        f++;

        // YunetaS precedence over Yuneta
        gbuffer_t *gbuf_b64 = source2base64_for_yunetas(n, comment);
        if(!gbuf_b64) {
            gbuf_b64 = source2base64_for_yuneta(n, comment);
            if(!gbuf_b64) {
                gbuffer_decref(gbuf);
                gbmem_free(command_);
                return 0;
            }
        }

        gbuffer_append(gbuf, "'", 1);
        gbuffer_append_gbuf(gbuf, gbuf_b64);
        gbuffer_append(gbuf, "'", 1);
        gbuffer_decref(gbuf_b64);

        p = f;
    }
    if(!empty_string(p)) {
        gbuffer_append(gbuf, p, strlen(p));
    }

    gbmem_free(command_);
    return gbuf;
}

/***************************************************************************
 *  Busca el shortkey 'key, y si existe ponlo en bf.
 *  Retorna bf si hay shortkey, o null sino.
 ***************************************************************************/
PRIVATE BOOL filter_by_shortkeys(hgobj gobj, char *bf, int bfsize, const char *key)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *jn_command = json_object_get(priv->jn_shortkeys, key);
    if(jn_command) {
        const char *command = json_string_value(jn_command);
        snprintf(bf, bfsize, "%s", command);
        return TRUE;
    }

    return FALSE;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *  HACK Este evento solo puede venir de GCLASS_WN_EDITLINE
 ***************************************************************************/
PRIVATE int ac_command(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    //PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_input_command = json_object();
    gobj_send_event(src, EV_GETTEXT, kw_input_command, gobj); // EV_GETTEXT is EVF_KW_WRITING
    const char *command = kw_get_str(gobj, kw_input_command, "text", 0, 0);

    char params_[4*1024];
    snprintf(params_, sizeof(params_), "%s", command);
    char *save_ptr = 0;
    char *key = get_parameter(params_, &save_ptr);

    /*
     *  Using variables $1 $2 .. like bash, only for shortkeys
     */
    char command_[4*1024];
    if(filter_by_shortkeys(gobj, command_, sizeof(command_), key)) {
        char *dolar_content = save_ptr;
        int i = 1;
        while((dolar_content = get_parameter(dolar_content, &save_ptr))) {
            char dolar_n[32];
            snprintf(dolar_n, sizeof(dolar_n), "$%d", i);
            char *rs = replace_string(command_, dolar_n, dolar_content);
            snprintf(command_, sizeof(command_), "%s", rs);
            free(rs);

            dolar_content = save_ptr;
            i++;
        }

        command = command_;
    }

    /*
     *  Select the destination of command: what output window is active?
     */
    hgobj wn_display = get_top_display_window(gobj);
    if(wn_display) {
        if(empty_string(command)) {
            display_webix_result(
                gobj,
                wn_display,
                msg_iev_build_response(
                    gobj,
                    0,
                    json_sprintf("\n"),
                    0,
                    0,
                    0
                )
            );
            KW_DECREF(kw_input_command);
            KW_DECREF(kw);
            return 0;

        }
        display_webix_result(
            gobj,
            wn_display,
            msg_iev_build_response(
                gobj,
                0,
                json_sprintf("> %s", command),
                0,
                0,
                0
            )
        );

        char *comment;
        gbuffer_t *gbuf_parsed_command = 0;
        gbuf_parsed_command = replace_cli_vars_for_yuneta(gobj, command, &comment);

        if(!gbuf_parsed_command) {
            display_webix_result(
                gobj,
                wn_display,
                msg_iev_build_response(
                    gobj,
                    -1,
                    json_sprintf("%s", comment),
                    0,
                    0,
                    0
                )
            );
            KW_DECREF(kw_input_command);
            KW_DECREF(kw);
            return 0;
        }
        hgobj gobj_cmd;
        char *xcmd = gbuffer_cur_rd_pointer(gbuf_parsed_command);
        json_t *kw_command = json_object();
        if(*xcmd == '!') {
            xcmd++;
            gobj_cmd = gobj;
        } else if(*xcmd == '*') {
            xcmd++;
            kw_set_subdict_value(gobj, kw_command, "__md_iev__", "display_mode", json_string("form"));
            gobj_cmd = gobj_read_pointer_attr(wn_display, "user_data");
        } else {
            gobj_cmd = gobj_read_pointer_attr(wn_display, "user_data");
        }
        json_t *webix;
        if(gobj_cmd) {
            webix = gobj_command(gobj_cmd, xcmd, kw_command, gobj);
        } else {
            json_decref(kw_command);
            webix = msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Window without connection"),
                0,
                0,
                0
            );
        }
        gbuffer_decref(gbuf_parsed_command);

        /*
         *  Print json response in display window
         */
        if(webix) {
            display_webix_result(
                gobj,
                wn_display,
                webix
            );
        } else {
            /* asynchronous responses return 0 */
            msg2statusline(gobj, 0, "Waiting response...");
        }
    }

    /*
     *  Clear input line
     */
    json_object_set_new(kw_input_command, "text", json_string(""));
    gobj_send_event(src, EV_SETTEXT, kw_input_command, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Quit
 ***************************************************************************/
PRIVATE int ac_quit(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int timeout = kw_get_int(gobj, kw, "timeout", 0, 0);
    if(timeout) {
        set_timeout(priv->timer, timeout * 1000);
        KW_DECREF(kw);
        return 0;
    }
    set_yuno_must_die();
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  From GCLASS_WN_STDSCR
 *  On screen size change, set new edit line position
 ***************************************************************************/
PRIVATE int ac_screen_size_change(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    //PRIVATE_DATA *priv = gobj_priv_data(gobj);

    //int y = gobj_read_integer_attr(priv->gobj_editbox, "y");
    // TODO set pos editline

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_previous_window(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    json_t *kw_sel = json_pack("{s:s}",
        "selected", ""
    );
    gobj_send_event(priv->gobj_toptoolbar, EV_GET_PREV_SELECTED_BUTTON, kw_sel, gobj);
    set_top_window(gobj, kw_get_str(gobj, kw_sel, "selected", "", 0));
    KW_DECREF(kw_sel);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_next_window(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_t *kw_sel = json_pack("{s:s}",
        "selected", ""
    );
    gobj_send_event(priv->gobj_toptoolbar, EV_GET_NEXT_SELECTED_BUTTON, kw_sel, gobj);
    set_top_window(gobj, kw_get_str(gobj, kw_sel, "selected", "", 0));
    KW_DECREF(kw_sel);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    const char *agent_name = kw_get_str(gobj, kw, "remote_yuno_name", 0, 0); // remote agent name
    char temp[NAME_MAX];

    /*
     *  Create display window of external agent
     */
    hgobj wn_disp = get_display_window(gobj, agent_name);
    if(wn_disp) {
        int n = (int)kw_get_int(gobj, priv->jn_window_counters, agent_name, 0, KW_CREATE);
        n++;
        snprintf(temp, sizeof(temp), "%s-%d", agent_name, n);
        json_object_set_new(priv->jn_window_counters, agent_name, json_integer(n));
        agent_name = temp;
    }

    wn_disp = create_display_window(gobj, agent_name, 0);

    /*
     *  Create button window of console (right now implemented as static window)
     */
    create_static(gobj, agent_name, 0);
    set_top_window(gobj, agent_name);

    gobj_write_pointer_attr(wn_disp, "user_data", src);
    gobj_write_pointer_attr(src, "user_data", wn_disp);

    hgobj wn_display_console = get_display_window(gobj, "console");
    display_webix_result(
        gobj,
        wn_display_console,
        msg_iev_build_response(
            gobj,
            0,
            json_sprintf("Connected to '%s'.\n\n", agent_name),
            0,
            0,
            0
        )
    );

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_is_running(gobj)) {
        KW_DECREF(kw);
        return 0;
    }

    hgobj wn_disp = gobj_read_pointer_attr(src, "user_data");
    const char *agent_name = gobj_name(wn_disp);

    json_object_set_new(priv->jn_window_counters, agent_name, json_integer(0));

    destroy_static(gobj, agent_name);
    destroy_display_window(gobj, agent_name);

    hgobj wn_display_console = get_display_window(gobj, "console");
    display_webix_result(
        gobj,
        wn_display_console,
        msg_iev_build_response(
            gobj,
            0,
            json_sprintf("Disconnected from '%s'.\n\n", agent_name),
            0,
            0,
            0
        )
    );

    set_top_window(gobj, "console");

    // No puedo parar y destruir con libuv.
    // De momento conexiones indestructibles, destruibles solo con la salida del yuno.
    // Hasta que quite la dependencia de libuv. FUTURE
    //gobj_stop_tree(src);
    //gobj_destroy(tree);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Response from agent mt_stats
 *  Response from agent mt_command
 *  Response to asynchronous queries
 ***************************************************************************/
PRIVATE int ac_mt_command_answer(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    hgobj wn_display = gobj_read_pointer_attr(src, "user_data");
    display_webix_result(
        gobj,
        wn_display,
        kw  // owned
    );

    new_line(gobj, wn_display);
    SetFocus(priv->gobj_editline);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_edit_config(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    hgobj wn_display = gobj_read_pointer_attr(src, "user_data");

    int result = (int)kw_get_int(gobj, kw, "result", -1, 0);
    if(result < 0) {
        return display_webix_result(
            gobj,
            wn_display,
            kw  // owned
        );
    }
    json_t *record = json_array_get(kw_get_dict_value(gobj, kw, "data", 0, 0), 0);
    if(!record) {
        return display_webix_result(
            gobj,
            wn_display,
            msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Internal error, no data"),
                0,
                0,
                kw  // owned
            )
        );
    }

    const char *id = kw_get_str(gobj, record, "id", 0, 0);
    if(!id) {
        return display_webix_result(
            gobj,
            wn_display,
            msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Internal error, no id"),
                0,
                0,
                kw  // owned
            )
        );
    }

    json_t *jn_content = kw_get_dict_value(gobj, record, "zcontent", 0, 0);
    if(!jn_content) {
        return display_webix_result(
            gobj,
            wn_display,
            msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Internal error, no content"),
                0,
                0,
                kw  // owned
            )
        );
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
        return display_webix_result(
            gobj,
            wn_display,
            msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Bad json format in '%s' source. Line %d, Column %d, Error '%s'",
                    path,
                    error.line,
                    error.column,
                    error.text
                ),
                0,
                0,
                kw  // owned
            )
        );
    }
    JSON_DECREF(jn_new_content);

    char upgrade_command[512];
    snprintf(upgrade_command, sizeof(upgrade_command),
        "create-config id='%s' content64=$$(%s) ",
        id,
        path
    );

    json_t *jn_text = json_pack("{s:s}",
        "text", upgrade_command
    );
    gobj_send_event(priv->gobj_editline, EV_SETTEXT, jn_text, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_view_config(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    hgobj wn_display = gobj_read_pointer_attr(src, "user_data");

    int result = kw_get_int(gobj, kw, "result", -1, 0);
    if(result < 0) {
        return display_webix_result(
            gobj,
            wn_display,
            kw  // owned
        );
    }
    json_t *record = json_array_get(kw_get_dict_value(gobj, kw, "data", 0, 0), 0);
    if(!record) {
        return display_webix_result(
            gobj,
            wn_display,
            msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Internal error, no data"),
                0,
                0,
                kw  // owned
            )
        );
    }

    json_t *jn_content = kw_get_dict_value(gobj, record, "zcontent", 0, 0);
    if(!jn_content) {
        JSON_INCREF(record);
        return display_webix_result(
            gobj,
            wn_display,
            msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Internal error, no zcontent"),
                0,
                record,
                kw  // owned
            )
        );
    }
    const char *name = kw_get_str(gobj, record, "name", "__temporal__", 0);
    JSON_INCREF(jn_content);
    char path[NAME_MAX];

    save_local_json(gobj, path, sizeof(path), name, jn_content);
    //log_debug_printf("save_local_json %s", path);
    edit_json(gobj, path);

    new_line(gobj, wn_display);
    msg2statusline(gobj, 0, "%s", "");

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_read_json(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    hgobj wn_display = gobj_read_pointer_attr(src, "user_data");

    int result = kw_get_int(gobj, kw, "result", -1, 0);
    if(result < 0) {
        return display_webix_result(
            gobj,
            wn_display,
            kw  // owned
        );
    }
    json_t *record = kw_get_dict_value(gobj, kw, "data", 0, 0);
    if(!record) {
        return display_webix_result(
            gobj,
            wn_display,
            msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Internal error, no data"),
                0,
                0,
                kw  // owned
            )
        );
    }

    json_t *jn_content = kw_get_dict_value(gobj, record, "zcontent", 0, 0);
    if(!jn_content) {
        return display_webix_result(
            gobj,
            wn_display,
            msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Internal error, no content"),
                0,
                0,
                kw  // owned
            )
        );
    }
    const char *name = kw_get_str(gobj, record, "name", "__temporal__", 0);
    JSON_INCREF(jn_content);
    char path[NAME_MAX];

    save_local_json(gobj, path, sizeof(path), name, jn_content);
    //log_debug_printf("save_local_json %s", path);
    edit_json(gobj, path);

    new_line(gobj, wn_display);
    msg2statusline(gobj, 0, "%s", "");

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_read_file(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    hgobj wn_display = gobj_read_pointer_attr(src, "user_data");

    int result = kw_get_int(gobj, kw, "result", -1, 0);
    if(result < 0) {
        return display_webix_result(
            gobj,
            wn_display,
            kw  // owned
        );
    }
    json_t *record = kw_get_dict_value(gobj, kw, "data", 0, 0);
    if(!record) {
        return display_webix_result(
            gobj,
            wn_display,
            msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Internal error, no data"),
                0,
                0,
                kw  // owned
            )
        );
    }

    json_t *jn_content = kw_get_dict_value(gobj, record, "zcontent", 0, 0);
    if(!jn_content) {
        return display_webix_result(
            gobj,
            wn_display,
            msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Internal error, no content"),
                0,
                0,
                kw  // owned
            )
        );
    }
    const char *name = kw_get_str(gobj, record, "name", "__temporal__", 0);
    JSON_INCREF(jn_content);
    char path[NAME_MAX];

    save_local_string(gobj, path, sizeof(path), name, jn_content);
    //log_debug_printf("save_local_json %s", path);
    edit_json(gobj, path);

    new_line(gobj, wn_display);
    msg2statusline(gobj, 0, "%s", "");

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_read_binary_file(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    hgobj wn_display = gobj_read_pointer_attr(src, "user_data");

    int result = kw_get_int(gobj, kw, "result", -1, 0);
    if(result < 0) {
        return display_webix_result(
            gobj,
            wn_display,
            kw  // owned
        );
    }
    json_t *record = kw_get_dict_value(gobj, kw, "data", 0, 0);
    if(!record) {
        return display_webix_result(
            gobj,
            wn_display,
            msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Internal error, no data"),
                0,
                0,
                kw  // owned
            )
        );
    }

    json_t *jn_content = kw_get_dict_value(gobj, record, "content64", 0, 0);
    if(!jn_content) {
        return display_webix_result(
            gobj,
            wn_display,
            msg_iev_build_response(
                gobj,
                -1,
                json_sprintf("Internal error, no content64"),
                0,
                0,
                kw  // owned
            )
        );
    }
    const char *name = kw_get_str(gobj, record, "name", "__temporal__", 0);
    JSON_INCREF(jn_content);
    char path[NAME_MAX];
    save_local_base64(gobj, path, sizeof(path), name, jn_content);

    msg2statusline(gobj, 0, "Binary file save in '%s'", path);
    new_line(gobj, wn_display);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  Response from agent
 ***************************************************************************/
PRIVATE int ac_agent_response(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    hgobj wn_display = gobj_read_pointer_attr(src, "user_data");
    display_webix_result(
        gobj,
        wn_display,
        kw  // owned
    );

    new_line(gobj, wn_display);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_token(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    const char *comment = kw_get_str(gobj, kw, "comment", "", 0);
    int result = kw_get_int(gobj, kw, "result", -1, KW_REQUIRED);
    if(result < 0) {
        msg2statusline(gobj, TRUE, "%s", comment);

    } else {
        const char *jwt = kw_get_str(gobj, kw, "jwt", "", KW_REQUIRED);
        gobj_write_str_attr(gobj, "jwt", jwt);
        msg2statusline(gobj, FALSE, "%s", comment);
    }

    gobj_stop(src);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
#ifdef TEST_KDEVELOP_DIE
    gobj_set_yuno_must_die();
#endif
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_mirror_open(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
//     const char *agent_name = gobj_name(gobj_read_pointer_attr(src, "user_data"));
//
//     json_t *jn_data = kw_get_dict(gobj, kw, "data", 0, KW_EXTRACT|KW_REQUIRED);
//     const char *tty_name = kw_get_str(gobj, jn_data, "name", "", KW_REQUIRED);
//
//     char window_tty_mirror_name[NAME_MAX];
//     snprintf(window_tty_mirror_name, sizeof(window_tty_mirror_name), "%s(%s)", agent_name, tty_name);
//
//     /*
//      *  Create display window of external agent
//      */
//     hgobj wn_tty_mirror_disp = create_tty_mirror_window(gobj, window_tty_mirror_name, jn_data);
//     if(wn_tty_mirror_disp) {
//         char name_[NAME_MAX+20];
//         snprintf(name_, sizeof(name_), "consoles`%s", window_tty_mirror_name);
//         gobj_kw_get_user_data( // save in input gate
//             src,
//             name_,
//             json_true(), // owned
//             KW_CREATE
//         );
//             gobj_write_pointer_attr(wn_tty_mirror_disp, "user_data", src);
//
//         /*
//          *  Create button window of console (right now implemented as static window)
//          */
//         create_static(gobj, window_tty_mirror_name, 0);
//         set_top_window(gobj, window_tty_mirror_name);
//     }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_mirror_close(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    if(!gobj_is_running(gobj)) {
        KW_DECREF(kw);
        return 0;
    }

//     const char *agent_name = gobj_name(gobj_read_pointer_attr(src, "user_data"));
//     const char *tty_name = kw_get_str(gobj, kw, "data`name", 0, 0);
//     char window_tty_mirror_name[NAME_MAX];
//     snprintf(window_tty_mirror_name, sizeof(window_tty_mirror_name), "%s(%s)", agent_name, tty_name);
//
//     hgobj wn_tty_mirror_disp = get_tty_mirror_window(gobj, window_tty_mirror_name);
//     if(wn_tty_mirror_disp) {
//         char name_[NAME_MAX];
//         snprintf(name_, sizeof(name_), "consoles`%s", tty_name);
//         json_decref(gobj_kw_get_user_data( // delete in input gate
//             src,
//             name_,
//             json_true(), // owned
//             KW_EXTRACT
//         ));
//
//         destroy_static(gobj, window_tty_mirror_name);
//         destroy_tty_mirror_window(gobj, window_tty_mirror_name);
//     }
//
//     set_top_window(gobj, agent_name);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_tty_mirror_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
//     const char *agent_name = gobj_name(gobj_read_pointer_attr(src, "user_data"));
//
//     json_t *jn_data = kw_get_dict(gobj, kw, "data", 0, KW_EXTRACT|KW_REQUIRED);
//     if(jn_data) {
//         const char *tty_name = kw_get_str(gobj, jn_data, "name", 0, 0);
//         char window_tty_mirror_name[NAME_MAX];
//         snprintf(window_tty_mirror_name, sizeof(window_tty_mirror_name), "%s(%s)", agent_name, tty_name);
//
//         hgobj wn_tty_mirror_disp = get_tty_mirror_window(gobj, window_tty_mirror_name);
//         if(wn_tty_mirror_disp) {
//             gobj_send_event(wn_tty_mirror_disp, EV_WRITE_TTY, jn_data, src);
//         } else {
//             json_decref(jn_data);
//         }
//     }

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
    .mt_create          = mt_create,
    .mt_destroy         = mt_destroy,
    .mt_start           = mt_start,
    .mt_stop            = mt_stop,
    .mt_writing         = mt_writing,
    .mt_inject_event    = mt_inject_event,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_CLI);

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
        {EV_COMMAND,              ac_command,             0},
        {EV_QUIT,                 ac_quit,                0},
        {EV_SCREEN_SIZE_CHANGE,   ac_screen_size_change,  0},
        {EV_PREVIOUS_WINDOW,      ac_previous_window,     0},
        {EV_NEXT_WINDOW,          ac_next_window,         0},
        {EV_ON_OPEN,              ac_on_open,             0},
        {EV_ON_CLOSE,             ac_on_close,            0},
        {EV_MT_STATS_ANSWER,      ac_mt_command_answer,   0},
        {EV_MT_COMMAND_ANSWER,    ac_mt_command_answer,   0},
        {EV_EDIT_CONFIG,          ac_edit_config,         0},
        {EV_VIEW_CONFIG,          ac_view_config,         0},
        {EV_EDIT_YUNO_CONFIG,     ac_edit_config,         0},
        {EV_VIEW_YUNO_CONFIG,     ac_view_config,         0},
        {EV_READ_JSON,            ac_read_json,           0},
        {EV_READ_FILE,            ac_read_file,           0},
        {EV_READ_BINARY_FILE,     ac_read_binary_file,    0},
        {EV_TTY_OPEN,             ac_tty_mirror_open,     0},
        {EV_TTY_CLOSE,            ac_tty_mirror_close,    0},
        {EV_TTY_DATA,             ac_tty_mirror_data,     0},
        {EV_ON_TOKEN,             ac_on_token,            0},
        {EV_TIMEOUT,              ac_timeout,             0},
        {EV_STOPPED,              0,                      0},
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
        {EV_EDIT_CONFIG,          EVF_PUBLIC_EVENT},
        {EV_VIEW_CONFIG,          EVF_PUBLIC_EVENT},
        {EV_EDIT_YUNO_CONFIG,     EVF_PUBLIC_EVENT},
        {EV_VIEW_YUNO_CONFIG,     EVF_PUBLIC_EVENT},
        {EV_READ_JSON,            EVF_PUBLIC_EVENT},
        {EV_READ_FILE,            EVF_PUBLIC_EVENT},
        {EV_READ_BINARY_FILE,     EVF_PUBLIC_EVENT},
        {EV_MT_STATS_ANSWER,      EVF_PUBLIC_EVENT},
        {EV_MT_COMMAND_ANSWER,    EVF_PUBLIC_EVENT},
        {EV_TTY_OPEN,             EVF_PUBLIC_EVENT},
        {EV_TTY_CLOSE,            EVF_PUBLIC_EVENT},
        {EV_TTY_DATA,             EVF_PUBLIC_EVENT},

        {EV_COMMAND,              0},
        {EV_QUIT,                 0},
        {EV_SCREEN_SIZE_CHANGE,   0},
        {EV_PREVIOUS_WINDOW,      0},
        {EV_NEXT_WINDOW,          0},
        {EV_ON_OPEN,              0},
        {EV_ON_CLOSE,             0},
        {EV_ON_TOKEN,             0},
        {EV_TIMEOUT,              0},
        {EV_STOPPED,              0},
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
        0,                  // Local method table
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,                  // Authorization table
        command_table,
        s_user_trace_level,
        0                   // GClass flags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_cli(void)
{
    return create_gclass(C_CLI);
}
