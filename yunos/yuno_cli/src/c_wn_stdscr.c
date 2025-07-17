/***********************************************************************
 *          C_WN_STDSCR.C
 *          Wn_stdscr GClass.
 *
 *          Copyright (c) 2015-2016 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <signal.h>
#include <sys/ioctl.h>
#include <ncurses/ncurses.h>
#include <string.h>
#include "c_wn_stdscr.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int get_color_pair(int fg, int bg);
PRIVATE int get_color_id(const char *color);
PRIVATE void my_endwin(void) { endwin();}
PRIVATE void catch_signals(void);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE struct {
    int id;
    const char *name;
} table_id[] = {
    {COLOR_BLACK,   "black"},
    {COLOR_RED,     "red"},
    {COLOR_GREEN,   "green"},
    {COLOR_YELLOW,  "yellow"},
    {COLOR_BLUE,    "blue"},
    {COLOR_MAGENTA, "magenta"},
    {COLOR_CYAN,    "cyan"},
    {COLOR_WHITE,   "white"}
};

// When SetFocus is rejected (return -1) the focus will be set int __gobj_default_focus__
PRIVATE hgobj __gobj_default_focus__ = 0;

PRIVATE hgobj __gobj_with_focus__ = 0;
PRIVATE char __new_stdsrc_size__ = FALSE;

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
SDATA (ASN_INTEGER,     "timeout",              0,  500, "Timeout, to detect size change in stdscr"),
SDATA (ASN_INTEGER,     "cx",                   0,  0, "cx window size"),
SDATA (ASN_INTEGER,     "cy",                   0,  0, "cy window size"),

SDATA (ASN_POINTER,     "user_data",            0,  0, "user data"),
SDATA (ASN_POINTER,     "user_data2",           0,  0, "more user data"),
SDATA (ASN_POINTER,     "subscriber",           0,  0, "subscriber of output-events. If it's null then subscriber is the parent."),
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
    hgobj timer;
    int32_t timeout;

    WINDOW *wn;      // ncurses handler
    uint32_t cx;
    uint32_t cy;

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
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber)
        subscriber = gobj_parent(gobj);
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,      gobj_read_int32_attr)

    /*
     *  Start up ncurses
     */

    catch_signals();
    atexit(my_endwin);

    /*
     *  stdscr timer to detect window size change
     */
    priv->timer = gobj_create("", GCLASS_TIMER, 0, gobj);

#ifndef TEST_KDEVELOP
    priv->wn = initscr();               /* Start curses mode            */
    cbreak();                           /* Line buffering disabled      */
    noecho();                           /* Don't echo() while we do getch */
    keypad(priv->wn, TRUE);             /* We get F1, F2 etc..          */
    halfdelay(1);
    //wtimeout(priv->wn, 10);              /* input non-blocking, wait 1 msec */
    if(has_colors()) {
        start_color();
    }
