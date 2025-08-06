/***********************************************************************
 *          C_GSS_UDP_S.C
 *          GssUdpS GClass.
 *
 *          Gossamer UDP Server
 *
    TODO review, dl_list is not a good choice for performance

            Api Gossamer
            ------------

            Input Events:
            - EV_SEND_MESSAGE

            Output Events:
            - EV_ON_OPEN
            - EV_ON_CLOSE
            - EV_ON_MESSAGE

 *
 *          Copyright (c) 2014 Niyamaka.
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>

#include <gobj.h>
#include <g_events.h>
#include <g_states.h>
#include <helpers.h>
#include "c_timer.h"
#include "c_udp_s.h"
#include "c_gss_udp_s.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct _UDP_CHANNEL {
    DL_ITEM_FIELDS

    const char *name;
    time_t t_inactivity;
    gbuffer_t *gbuf;
} UDP_CHANNEL;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE UDP_CHANNEL *find_udp_channel(hgobj gobj, const char *name);
PRIVATE UDP_CHANNEL *new_udp_channel(hgobj gobj, const char *name);
PRIVATE void del_udp_channel(hgobj gobj, UDP_CHANNEL *ch);
PRIVATE void free_channels(hgobj gobj);


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes - order affect to oid's
 *---------------------------------------------*/
PRIVATE sdata_desc_t tattr_desc[] = {
SDATA (DTP_STRING,      "url",                  SDF_RD, 0, "url of udp server"),
SDATA (DTP_INTEGER,     "timeout_base",         SDF_RD,  "5000", "timeout base"),
SDATA (DTP_INTEGER,     "seconds_inactivity",   SDF_RD,  "300", "Seconds to consider a gossamer close"),
SDATA (DTP_BOOLEAN,     "disable_end_of_frame", SDF_RD|SDF_STATS, 0, "Disable null as end of frame"),
SDATA (DTP_STRING,      "__username__",         SDF_RD,  "",             "Username"),
SDATA (DTP_POINTER,     "user_data",            0,  0, "user data"),
SDATA (DTP_POINTER,     "user_data2",           0,  0, "more user data"),
SDATA (DTP_POINTER,     "subscriber",           0,  0, "subscriber of output-events. Default if null is parent."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_DEBUG = 0x0001,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"debug",        "Trace to debug"},
{0, 0},
};

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    // Conf
    int32_t timeout_base;
    int32_t seconds_inactivity;

    // Data oid
    BOOL disable_end_of_frame;

    hgobj gobj_udp_s;
    hgobj timer;
    dl_list_t dl_channel;
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

    priv->timer = gobj_create_pure_child(gobj_name(gobj), C_TIMER, 0, gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout_base,          gobj_read_integer_attr)
    SET_PRIV(seconds_inactivity,    gobj_read_integer_attr)
    SET_PRIV(disable_end_of_frame,  gobj_read_bool_attr)

    json_t *kw_udps = json_pack("{s:s}",
        "url", gobj_read_str_attr(gobj, "url")
    );
    priv->gobj_udp_s = gobj_create("", C_UDP_S, kw_udps, gobj);

    dl_init(&priv->dl_channel, gobj);

    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber)
        subscriber = gobj_parent(gobj);
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_start(priv->timer);
    set_timeout_periodic(priv->timer, priv->timeout_base);
    gobj_start(priv->gobj_udp_s);
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);
    gobj_stop(priv->gobj_udp_s);
    free_channels(gobj);
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
PRIVATE UDP_CHANNEL *new_udp_channel(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    UDP_CHANNEL *ch;

    ch = gbmem_malloc(sizeof(UDP_CHANNEL));
    if(!ch) {
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_MEMORY_ERROR,
            "msg",                  "%s", "no memory",
            "sizeof(UDP_CHANNEL)",  "%d", sizeof(UDP_CHANNEL),
            NULL
        );
        return 0;
    }
    GBMEM_STRDUP(ch->name, name);
    dl_add(&priv->dl_channel, ch);

    return ch;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void free_channels(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    UDP_CHANNEL *ch; ;
    while((ch=dl_first(&priv->dl_channel))) {
        del_udp_channel(gobj, ch);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void del_udp_channel(hgobj gobj, UDP_CHANNEL *ch)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    dl_delete(&priv->dl_channel, ch, 0);
    GBUFFER_DECREF(ch->gbuf);
    GBMEM_FREE(ch->name);
    GBMEM_FREE(ch);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE UDP_CHANNEL *find_udp_channel(hgobj gobj, const char *name)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    UDP_CHANNEL *ch;

    if(!name) {
        return 0;
    }
    ch = dl_first(&priv->dl_channel);
    while(ch) {
        if(strcmp(ch->name, name)==0) {
            return ch;
        }
        ch = dl_next(ch);
    }
    return 0;
}




            /***************************
             *      Actions
             ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_rx_data(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);
    const char *udp_channel = gbuffer_getlabel(gbuf);

    UDP_CHANNEL *ch = find_udp_channel(gobj, udp_channel);
    if(!ch) {
        ch = new_udp_channel(gobj, udp_channel);

        gobj_publish_event(gobj, EV_ON_OPEN, 0);
    }
    ch->t_inactivity = start_sectimer(priv->seconds_inactivity);

    if(priv->disable_end_of_frame) {
        gobj_publish_event(gobj, EV_ON_MESSAGE, kw);
        return 0;
    }

    char *p;
    while((p=gbuffer_get(gbuf, 1))) {
        if(!p) {
            break; // no more data in gbuf
        }
        /*
         *  Use a temporal gbuf, to save data until arrive of null.
         */
        if(!ch->gbuf) {
            size_t size = MIN(1*1024L*1024L, gbmem_get_maximum_block());
            ch->gbuf = gbuffer_create(size, size);
            if(!ch->gbuf) {
                gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_MEMORY_ERROR,
                    "msg",              "%s", "no memory",
                    "size",             "%d", size,
                    NULL
                );
                break;
            }
        }
        if(*p==0) {
            /*
             *  End of frame
             */
            json_t *kw_ev = json_pack("{s:I}",
                "gbuffer", (json_int_t)(size_t)ch->gbuf
            );
            ch->gbuf = 0;
            gobj_publish_event(gobj, EV_ON_MESSAGE, kw_ev);
        } else {
            if(gbuffer_append(ch->gbuf, p, 1)!=1) {
                gobj_log_error(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_MEMORY_ERROR,
                    "msg",              "%s", "gbuffer_append() FAILED",
                    NULL
                );
                /*
                 *  No space, process whatever received
                 */
                json_t *kw_ev = json_pack("{s:I}",
                    "gbuffer", (json_int_t)(size_t)ch->gbuf
                );
                ch->gbuf = 0;
                gobj_publish_event(gobj, EV_ON_MESSAGE, kw_ev);
                //break;
            }
        }
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);
    const char *udp_channel = gbuffer_getlabel(gbuf);

    UDP_CHANNEL *ch = find_udp_channel(gobj, udp_channel);
    if(ch) {
    } else {
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_PARAMETER_ERROR,
            "msg",                  "%s", "UDP channel NOT FOUND",
            "udp_channel",          "%s", udp_channel?udp_channel:"",
            NULL
        );
    }
    return gobj_send_event(priv->gobj_udp_s, EV_TX_DATA, kw, gobj);
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_transmit_ready(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    UDP_CHANNEL *ch, *nx;

    ch = dl_first(&priv->dl_channel);
    while(ch) {
        nx = dl_next(ch);
        if(test_sectimer(ch->t_inactivity)) {
            gobj_publish_event(gobj, EV_ON_CLOSE, 0);
            del_udp_channel(gobj, ch);
        }
        ch = nx;
    }

    KW_DECREF(kw);
    return 0;
}

