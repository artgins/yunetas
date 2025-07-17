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
#include "c_wn_list.h"

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
SDATA (ASN_OCTET_STR,   "layout_type",          0,  0, "Layout inherit from parent"),
SDATA (ASN_INTEGER,     "w",                    0,  0, "logical witdh window size"),
SDATA (ASN_INTEGER,     "h",                    0,  0, "logical height window size"),
SDATA (ASN_INTEGER,     "x",                    0,  0, "x window coord"),
SDATA (ASN_INTEGER,     "y",                    0,  0, "y window coord"),
SDATA (ASN_INTEGER,     "cx",                   0,  80, "physical witdh window size"),
SDATA (ASN_INTEGER,     "cy",                   0,  1, "physical height window size"),
SDATA (ASN_INTEGER,     "scroll_size",          0,  1000000, "scroll size. 0 is unlimited (until out of memory)"),
SDATA (ASN_OCTET_STR,   "bg_color",             0,  "blue", "Background color"),
SDATA (ASN_OCTET_STR,   "fg_color",             0,  "white", "Foreground color"),
SDATA (ASN_POINTER,     "user_data",            0,  0, "user data"),
SDATA (ASN_POINTER,     "user_data2",           0,  0, "more user data"),
SDATA (ASN_POINTER,     "subscriber",           0,  0, "subscriber of output-events. If it's null then subscriber is the parent."),
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
    SET_PRIV(cx,                        gobj_read_int32_attr)
    SET_PRIV(cy,                        gobj_read_int32_attr)
    SET_PRIV(scroll_size,               gobj_read_int32_attr)

    int x = gobj_read_int32_attr(gobj, "x");
    int y = gobj_read_int32_attr(gobj, "y");
    int cx = gobj_read_int32_attr(gobj, "cx");
    int cy = gobj_read_int32_attr(gobj, "cy");

    priv->wn = newwin(cy, cx, y, x);
    if(!priv->wn) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "newwin() FAILED",
            NULL
        );
    }
    priv->panel = new_panel(priv->wn);
    if(!priv->panel) {
        log_error(0,
            "gobj",         "%s", gobj_full_name(gobj),
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_SYSTEM_ERROR,
            "msg",          "%s", "new_panel() FAILED",
            NULL
        );
    }
    //scrollok(priv->wn, true);

    dl_init(&priv->dl_lines);
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
    ELIF_EQ_SET_PRIV(cx,                    gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(cy,                    gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(scroll_size,           gobj_read_int32_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    gobj_send_event(gobj, "EV_PAINT", 0, gobj);
    gobj_start_childs(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    gobj_stop_childs(gobj);
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
    gobj_change_state(gobj, "ST_IDLE");
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
                _get_curses_color(priv->fg_color, line->bg_color)
            );
        } else
        if(!empty_string(priv->fg_color) && !empty_string(priv->bg_color)) {
            wbkgd(
                priv->wn,
                _get_curses_color(priv->fg_color, priv->bg_color)
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
PRIVATE int ac_paint(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->wn) {
        // Debugging in kdevelop or batch mode has no wn
        KW_DECREF(kw);
        return 0;
    }
    wclear(priv->wn);

    int n_lines = dl_size(&priv->dl_lines);
    int n_win = priv->cy;
    if(n_lines <= n_win) {
        int y = n_win - n_lines;
        line_t *line = dl_first(&priv->dl_lines);
        for(int i=0; i < n_lines; i++) {
            int ll = strlen(line->text);
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
            log_error(0,
                "gobj",         "%s", gobj_full_name(gobj),
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL_ERROR,
                "msg",          "%s", "no line",
                "line",         "%d", b,
                NULL
            );
        } else {
            for(int i=0; i < n_win && line; i++) {
                int ll = strlen(line->text);
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
PRIVATE int ac_settext(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    const char *text = kw_get_str(kw, "text", "", KW_REQUIRED);
    const char *bg_color = kw_get_str(kw, "bg_color", 0, 0);
    const char *fg_color = kw_get_str(kw, "fg_color", 0, 0);

    int n_win = priv->cy;

    if(strchr(text, '\n')) {
        int len = strlen(text);
        GBUFFER *gbuf = gbuf_create(len, len, 0, 0);
        gbuf_append(gbuf, (void *)text, len);
        if(gbuf) {
            char *s;
            while((s=gbuf_getline(gbuf, '\n'))) {
                add_line(gobj, s, bg_color, fg_color);

                int n_lines = dl_size(&priv->dl_lines);
                if(n_lines > n_win && priv->base > n_win) {
                    priv->base++;
                }
            }
            gbuf_decref(gbuf);
        }
    } else {
        add_line(gobj, text, bg_color, fg_color);

        int n_lines = dl_size(&priv->dl_lines);
        if(n_lines > n_win && priv->base > n_win) {
            priv->base++;
        }
    }

    while(priv->scroll_size > 0 && dl_size(&priv->dl_lines) > priv->scroll_size) {
        delete_line(gobj, 1);
    }
    gobj_send_event(gobj, "EV_PAINT", 0, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    GBUFFER *gbuf = (GBUFFER *)(size_t)kw_get_int(kw, "gbuffer", 0, 0);
    const char *bg_color = kw_get_str(kw, "bg_color", 0, 0);
    const char *fg_color = kw_get_str(kw, "fg_color", 0, 0);

    int n_win = priv->cy;

    char *s;
    while((s=gbuf_getline(gbuf, '\n'))) {
        add_line(gobj, s, bg_color, fg_color);

        int n_lines = dl_size(&priv->dl_lines);
        if(n_lines > n_win && priv->base > n_win) {
            priv->base++;
        }
    }

    while(priv->scroll_size > 0 && dl_size(&priv->dl_lines) > priv->scroll_size) {
        delete_line(gobj, 1);
    }
    gobj_send_event(gobj, "EV_PAINT", 0, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_scroll_line_up(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int n_lines = dl_size(&priv->dl_lines);
    int n_win = priv->cy;

    if(n_lines > n_win) {
        if(priv->base < n_lines - n_win) {
            priv->base++;
            gobj_send_event(gobj, "EV_PAINT", 0, gobj);
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
        gobj_send_event(gobj, "EV_PAINT", 0, gobj);
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

    int n_lines = dl_size(&priv->dl_lines);
    int n_win = priv->cy;

    if(n_lines > n_win) {
        if(priv->base < n_lines - n_win) {
            priv->base += n_win;
            if(priv->base >= n_lines - n_win) {
                priv->base = n_lines - n_win;
            }
            gobj_send_event(gobj, "EV_PAINT", 0, gobj);
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
        gobj_send_event(gobj, "EV_PAINT", 0, gobj);
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

    int n_lines = dl_size(&priv->dl_lines);
    int n_win = priv->cy;

    priv->base = n_lines - n_win;

    gobj_send_event(gobj, "EV_PAINT", 0, gobj);

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

    gobj_send_event(gobj, "EV_PAINT", 0, gobj);

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
    gobj_send_event(gobj, "EV_PAINT", 0, gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_setfocus(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    KW_DECREF(kw);
    return -1;  // Don't want focus
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_move(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int x = kw_get_int(kw, "x", 0, KW_REQUIRED);
    int y = kw_get_int(kw, "y", 0, KW_REQUIRED);
    gobj_write_int32_attr(gobj, "x", x);
    gobj_write_int32_attr(gobj, "y", y);

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

    int cx = kw_get_int(kw, "cx", 0, KW_REQUIRED);
    int cy = kw_get_int(kw, "cy", 0, KW_REQUIRED);
    gobj_write_int32_attr(gobj, "cx", cx);
    gobj_write_int32_attr(gobj, "cy", cy);

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
    gobj_send_event(gobj, "EV_PAINT", 0, gobj);  // repaint, ncurses doesn't do it

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_top(hgobj gobj, const char *event, json_t *kw, hgobj src)
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
PRIVATE const EVENT input_events[] = {
    {"EV_ON_MESSAGE",       0,  0,  0},
    {"EV_SETTEXT",          0,  0,  0},
    {"EV_KILLFOCUS",        0,  0,  0},
    {"EV_SETFOCUS",         0,  0,  0},
    {"EV_PAINT",            0,  0,  0},
    {"EV_MOVE",             0,  0,  0},
    {"EV_SIZE",             0,  0,  0},
    {"EV_SET_TOP_WINDOW",   0,  0,  0},
    {"EV_SCROLL_LINE_UP",   0,  0,  0},
    {"EV_SCROLL_LINE_DOWN", 0,  0,  0},
    {"EV_SCROLL_PAGE_UP",   0,  0,  0},
    {"EV_SCROLL_PAGE_DOWN", 0,  0,  0},
    {"EV_SCROLL_TOP",       0,  0,  0},
    {"EV_SCROLL_BOTTOM",    0,  0,  0},
    {"EV_CLRSCR",           0,  0,  0},
    {"EV_ON_OPEN",          0,  0,  0},
    {"EV_ON_CLOSE",         0,  0,  0},
    {NULL, 0, 0, 0}
};
PRIVATE const EVENT output_events[] = {
    {NULL, 0, 0, 0}
};
PRIVATE const char *state_names[] = {
    "ST_IDLE",
    "ST_DISABLED",
    NULL
};

PRIVATE EV_ACTION ST_IDLE[] = {
    {"EV_ON_MESSAGE",       ac_on_message,          0},
    {"EV_SETTEXT",          ac_settext,             0},
    {"EV_SETFOCUS",         ac_setfocus,            0},
    {"EV_KILLFOCUS",        0,                      0},
    {"EV_MOVE",             ac_move,                0},
    {"EV_SIZE",             ac_size,                0},
    {"EV_PAINT",            ac_paint,               0},
    {"EV_SET_TOP_WINDOW",   ac_top,                 0},
    {"EV_SCROLL_LINE_UP",   ac_scroll_line_up,      0},
    {"EV_SCROLL_LINE_DOWN", ac_scroll_line_down,    0},
    {"EV_SCROLL_PAGE_UP",   ac_scroll_page_up,      0},
    {"EV_SCROLL_PAGE_DOWN", ac_scroll_page_down,    0},
    {"EV_SCROLL_TOP",       ac_scroll_top,          0},
    {"EV_SCROLL_BOTTOM",    ac_scroll_bottom,       0},
    {"EV_CLRSCR",           ac_clrscr,              0},
    {"EV_ON_OPEN",          0,                      0},
    {"EV_ON_CLOSE",         0,                      0},
    {0,0,0}
};

PRIVATE EV_ACTION ST_DISABLED[] = {
    {0,0,0}
};

PRIVATE EV_ACTION *states[] = {
    ST_IDLE,
    ST_DISABLED,
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
    GCLASS_WN_LIST_NAME,
    &fsm,
    {
        mt_create,
        0, //mt_create2,
        mt_destroy,
        mt_start,
        mt_stop,
        mt_play,
        mt_pause,
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
PUBLIC GCLASS *gclass_wn_list(void)
{
    return &_gclass;
}