#endif
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,         gobj_read_int32_attr)
        if(gobj_is_running(gobj)) {
            set_timeout_periodic(priv->timer, priv->timeout);
        }
    ELIF_EQ_SET_PRIV(cx,            gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(cy,            gobj_read_int32_attr)
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


    int cx, cy;
    get_stdscr_size(&cx, &cy);
    gobj_write_int32_attr(gobj, "cx", cx);
    gobj_write_int32_attr(gobj, "cy", cy);

    //log_debug_printf(0, "initial size cx %d cy %d %s\n", cx, cy, gobj_name(gobj));

    json_t *jn_kw = json_object();
    json_object_set_new(jn_kw, "cx", json_integer(cx));
    json_object_set_new(jn_kw, "cy", json_integer(cy));
    gobj_send_event_to_childs(gobj, "EV_SIZE", jn_kw, gobj);

    gobj_start_childs(gobj);
    //wrefresh(priv->wn);

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
    gobj_stop_childs(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int get_color_pair(int fg, int bg)
{
    static int next_pair = 1;
    int pair;
    static int cp[8][8];

    if(fg < 0 || fg >= 8 || bg < 0 || bg >= 8) {
        return 0;
    }
    if ((pair = cp[fg][bg])) {
        return COLOR_PAIR(pair);
    }
    if (next_pair >= COLOR_PAIRS) {
        return 0;
    }
    if (init_pair(next_pair, fg, bg) != OK){
        return 0;
    }
    pair = cp[fg][bg] = next_pair++;
    return COLOR_PAIR(pair);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int get_color_id(const char *color)
{
    int i;
    int len = ARRAY_NSIZE(table_id);
    for(i=0; i<len; i++) {
        if(strcasecmp(table_id[i].name, color)==0) {
            return table_id[i].id;
        }
    }
    return COLOR_WHITE;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int get_stdscr_size(int *w, int *h)
{
    struct winsize size;

    if(ioctl(fileno(stdout), TIOCGWINSZ, &size) == 0) {
        resizeterm (size.ws_row, size.ws_col);
        if(size.ws_col <= 0) {
            size.ws_col = 80;
        }
        if(size.ws_row <= 0) {
            size.ws_row = 24;
        }
        *w = size.ws_col;
        *h = size.ws_row;
    } else {
        int new_width, new_height;
        getmaxyx(stdscr, new_height, new_width);
        if(new_width < 0) {
            new_width = 0;
        }
        if(new_height < 0) {
            new_height = 0;
        }

        *w = new_width;
        *h = new_height;
    }
    //printf("w=%d, h=%d\r\n", *w, *h);
    return 0;
}

/***************************************************************************
 *      Signal handlers
 ***************************************************************************/
PRIVATE void sighandler(int sig)
{
    if(sig == SIGWINCH) {
        __new_stdsrc_size__ = TRUE;
    }
}

PRIVATE void catch_signals(void)
{
    struct sigaction sigIntHandler;

    memset(&sigIntHandler, 0, sizeof(sigIntHandler));
    sigIntHandler.sa_handler = sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGWINCH, &sigIntHandler, NULL);
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    if(__new_stdsrc_size__) {
        __new_stdsrc_size__ = FALSE;
        int cx, cy;
        get_stdscr_size(&cx, &cy);
        gobj_write_int32_attr(gobj, "cx", cx);
        gobj_write_int32_attr(gobj, "cy", cy);

        json_t *jn_kw = json_object();
        json_object_set_new(jn_kw, "cx", json_integer(cx));
        json_object_set_new(jn_kw, "cy", json_integer(cy));

        json_incref(jn_kw);
        gobj_send_event_to_childs(gobj, "EV_SIZE", jn_kw, gobj);

        gobj_publish_event(gobj, "EV_SCREEN_SIZE_CHANGE", jn_kw);
    }
    KW_DECREF(kw);
    return 0;
}


/***************************************************************************
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
//    {"EV_SIZE",         0,  0,  0},
//     {"EV_PAINT",        0,  0,  0},
//     {"EV_HSCROLL",      0,  0,  0},
//     {"EV_VSCROLL",      0,  0,  0},
//     {"EV_MOUSE",        0,  0,  0},
//     {"EV_KEY",          0,  0,  0},
//     {"EV_KILLFOCUS",    0,  0,  0},
//     {"EV_SETFOCUS",     0,  0,  0},
//     {"EV_MOVE",         0,  0,  0},
    {"EV_TIMEOUT",      0,  0,  0},
    {"EV_STOPPED",      0,  0,  0},
    {NULL, 0, 0, 0}
};
PRIVATE const EVENT output_events[] = {
    {"EV_SCREEN_SIZE_CHANGE",   0,  0,  0},
    {NULL, 0, 0, 0}
};
PRIVATE const char *state_names[] = {
    "ST_IDLE",
    NULL
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {"EV_TIMEOUT",          ac_timeout,         0},
    {"EV_STOPPED",          0,                  0},
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_IDLE,
    NULL
};

PRIVATE FSM fsm = {
    input_events,
    output_events,
    state_names,
    states,
};

/***************************************************************************
 *              GClass
 ***************************************************************************/
/*---------------------------------------------*
 *              Local methods table
 *---------------------------------------------*/
PRIVATE LMETHOD lmt[] = {
    {0, 0, 0}
};

/*---------------------------------------------*
 *              GClass
 *---------------------------------------------*/
PRIVATE GCLASS _gclass = {
    0,  // base
    GCLASS_WN_STDSCR_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        mt_destroy,
        mt_start,
        mt_stop,
        0, //mt_play,
        0, //mt_pause,
        mt_writing,
        0, //mt_reading,
        0, //mt_subscription_added,
        0, //mt_subscription_deleted,
        0, //mt_child_added,
        0, //mt_child_removed,
        0, //mt_stats,
        0, //mt_command,
        0, //mt_inject_event,
        0, //mt_create_resource,
        0, //mt_list_resource,
        0, //mt_save_resource,
        0, //mt_delete_resource,
        0, //mt_future21
        0, //mt_future22
        0, //mt_get_resource
        0, //mt_state_changed,
        0, //mt_authenticate,
        0, //mt_list_childs,
        0, //mt_stats_updated,
        0, //mt_disable,
        0, //mt_enable,
        0, //mt_trace_on,
        0, //mt_trace_off,
        0, //mt_gobj_created,
        0, //mt_future33,
        0, //mt_future34,
        0, //mt_publish_event,
        0, //mt_publication_pre_filter,
        0, //mt_publication_filter,
        0, //mt_authz_checker,
        0, //mt_future39,
        0, //mt_create_node,
        0, //mt_update_node,
        0, //mt_delete_node,
        0, //mt_link_nodes,
        0, //mt_future44,
        0, //mt_unlink_nodes,
        0, //mt_topic_jtree,
        0, //mt_get_node,
        0, //mt_list_nodes,
        0, //mt_shoot_snap,
        0, //mt_activate_snap,
        0, //mt_list_snaps,
        0, //mt_treedbs,
        0, //mt_treedb_topics,
        0, //mt_topic_desc,
        0, //mt_topic_links,
        0, //mt_topic_hooks,
        0, //mt_node_parents,
        0, //mt_node_childs,
        0, //mt_list_instances,
        0, //mt_node_tree,
        0, //mt_topic_size,
        0, //mt_future62,
        0, //mt_future63,
        0, //mt_future64
    },
    lmt,
    tattr_desc,
    sizeof(PRIVATE_DATA),
    0,  // acl
    s_user_trace_level,
    0, // cmds
    0, // gcflag
};

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC GCLASS *gclass_wn_stdscr(void)
{
    return &_gclass;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int _get_curses_color(const char *fg_color, const char *bg_color)
{
    return get_color_pair(
        get_color_id(fg_color),
        get_color_id(bg_color)
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int SetDefaultFocus(hgobj gobj)
{
    __gobj_default_focus__ = gobj;
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int SetFocus(hgobj gobj)
{
    if(gobj != __gobj_with_focus__) {
        // Si hay nuevo focus avisa al antiguo
        if(__gobj_with_focus__) {
            gobj_send_event(__gobj_with_focus__, "EV_KILLFOCUS", 0, 0);
            if(gobj_trace_level(gobj) & (TRACE_MACHINE|TRACE_EV_KW)) {
                trace_msg("👁 👁 ⏪ %s", gobj_short_name(__gobj_with_focus__));
            }
        }
    }

    __gobj_with_focus__ = gobj;
    if(gobj_send_event(__gobj_with_focus__, "EV_SETFOCUS", 0, 0)<0) {
        // Si el nuevo focus no acepta pon el default focus
        __gobj_with_focus__ = __gobj_default_focus__;
        gobj_send_event(__gobj_default_focus__, "EV_SETFOCUS", 0, 0);
        if(gobj_trace_level(gobj) & (TRACE_MACHINE|TRACE_EV_KW)) {
            trace_msg("👁 👁 ⏩ %s", gobj_short_name(__gobj_with_focus__));
        }
    } else {
        if(gobj_trace_level(gobj) & (TRACE_MACHINE|TRACE_EV_KW)) {
            trace_msg("👁 👁 ⏩ %s", gobj_short_name(__gobj_with_focus__));
        }
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC hgobj GetFocus(void)
{
    return __gobj_with_focus__;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int SetTextColor(hgobj gobj, const char *color)
{
    return gobj_write_str_attr(gobj, "fg_color", color);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int SetBkColor(hgobj gobj, const char *color)
{
    return gobj_write_str_attr(gobj, "bk_color", color);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int DrawText(hgobj gobj, int x, int y, const char *s)
{
    json_t *kw = json_pack("{s:s, s:i, s:i}",
        "text", s,
        "x", x,
        "y", y
    );
    gobj_send_event(gobj, "EV_SETTEXT", kw, gobj);
    if(__gobj_with_focus__) {
        gobj_send_event(__gobj_with_focus__, "EV_SETFOCUS", 0, 0);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int cb_play(rc_instance_t *i_gobj, hgobj gobj, void *user_data, void *user_data2, void *user_data3)
{
    gobj_play(gobj);
    return 0;
}
PRIVATE int cb_pause(rc_instance_t *i_gobj, hgobj gobj, void *user_data, void *user_data2, void *user_data3)
{
    gobj_pause(gobj);
    return 0;
}
PUBLIC int EnableWindow(hgobj gobj, BOOL enable)
{
    if(enable) {
        gobj_walk_gobj_childs_tree(gobj, WALK_TOP2BOTTOM, cb_play, 0, 0, 0);
        // TODO send EV_ACTIVATE si es una window padre
    } else {
        gobj_walk_gobj_childs_tree(gobj, WALK_TOP2BOTTOM, cb_pause, 0, 0, 0);
    }
    return 0;
}
