/***********************************************************************
 *          C_WN_STDSCR.C
 *          Wn_stdscr GClass.
 *
 *          Windows Framework with ncurses
 *
 *          The goal of wn_stdscr is :
 *              - detect changes in stdscr size and inform to all his children (they must be c_box)
 *              - centralize the background/foreground colors
 *              - centralize what window has the focus
 *              - generic write function
 *
 *          - Catch the signal SIGWINCH for changes of screen size: set the __new_stdsrc_size__ to TRUE.
*          - The timeout check the variable and if true:
 *              - set __new_stdsrc_size__ to FALSE
 *              - sends the message EV_SIZE to the children
 *              - publishes the message EV_SCREEN_SIZE_CHANGE
  *
 *          Copyright (c) 2015-2016 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <signal.h>
#include <sys/ioctl.h>
#include <ncurses/ncurses.h>
#include <strings.h>

#include <g_ev_console.h>
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
PRIVATE void my_endwin(void);
PRIVATE void catch_signals(void);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/
PRIVATE WINDOW *wn = NULL;      // ncurses handler
PRIVATE bool __colors_ready__ = false;

PRIVATE struct { // Code repeated
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
SDATA (DTP_INTEGER,     "timeout",              0,  "500", "Timeout, to detect size change in stdscr"),
SDATA (DTP_INTEGER,     "cx",                   0,  0, "cx window size"),
SDATA (DTP_INTEGER,     "cy",                   0,  0, "cy window size"),

SDATA (DTP_POINTER,     "user_data",            0,  0, "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,  0, "more user data"),
SDATA (DTP_POINTER,     "subscriber",           0,  0, "subscriber of output-events. If it's null then subscriber is the parent."),
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
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout,      gobj_read_integer_attr)

    /*
     *  Start up ncurses
     */

    catch_signals();
    atexit(my_endwin);

    /*
     *  stdscr timer to detect window size change
     */
    priv->timer = gobj_create_pure_child("", C_TIMER, 0, gobj);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(timeout,         gobj_read_integer_attr)
        if(gobj_is_running(gobj)) {
            set_timeout_periodic(priv->timer, priv->timeout);
        }
    ELIF_EQ_SET_PRIV(cx,            gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(cy,            gobj_read_integer_attr)
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


    wn = initscr();                 /* Start curses mode            */
    cbreak();                       /* Line buffering disabled      */
    noecho();                       /* Don't echo() while we do getch */
    keypad(wn, TRUE);               /* We get F1, F2 etc..          */
    halfdelay(1);
    //wtimeout(wn, 10);             /* input non-blocking, wait 1 msec */
    if(has_colors()) {
        start_color();
        /* Allow -1 (default fg/bg) to pass through */
        assume_default_colors(-1, -1);
        __colors_ready__ = true;
    } else {
        __colors_ready__ = false;
    }

    int cx, cy;
    get_stdscr_size(&cx, &cy);
    gobj_write_integer_attr(gobj, "cx", cx);
    gobj_write_integer_attr(gobj, "cy", cy);

    //log_debug_printf(0, "initial size cx %d cy %d %s\n", cx, cy, gobj_name(gobj));

    json_t *jn_kw = json_object();
    json_object_set_new(jn_kw, "cx", json_integer(cx));
    json_object_set_new(jn_kw, "cy", json_integer(cy));
    gobj_send_event_to_children(gobj, EV_SIZE, jn_kw, gobj);

    gobj_start_children(gobj);
    wrefresh(wn);

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
PRIVATE void my_endwin(void)
{
    if(wn) {
        endwin();
        wn = NULL;
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int get_color_id(const char *color)
{
    if(!color || !*color) {
        return -1; /* default */
    }
    if(!strcasecmp(color, "default") || !strcasecmp(color, "def")) {
        return -1;
    }
    for(size_t i = 0; i < ARRAY_SIZE(table_id); i++) {
        if(strcasecmp(table_id[i].name, color) == 0) {
            return table_id[i].id;
        }
    }
    return -1; /* fall back to default, not white */
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int get_color_pair(int fg, int bg)
{
    static int next_pair = 1;
    static int cp[16][16]; /* allow up to 16 basic colors; still O(1) */
    int maxc = (COLORS > 16) ? 16 : COLORS;

    if(!__colors_ready__) {
        return 0; /* no color attribute */
    }

    /* Map -1 to default colors only if use_default_colors() succeeded */
    if(fg < -1 || bg < -1) {
        return 0;
    }

    /* Validate against actual COLORS; clamp to supported range */
    if((fg >= maxc && fg != -1) || (bg >= maxc && bg != -1)) {
        return 0;
    }

    int ifg = (fg < 0) ? 0 : fg;
    int ibg = (bg < 0) ? 0 : bg;

    if(cp[ifg][ibg]) {
        return COLOR_PAIR(cp[ifg][ibg]);
    }

    if(next_pair >= COLOR_PAIRS) {
        return 0; /* out of pairs */
    }

    /* init_pair() accepts -1 when use_default_colors() is active */
    if(init_pair((short)next_pair, (short)fg, (short)bg) != OK) {
        return 0;
    }

    cp[ifg][ibg] = next_pair++;
    return COLOR_PAIR(cp[ifg][ibg]);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int get_paint_color(const char *fg_color, const char *bg_color)
{
    int fg = get_color_id(fg_color);
    int bg = get_color_id(bg_color);
    return get_color_pair(fg, bg);
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
        gobj_write_integer_attr(gobj, "cx", cx);
        gobj_write_integer_attr(gobj, "cy", cy);

        json_t *jn_kw = json_object();
        json_object_set_new(jn_kw, "cx", json_integer(cx));
        json_object_set_new(jn_kw, "cy", json_integer(cy));

        json_incref(jn_kw);
        gobj_send_event_to_children(gobj, EV_SIZE, jn_kw, gobj);

        gobj_publish_event(gobj, EV_SCREEN_SIZE_CHANGE, jn_kw);
    }
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
    .mt_writing     = mt_writing
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_WN_STDSCR);

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
        {EV_TIMEOUT_PERIODIC,       ac_timeout,      0},
        {EV_STOPPED,                0,               0},
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,                  st_idle},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_SCREEN_SIZE_CHANGE,     EVF_OUTPUT_EVENT},
        {EV_TIMEOUT_PERIODIC,       0},
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
        0,  // Command table
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
PUBLIC int register_c_wn_stdscr(void)
{
    return create_gclass(C_WN_STDSCR);
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int get_paint_color(const char *fg_color, const char *bg_color) // Code repeated
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
            gobj_send_event(__gobj_with_focus__, EV_KILLFOCUS, 0, 0);
            if(gobj_trace_level(gobj) & (TRACE_MACHINE|TRACE_EV_KW)) {
                gobj_trace_msg(gobj, "👁 👁 ⏪ %s", gobj_short_name(__gobj_with_focus__));
            }
        }
    }

    __gobj_with_focus__ = gobj;
    if(gobj_send_event(__gobj_with_focus__, EV_SETFOCUS, 0, 0)<0) {
        // Si el nuevo focus no acepta pon el default focus
        __gobj_with_focus__ = __gobj_default_focus__;
        gobj_send_event(__gobj_default_focus__, EV_SETFOCUS, 0, 0);
        if(gobj_trace_level(gobj) & (TRACE_MACHINE|TRACE_EV_KW)) {
            gobj_trace_msg(gobj, "👁 👁 ⏩ %s", gobj_short_name(__gobj_with_focus__));
        }
    } else {
        if(gobj_trace_level(gobj) & (TRACE_MACHINE|TRACE_EV_KW)) {
            gobj_trace_msg(gobj, "👁 👁 ⏩ %s", gobj_short_name(__gobj_with_focus__));
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
PUBLIC int DrawText(hgobj gobj, int x, int y, const char *s) // Shortcut for EV_SETTEXT
{
    json_t *kw = json_pack("{s:s, s:i, s:i}",
        "text", s,
        "x", x,
        "y", y
    );
    gobj_send_event(gobj, EV_SETTEXT, kw, gobj);
    // if(__gobj_with_focus__) {
    //     gobj_send_event(__gobj_with_focus__, EV_SETFOCUS, 0, 0);
    // }
    return 0;
}
