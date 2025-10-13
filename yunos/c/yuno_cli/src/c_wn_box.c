/***********************************************************************
 *          C_WN_BOX.C
 *          Wn_box GClass.
 *
 *          UI Box. Manage position, size and colors, it seems a virtual window.
 *          Well, it manages a WINDOW and PANEL ncurses objects.
 *
 *          It has the mt_play/mt_pause,  to disable or not the box,
 *              - changing the state ST_DISABLED,ST_IDLE
 *
 *          It has the mt_child_added, mt_child_removed:
 *              - mt_child_added: when a child is added it sends:
 *                  - a EV_MOVE message with the position (x,y) of the box.
 *                  - a EV_SIZE message with the size (cx, cy) of the box.
 *              - mt_child_removed: nothing
 *
 *          It supports the events:
 *              EV_PAINT
 *                  - clear the WINDOW
 *                  - it it has color set the color in WINDOW (back/fore)???
 *                  - update PANEL or WINDOW
 *              EV_MOVE
 *                  - update PANEL or WINDOW
                    - send EV_MOVE to children
 *              EV_SIZE
 *                  - update PANEL or WINDOW
 *                  - auto-send EV_PAINT (ncurses not does refresh)
 *                  - send EV_SIZE to children.
 *
 *          HACK: the PANEL exists in Toolbar and Stsline, in WorkArea and Editline not.
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
#include <string.h>

#include <help_ncurses.h>
#include <g_ev_console.h>
#include "c_wn_box.h"
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

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
SDATA (DTP_INTEGER,     "w",                    0,  0, "logical width window size"),
SDATA (DTP_INTEGER,     "h",                    0,  0, "logical height window size"),
SDATA (DTP_INTEGER,     "x",                    0,  0, "x window coord"),
SDATA (DTP_INTEGER,     "y",                    0,  0, "y window coord"),
SDATA (DTP_INTEGER,     "cx",                   0,  "80", "physical width window size"),
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
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(fg_color,                  gobj_read_str_attr)
    SET_PRIV(bg_color,                  gobj_read_str_attr)
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
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

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
        return -1;
    }

    /* After newwin() in mt_start(), set default bkg once */
    int def_attr = get_paint_color(
        gobj_read_str_attr(gobj, "fg_color"),
        gobj_read_str_attr(gobj, "bg_color")
    );
    if(def_attr) {
        wbkgdset(priv->wn, ' ' | def_attr);
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
    gobj_send_event(gobj, EV_PAINT, 0, gobj);
    gobj_start_children(gobj);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop_children(gobj);

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

    return 0;
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
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
    gobj_change_state(gobj, ST_DISABLED);
    // TODO re PAINT in disabled colors
    return 0;
}

/***************************************************************************
 *      Framework Method child_added
 ***************************************************************************/
PRIVATE int mt_child_added(hgobj gobj, hgobj child)
{
    int x = (int)gobj_read_integer_attr(gobj, "x");
    int y = (int)gobj_read_integer_attr(gobj, "y");

    json_t *kw_move  = json_pack("{s:i, s:i}",
        "x", x,
        "y", y
    );
    gobj_send_event(child, EV_MOVE, kw_move, gobj);

    int cx = (int)gobj_read_integer_attr(gobj, "cx");
    int cy = (int)gobj_read_integer_attr(gobj, "cy");

    json_t *kw_size  = json_pack("{s:i, s:i}",
        "cx", cx,
        "cy", cy
    );
    gobj_send_event(child, EV_SIZE, kw_size, gobj);

    return 0;
}

/***************************************************************************
 *      Framework Method child_removed
 ***************************************************************************/
PRIVATE int mt_child_removed(hgobj gobj, hgobj child)
{

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

    // TEST
    if(0) {
        wmove(priv->wn, 0, 0);
        const char *s = gobj_full_name(gobj);
        waddnstr(priv->wn, s, strlen(s));
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
PRIVATE int ac_move(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int x = (int)kw_get_int(gobj, kw, "x", 0, KW_REQUIRED);
    int y = (int)kw_get_int(gobj, kw, "y", 0, KW_REQUIRED);
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

    json_t *kw_move = json_pack("{s:i, s:i}",
        "x", x,
        "y", y
    );
    gobj_send_event_to_children(gobj, EV_MOVE, kw_move, gobj);

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

    json_t *kw_size  = json_pack("{s:i, s:i}",
        "cx", cx,
        "cy", cy
    );
    gobj_send_event_to_children(gobj, EV_SIZE, kw_size, gobj);

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
    .mt_play            = mt_play,
    .mt_pause           = mt_pause,
    .mt_writing         = mt_writing,
    .mt_child_added     = mt_child_added,
    .mt_child_removed   = mt_child_removed,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_WN_BOX);

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
        {EV_MOVE,       ac_move,    0},
        {EV_SIZE,       ac_size,    0},
        {EV_PAINT,      ac_paint,   0},
        {0,0,0}
    };

    ev_action_t st_disabled[] = {
        {0,0,0}
    };

    states_t states[] = {
        {ST_IDLE,       st_idle},
        {ST_DISABLED,   st_disabled},
        {0, 0}
    };

    /*------------------------*
     *      Events
     *------------------------*/
    event_type_t event_types[] = {
        {EV_PAINT,      0},
        {EV_MOVE,       0},
        {EV_SIZE,       0},
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
        0,  // LMETHOD
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // ACL
        0,  // Commands
        s_user_trace_level,
        0   // gcflag
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************************
 *          Public access
 ***************************************************************************/
PUBLIC int register_c_wn_box(void)
{
    return create_gclass(C_WN_BOX);
}