/***********************************************************************
 *          FSM
 ***********************************************************************/
/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
PRIVATE const GMETHODS gmt = {
    .mt_create = mt_create,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_writing = mt_writing,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_GSS_UDP_S);

/*------------------------*
 *      States
 *------------------------*/

/*------------------------*
 *      Events
 *------------------------*/

/***************************************************************************
 *
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

    /*----------------------------------------*
     *          Define States
     *----------------------------------------*/
    ev_action_t st_idle[] = {
        {EV_RX_DATA,            ac_rx_data,         0},
        {EV_SEND_MESSAGE,       ac_send_message,    0},
        {EV_TX_READY,           ac_transmit_ready,  0},
        {EV_TIMEOUT_PERIODIC,   ac_timeout,         0},
        {EV_STOPPED,            0,                  0},
        {0, 0, 0}
    };

    states_t states[] = {
        {ST_IDLE,               st_idle},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_MESSAGE,         EVF_OUTPUT_EVENT},
        {EV_RX_DATA,            0},
        {EV_ON_OPEN,            EVF_OUTPUT_EVENT},
        {EV_ON_CLOSE,           EVF_OUTPUT_EVENT},
        {EV_SEND_MESSAGE,       0},
        {EV_TX_READY,           0},
        {EV_TIMEOUT_PERIODIC,   0},
        {EV_STOPPED,            0},
        {0, 0}
    };

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        &gmt,
        0,  // lmt
        tattr_desc,
        sizeof(PRIVATE_DATA),
        0,  // authz_table
        0,  // command_table
        s_user_trace_level,
        0   // gcflag
    );
    if(!__gclass__) {
        // Error already logged
        return -1;
    }

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PUBLIC int register_c_gss_udp_s(void)
{
    return create_gclass(C_GSS_UDP_S);
}
