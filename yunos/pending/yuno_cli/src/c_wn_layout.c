/***********************************************************************
 *          C_WN_LAYOUT.C
 *          Wn_layout GClass.
 *
 *          UI Layout
 *
 *          Copyright (c) 2016 Niyamaka.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include <ncurses/ncurses.h>
#include <ncurses/panel.h>
#include "c_wn_layout.h"

/***********************************************************************
 *  layout_type: 'vertical' -> the elements are blocks
 *               'horizontal' -> the elements are inline-blocks
 *
 *  vertical layout:
 *   ┌────────────┐⇑
 *   │            │║
 *   ├────────────┤║    overflow-x: hidden
 *   │            │║    overflow-y: auto
 *   ├────────────┤║
 *   │            │║
 *   └────────────┘⇩
 *
 *  horizontal layout:
 *  ┌───────╥──────╥───────┐
 *  │       ║      ║       │    overflow-x: auto
 *  │       ║      ║       │    overflow-y: hidden
 *  └───────╨──────╨───────┘
 *  ⇦══════════════════════⇨
 *
 *
 *  vertical layout:
 *      - witdh:    100%
 *      - height:   fixed pixels
 *
 *  horizontal layout:
 *      - witdh:    fixed pixels
 *      - height:   100%
 *
 *
 *  GLayout se caracteriza por admitir el evento de entrada EV_SIZE.
 *  GBox se caracteriza por admitir el evento de entrada EV_SIZE.
 *
 *  La diferencia entre ambos es que GLayout adopta el nuevo size que recibe
 *  por el evento EV_SIZE, recalcula los sizes que deben tener
 *  sus hijos, y luego reenvia el evento EV_SIZE a todos sus hijos,
 *  con el nuevo size que debe tener cada uno.
 *
 *  GBox simplemente recibe el evento EV_SIZE y adopta el nuevo size.
 *  También hace un publish_event del evento EV_SIZE por si algún hijo es
 *  otro GLayout.
 *
 *  Los GLayout tendrán que reajustar los sizes de sus hijos a partir de cada
 *  nuevo hijo.
 *
 *  Los hijos de un GLayout deben ser GBox u otro GLayout.
 *  Los GLayout tienen la propiedad de estar escuchando el evento resize,
 *  para reajustar a todos sus hijos en cada redimensión.
 *  Los GBox no tienen porqué trabajar todos con dimensiones en %,
 *  es decir, que el redraw no será un trabajo hecho automáticamente por el browser.
 *  Los GBox pueden trabajar con espacios flexibles además de los fijos,
 *  por lo que el espacio flexible tiene que ser reajustado en los hijos
 *  en cada redimensión del padre.
 *  Los GBox hijos, se adaptan al tipo de layout del padre (vertical u horizontal),
 *  por lo que habrá que llevar cuidado en la configuración de cada tipo de GBox:
 *
 *   - en los layout verticales, la intiable es height, mientras que width es
 *     fijo siempre al 100%.
 *   - en los layout horizontales la intiable es width, mientras que height es
 *     fijo siempre al 100%.
 *
 *  El layout padre, forzará en los hijos, la dimensión flexible en la intiable
 *  que corresponde a cada tipo de layout (vertical u horizontal).
 ***************************************************************************/


