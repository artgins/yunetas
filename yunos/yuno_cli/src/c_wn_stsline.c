/***********************************************************************
 *          C_WN_STSLINE.C
 *          Wn_stsline GClass.
 *
 *          Status Line
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include <ncurses/ncurses.h>
#include <ncurses/panel.h>
#include "c_wn_stsline.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
SDATA (ASN_OCTET_STR,   "text",                 0,  0, "Text of status line"),
SDATA (ASN_INTEGER,     "x",                    0,  0, "x window coord"),
SDATA (ASN_INTEGER,     "y",                    0,  0, "y window coord"),
SDATA (ASN_INTEGER,     "cx",                   0,  80, "physical witdh window size"),
SDATA (ASN_INTEGER,     "cy",                   0,  1, "physical height window size"),
SDATA (ASN_OCTET_STR,   "bg_color",             0,  "cyan", "Background color"),
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
    int cx;
    int cy;
    const char *text;
    const char *fg_color;
    const char *bg_color;
    WINDOW *wn;     // ncurses window handler
    PANEL *panel;   // panel handler
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
    SET_PRIV(fg_color,                  gobj_read_str_attr)
    SET_PRIV(bg_color,                  gobj_read_str_attr)
    SET_PRIV(text,                      gobj_read_str_attr)
    SET_PRIV(cx,                        gobj_read_int32_attr)
    SET_PRIV(cy,                        gobj_read_int32_attr)

    int x = gobj_read_int32_attr(gobj, "x");
    int y = gobj_read_int32_attr(gobj, "y");

    priv->wn = newwin(priv->cy, priv->cx, y, x);
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
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(bg_color,                gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(fg_color,              gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(text,                  gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(cx,                    gobj_read_int32_attr)
    ELIF_EQ_SET_PRIV(cy,                    gobj_read_int32_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    gobj_send_event(gobj, "EV_PAINT", 0, gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
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

    if(has_colors()) {
        if(!empty_string(priv->fg_color) && !empty_string(priv->bg_color)) {
            wbkgd(
                priv->wn,
                _get_curses_color(priv->fg_color, priv->bg_color)
            );
        }
    }
    if(!empty_string(priv->text)) {
        wmove(priv->wn, 0, 0);
        waddnstr(priv->wn, priv->text, strlen(priv->text));
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
    const char *text = kw_get_str(kw, "text", "", KW_REQUIRED);
    gobj_write_str_attr(gobj, "text", text);
    gobj_send_event(gobj, "EV_PAINT", 0, gobj);

    KW_DECREF(kw);
    return 0;
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
 *                          FSM
 ***************************************************************************/
PRIVATE const EVENT input_events[] = {
    {"EV_SETTEXT",      0,  0,  0},
    {"EV_PAINT",        0,  0,  0},
    {"EV_MOVE",         0,  0,  0},
    {"EV_SIZE",         0,  0,  0},
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
    {"EV_SETTEXT",          ac_settext,         0},
    {"EV_MOVE",             ac_move,            0},
    {"EV_SIZE",             ac_size,            0},
    {"EV_PAINT",            ac_paint,           0},
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
    GCLASS_WN_STSLINE_NAME,
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
PUBLIC GCLASS *gclass_wn_stsline(void)
{
    return &_gclass;
}
