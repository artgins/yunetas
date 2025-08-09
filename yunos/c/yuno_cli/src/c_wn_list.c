/***********************************************************************
 *          C_WN_LIST.C
 *          Wn_list GClass.
 *
 *          UI List
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include <ncurses/ncurses.h>
#include <ncurses/panel.h>

#include <g_ev_console.h>
#include "c_wn_list.h"
#include "c_wn_stdscr.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct line_s {
    DL_ITEM_FIELDS

    char *text;
    char *bg_color;
    char *fg_color;
} line_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int clrscr(hgobj gobj);


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
SDATA (DTP_STRING,      "layout_type",          0,  0, "Layout inherit from parent"),
SDATA (DTP_INTEGER,     "scroll_size",          0,  "1000000", "scroll size. 0 is unlimited (until out of memory)"),
SDATA (DTP_INTEGER,     "w",                    0,  0, "logical witdh window size"),
SDATA (DTP_INTEGER,     "h",                    0,  0, "logical height window size"),
SDATA (DTP_INTEGER,     "x",                    0,  0, "x window coord"),
SDATA (DTP_INTEGER,     "y",                    0,  0, "y window coord"),
SDATA (DTP_INTEGER,     "cx",                   0,  "80", "physical witdh window size"),
SDATA (DTP_INTEGER,     "cy",                   0,  "1", "physical height window size"),
SDATA (DTP_STRING,      "bg_color",             0,  "blue", "Background color"),
SDATA (DTP_STRING,      "fg_color",             0,  "white", "Foreground color"),
SDATA (DTP_POINTER,     "user_data",            0,  0, "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,  0, "more user data"),
SDATA (DTP_POINTER,     "subscriber",           0,  0, "subscriber of output-events. If it's null then subscriber is the parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_USER = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"trace_user",        "Trace user description"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    const char *layout_type;        // inherit from parent glayout, don't touch.
    const char *fg_color;
    const char *bg_color;
    int32_t scroll_size;
    WINDOW *wn;     // ncurses window handler
    PANEL *panel;   // panel handler

    dl_list_t dl_lines;
    int32_t cx;
    int32_t cy;
    int32_t base;
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
    SET_PRIV(layout_type,               gobj_read_str_attr)
    SET_PRIV(fg_color,                  gobj_read_str_attr)
    SET_PRIV(bg_color,                  gobj_read_str_attr)
    SET_PRIV(cx,                        gobj_read_integer_attr)
    SET_PRIV(cy,                        gobj_read_integer_attr)
    SET_PRIV(scroll_size,               gobj_read_integer_attr)

    int x = (int)gobj_read_integer_attr(gobj, "x");
    int y = (int)gobj_read_integer_attr(gobj, "y");
    int cx = (int)gobj_read_integer_attr(gobj, "cx");
    int cy = (int)gobj_read_integer_attr(gobj, "cy");

    priv->wn = newwin(cy, cx, y, x);
    if(!priv->wn) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "newwin() FAILED",
            NULL
        );
    }
    priv->panel = new_panel(priv->wn);
    if(!priv->panel) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "new_panel() FAILED",
            NULL
        );
    }
    //scrollok(priv->wn, true);

    dl_init(&priv->dl_lines, gobj);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *
     */
    IF_EQ_SET_PRIV(bg_color,                gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(fg_color,              gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(cx,                    gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(cy,                    gobj_read_integer_attr)
    ELIF_EQ_SET_PRIV(scroll_size,           gobj_read_integer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    gobj_send_event(gobj, EV_PAINT, 0, gobj);
    gobj_start_children(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    gobj_stop_children(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(priv->panel) {
        del_panel(priv->panel);
        priv->panel = 0;
        update_panels();
        doupdate();
    }
    if(priv->wn) {
        delwin(priv->wn);
        priv->wn = 0;
    }

    clrscr(gobj);
}

/***************************************************************************
 *      Framework Method play
 ***************************************************************************/
PRIVATE int mt_play(hgobj gobj)
{
    gobj_change_state(gobj, ST_IDLE);
    // TODO re PAINT in enabled colors
    return 0;
}

/***************************************************************************
 *      Framework Method pause
 ***************************************************************************/
PRIVATE int mt_pause(hgobj gobj)
{
    gobj_change_state(gobj, "ST_DISABLED");
    // TODO re PAINT in disabled colors
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int clrscr(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    line_t *line;

    while((line=dl_first(&priv->dl_lines))) {
        dl_delete_item(line, 0);
        GBMEM_FREE(line->bg_color);
        GBMEM_FREE(line->fg_color);
        GBMEM_FREE(line->text);
        GBMEM_FREE(line);
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int delete_line(hgobj gobj, int n)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    line_t *line = dl_nfind(&priv->dl_lines,  n);
    if(line) {
        dl_delete_item(line, 0);
        GBMEM_FREE(line->bg_color);
        GBMEM_FREE(line->fg_color);
        GBMEM_FREE(line->text);
        GBMEM_FREE(line);
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int add_line(hgobj gobj, const char *s, const char *bg_color, const char *fg_color)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    line_t *line = gbmem_malloc(sizeof(line_t));
    line->text = gbmem_strdup(s);

    line->bg_color = !empty_string(bg_color)?gbmem_strdup(bg_color):0;
    line->fg_color = !empty_string(fg_color)?gbmem_strdup(fg_color):0;
    return dl_add(&priv->dl_lines, line);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void setcolor(hgobj gobj, line_t *line)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(has_colors()) {
        if(!empty_string(line->fg_color)) {
            wbkgd(
                priv->wn,
                get_paint_color(priv->fg_color, line->bg_color)
            );
        } else
        if(!empty_string(priv->fg_color) && !empty_string(priv->bg_color)) {
            wbkgd(
                priv->wn,
                get_paint_color(priv->fg_color, priv->bg_color)
            );
        }
    }
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_settext(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *text = kw_get_str(gobj, kw, "text", "", KW_REQUIRED);
    const char *bg_color = kw_get_str(gobj, kw, "bg_color", 0, 0);
    const char *fg_color = kw_get_str(gobj, kw, "fg_color", 0, 0);

    int n_win = priv->cy;

    if(strchr(text, '\n')) {
        int len = (int)strlen(text);
        gbuffer_t *gbuf = gbuffer_create(len, len);
        gbuffer_append(gbuf, (void *)text, len);
        if(gbuf) {
            char *s;
            while((s=gbuffer_getline(gbuf, '\n'))) {
                add_line(gobj, s, bg_color, fg_color);

                int n_lines = (int)dl_size(&priv->dl_lines);
                if(n_lines > n_win && priv->base > n_win) {
                    priv->base++;
                }
            }
            gbuffer_decref(gbuf);
        }
    } else {
        add_line(gobj, text, bg_color, fg_color);

        int n_lines = (int)dl_size(&priv->dl_lines);
        if(n_lines > n_win && priv->base > n_win) {
            priv->base++;
        }
    }

    while(priv->scroll_size > 0 && dl_size(&priv->dl_lines) > priv->scroll_size) {
        delete_line(gobj, 1);
    }
    gobj_send_event(gobj, EV_PAINT, 0, gobj);

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
    const char *bg_color = kw_get_str(gobj, kw, "bg_color", 0, 0);
    const char *fg_color = kw_get_str(gobj, kw, "fg_color", 0, 0);

    int n_win = priv->cy;

    char *s;
    while((s=gbuffer_getline(gbuf, '\n'))) {
        add_line(gobj, s, bg_color, fg_color);

        int n_lines = (int)dl_size(&priv->dl_lines);
        if(n_lines > n_win && priv->base > n_win) {
            priv->base++;
        }
    }

    while(priv->scroll_size > 0 && dl_size(&priv->dl_lines) > priv->scroll_size) {
        delete_line(gobj, 1);
    }
    gobj_send_event(gobj, EV_PAINT, 0, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_paint(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->wn) {
        // Debugging in kdevelop or batch mode has no wn
        KW_DECREF(kw);
        return 0;
    }
    wclear(priv->wn);

    int n_lines = (int)dl_size(&priv->dl_lines);
    int n_win = priv->cy;
    if(n_lines <= n_win) {
        int y = n_win - n_lines;
        line_t *line = dl_first(&priv->dl_lines);
        for(int i=0; i < n_lines; i++) {
            int ll = (int)strlen(line->text);
            if(ll > priv->cx) {
                ll = priv->cx;
            }
            wmove(priv->wn, y+i, 0);
            setcolor(gobj, line);
            waddnstr(priv->wn, line->text, ll);
            line = dl_next(line);
        }
    } else {
        int y = 0;
        int b = n_lines - priv->base - n_win + 1;
        if(b < 1) {
            b = 1;
        }
        //log_debug_printf(0, "n_lines %d, n_win %d, b %d, base %d", n_lines, n_win, b, priv->base);
        line_t *line = dl_nfind(&priv->dl_lines, b);
        if(!line) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "no line",
                "line",         "%d", b,
                NULL
            );
        } else {
            for(int i=0; i < n_win && line; i++) {
                int ll = (int)strlen(line->text);
                if(ll > priv->cx) {
                    ll = priv->cx;
                }
                wmove(priv->wn, y+i, 0);
                setcolor(gobj, line);
                waddnstr(priv->wn, line->text, ll);
                line = dl_next(line);
            }
        }
    }

    if(priv->panel) {
        update_panels();
        doupdate();
    } else if(priv->wn) {
        wrefresh(priv->wn);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_scroll_line_up(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int n_lines = (int)dl_size(&priv->dl_lines);
    int n_win = priv->cy;

    if(n_lines > n_win) {
        if(priv->base < n_lines - n_win) {
            priv->base++;
            gobj_send_event(gobj, EV_PAINT, 0, gobj);
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_scroll_line_down(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->base > 0) {
        priv->base--;
        gobj_send_event(gobj, EV_PAINT, 0, gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_scroll_page_up(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int n_lines = (int)dl_size(&priv->dl_lines);
    int n_win = priv->cy;

    if(n_lines > n_win) {
        if(priv->base < n_lines - n_win) {
            priv->base += n_win;
            if(priv->base >= n_lines - n_win) {
                priv->base = n_lines - n_win;
            }
            gobj_send_event(gobj, EV_PAINT, 0, gobj);
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_scroll_page_down(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int n_win = priv->cy;

    if(priv->base > 0) {
        priv->base -= n_win;
        if(priv->base < 0) {
            priv->base = 0;
        }
        gobj_send_event(gobj, EV_PAINT, 0, gobj);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_scroll_top(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int n_lines = (int)dl_size(&priv->dl_lines);
    int n_win = priv->cy;

    priv->base = n_lines - n_win;

    gobj_send_event(gobj, EV_PAINT, 0, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_scroll_bottom(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->base = 0;

    gobj_send_event(gobj, EV_PAINT, 0, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_clrscr(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clrscr(gobj);
    priv->base = 0;
    gobj_send_event(gobj, EV_PAINT, 0, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_move(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int x = kw_get_int(gobj, kw, "x", 0, KW_REQUIRED);
    int y = kw_get_int(gobj, kw, "y", 0, KW_REQUIRED);
    gobj_write_integer_attr(gobj, "x", x);
    gobj_write_integer_attr(gobj, "y", y);

    if(priv->panel) {
        //log_debug_printf(0, "move panel x %d y %d %s", x, y, gobj_name(gobj));
        move_panel(priv->panel, y, x);
        update_panels();
        doupdate();
    } else if(priv->wn) {
        //log_debug_printf(0, "move window x %d y %d %s", x, y, gobj_name(gobj));
        mvwin(priv->wn, y, x);
        wrefresh(priv->wn);
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_size(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int cx = kw_get_int(gobj, kw, "cx", 0, KW_REQUIRED);
    int cy = kw_get_int(gobj, kw, "cy", 0, KW_REQUIRED);
    gobj_write_integer_attr(gobj, "cx", cx);
    gobj_write_integer_attr(gobj, "cy", cy);

    if(priv->panel) {
        //log_debug_printf(0, "size panel cx %d cy %d %s", cx, cy, gobj_name(gobj));
        wresize(priv->wn, cy, cx);
        update_panels();
        doupdate();
    } else if(priv->wn) {
        //log_debug_printf(0, "size window cx %d cy %d %s", cx, cy, gobj_name(gobj));
        wresize(priv->wn, cy, cx);
        wrefresh(priv->wn);
    }
    gobj_send_event(gobj, EV_PAINT, 0, gobj);  // repaint, ncurses doesn't do it

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_set_top_window(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->panel) {
        top_panel(priv->panel);
        update_panels(); // TODO is necessary this?
        doupdate();
    } else if(priv->wn) {
        wrefresh(priv->wn);
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
    .mt_play        = mt_play,
    .mt_pause       = mt_pause,
    .mt_writing     = mt_writing
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_WN_LIST);

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

    ev_action_t st_idle[] = {
        {EV_ON_MESSAGE,         ac_on_message,          0},
        {EV_SETTEXT,            ac_settext,             0},
        {EV_PAINT,              ac_paint,               0},
        {EV_MOVE,               ac_move,                0},
        {EV_SIZE,               ac_size,                0},
        {EV_SET_TOP_WINDOW,     ac_set_top_window,      0},
        {EV_SCROLL_LINE_UP,     ac_scroll_line_up,      0},
        {EV_SCROLL_LINE_DOWN,   ac_scroll_line_down,    0},
        {EV_SCROLL_PAGE_UP,     ac_scroll_page_up,      0},
        {EV_SCROLL_PAGE_DOWN,   ac_scroll_page_down,    0},
        {EV_SCROLL_TOP,         ac_scroll_top,          0},
        {EV_SCROLL_BOTTOM,      ac_scroll_bottom,       0},
        {EV_CLRSCR,             ac_clrscr,              0},
        {EV_ON_OPEN,            0,                      0},
        {EV_ON_CLOSE,           0,                      0},
        {0,0,0}
    };

    ev_action_t st_disabled[] = {
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,           st_idle},
        {ST_DISABLED,       st_disabled},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_MESSAGE,         0},
        {EV_SETTEXT,            0},
        {EV_PAINT,              0},
        {EV_MOVE,               0},
        {EV_SIZE,               0},
        {EV_SET_TOP_WINDOW,     0},
        {EV_SCROLL_LINE_UP,     0},
        {EV_SCROLL_LINE_DOWN,   0},
        {EV_SCROLL_PAGE_UP,     0},
        {EV_SCROLL_PAGE_DOWN,   0},
        {EV_SCROLL_TOP,         0},
        {EV_SCROLL_BOTTOM,      0},
        {EV_CLRSCR,             0},
        {EV_ON_OPEN,            0},
        {EV_ON_CLOSE,           0},
        {NULL, 0}
    };

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // local methods
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // acl
        0,  // cmds
        s_user_trace_level,
        0   // gcflags
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *              Public access
 ***************************************************************************/
PUBLIC int register_c_wn_list(void)
{
    return create_gclass(C_WN_LIST);
}