/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int fix_child_sizes(hgobj gobj);


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
SDATA (DTP_STRING,      "layout_type",          0,  "vertical", "Layout 'vertical' or 'horizontal"),
SDATA (DTP_INTEGER,     "x",                    0,  0, "x window coord"),
SDATA (DTP_INTEGER,     "y",                    0,  0, "y window coord"),
SDATA (DTP_INTEGER,     "cx",                   0,  80, "physical witdh window size"),
SDATA (DTP_INTEGER,     "cy",                   0,  1, "physical height window size"),
SDATA (DTP_STRING,      "bg_color",             0,  "cyan", "Background color"),
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
    const char *layout_type;
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
    SET_PRIV(layout_type,               gobj_read_str_attr)
    SET_PRIV(fg_color,                  gobj_read_str_attr)
    SET_PRIV(bg_color,                  gobj_read_str_attr)

    int x = gobj_read_integer_attr(gobj, "x");
    int y = gobj_read_integer_attr(gobj, "y");
    int cx = gobj_read_integer_attr(gobj, "cx");
    int cy = gobj_read_integer_attr(gobj, "cy");

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
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(bg_color,                gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(fg_color,              gobj_read_str_attr)
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

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_child_added(hgobj gobj, hgobj child)
{
    fix_child_sizes(gobj);
    gobj_send_event(gobj, "EV_PAINT", 0, gobj); // repaint myself: in resize of childs the remain zones are not refreshed
    return 0;
}

/***************************************************************************
 *      Framework Method
 ***************************************************************************/
PRIVATE int mt_child_removed(hgobj gobj, hgobj child)
{
    fix_child_sizes(gobj);
    if(!gobj_is_destroying(gobj) && gobj_is_running(gobj)) {
        gobj_send_event(gobj, "EV_PAINT", 0, gobj); // repaint myself: in resize of childs the remain zones are not refreshed
    }
    return 0;
}




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  Calculate and fix the position and size of childs
 *
 *  Sum up all fixed child sizes.
 *  The total size minus the above value,
 *  it's the amount to deal among all flexible childs.
 ***************************************************************************/
PRIVATE int fix_child_sizes(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    int parent_width = gobj_read_integer_attr(gobj, "cx");
    int parent_height = gobj_read_integer_attr(gobj, "cy");
    int abs_x = gobj_read_integer_attr(gobj, "x");
    int abs_y = gobj_read_integer_attr(gobj, "y");

    int ln = gobj_child_size(gobj);
    if(!ln) {
        return 0;
    }
    int *width_flexs = gbmem_malloc(sizeof(int) * ln);
    int *height_flexs = gbmem_malloc(sizeof(int) * ln);
    int total_fixed_height = 0;
    int total_fixed_width = 0;
    int total_flexs_height = 0;
    int total_flexs_width = 0;

    for(int i=0; i<ln; i++) {
        hgobj child;
        gobj_child_by_index(gobj, i+1, &child);
        if(gobj_is_destroying(child)) {
            continue;
        }
        int child_width = gobj_read_integer_attr(child, "w");
        int child_height = gobj_read_integer_attr(child, "h");

        /*
         *  height
         */
        if(child_height < 0) {
            height_flexs[i] = -child_height;
            total_flexs_height += -child_height;
        } else {
            height_flexs[i] = 0;
            total_fixed_height += child_height;
        }

        /*
         *  width
         */
        if(child_width < 0) {
            width_flexs[i] = -child_width;
            total_flexs_width += -child_width;
        } else {
            width_flexs[i] = 0;
            total_fixed_width += child_width;
        }
    }

    if(strcmp(priv->layout_type, "vertical")==0) {
        /*
         *  Vertical layout
         */
        int free_height = parent_height - total_fixed_height;
        int arepartir = 0;
        if(free_height > 0 && total_flexs_height > 0) {
            arepartir = free_height/total_flexs_height;
        }
        int partial_height = 0;
        for(int i=0; i<ln; i++) {
            hgobj child;
            gobj_child_by_index(gobj, i+1, &child);
            if(gobj_is_destroying(child)) {
                continue;
            }

            int new_height, new_width;
            if (height_flexs[i]) {
                new_height = arepartir * height_flexs[i];
            } else {
                new_height = gobj_read_integer_attr(child, "h");
            }
            new_width = parent_width;

            json_t *kw_move = json_pack("{s:i, s:i}",
                "x", abs_x,
                "y", partial_height
            );
            gobj_send_event(child, "EV_MOVE", kw_move, gobj);

            json_t *kw_size = json_pack("{s:i, s:i}",
                "cx", new_width,
                "cy", new_height
            );
            gobj_send_event(child, "EV_SIZE", kw_size, gobj);

            partial_height += new_height;
        }
    } else {
        /*
         *  Horizontal layout
         */
        int free_width = parent_width - total_fixed_width;
        int arepartir = 0;
        if(free_width > 0 && total_flexs_width > 0) {
            arepartir = free_width/total_flexs_width;
        }
        int partial_width = 0;
        for(int i=0; i<ln; i++) {
            hgobj child;
            gobj_child_by_index(gobj, i+1, &child);
            if(gobj_is_destroying(child)) {
                continue;
            }

            int new_height, new_width;
            if (width_flexs[i]) {
                new_width = arepartir * width_flexs[i];
            } else {
                new_width = gobj_read_integer_attr(child, "w");
            }
            new_height = parent_height;

            json_t *kw_move = json_pack("{s:i, s:i}",
                "x", partial_width,
                "y", abs_y
            );
            gobj_send_event(child, "EV_MOVE", kw_move, gobj);

            json_t *kw_size = json_pack("{s:i, s:i}",
                "cx", new_width,
                "cy", new_height
            );
            gobj_send_event(child, "EV_SIZE", kw_size, gobj);

            partial_width += new_width;
        }
    }

    gbmem_free(width_flexs);
    gbmem_free(height_flexs);
    return 0;
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

    // TEST
    if(0) {
        wmove(priv->wn, 0, 0);
        const char *s = gobj_full_name(gobj);
        waddnstr(priv->wn, s, strlen(s));
    }

    if(has_colors()) {
        if(!empty_string(priv->fg_color) && !empty_string(priv->bg_color)) {
            wbkgd(
                priv->wn,
                _get_curses_color(priv->fg_color, priv->bg_color)
            );
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
PRIVATE int ac_move(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    int x = kw_get_int(gobj, kw, "x", 0, KW_REQUIRED);
    int y = kw_get_int(gobj, kw, "y", 0, KW_REQUIRED);
    gobj_write_integer_attr(gobj, "x", x);
    gobj_write_integer_attr(gobj, "y", y);

    if(priv->panel) {
        // log_debug_printf(0, "====> move panel x %d y %d %s", x, y, gobj_name(gobj));
        move_panel(priv->panel, y, x);
        update_panels();
        doupdate();
    } else if(priv->wn) {
        // log_debug_printf(0, "====> move window x %d y %d %s", x, y, gobj_name(gobj));
        mvwin(priv->wn, y, x);
        wrefresh(priv->wn);
    }

    json_t *kw_move = json_pack("{s:i, s:i}",
        "x", x,
        "y", y
    );
    gobj_send_event_to_childs(gobj, "EV_MOVE", kw_move, gobj);

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
        // log_debug_printf(0, "====> size panel cx %d cy %d %s", cx, cy, gobj_name(gobj));
        wresize(priv->wn, cy, cx);
        update_panels();
        doupdate();
    } else if(priv->wn) {
        // log_debug_printf(0, "====> size window cx %d cy %d %s", cx, cy, gobj_name(gobj));
        wresize(priv->wn, cy, cx);
        wrefresh(priv->wn);
    }
    gobj_send_event(gobj, "EV_PAINT", 0, gobj);  // repaint, ncurses doesn't do it

    fix_child_sizes(gobj);

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
    .mt_child_removed   = mt_child_removed
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_WN_LAYOUT);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_PAINT);
GOBJ_DEFINE_EVENT(EV_MOVE);
GOBJ_DEFINE_EVENT(EV_SIZE);

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
        {EV_MOVE,      ac_move,    0},
        {EV_SIZE,      ac_size,    0},
        {EV_PAINT,     ac_paint,   0},
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

    event_type_t event_types[] = {
        {EV_PAINT,      0},
        {EV_MOVE,       0},
        {EV_SIZE,       0},
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
PUBLIC int register_c_wn_layout(void)
{
    return create_gclass(C_WN_LAYOUT);
}
