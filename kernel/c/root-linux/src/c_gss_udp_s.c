/***********************************************************************
 *          C_GSS_UDP_S.C
 *          GssUdpS GClass.
 *
 *          Gossamer UDP Server
 *
 *          A frame ends with a NUL byte, and may come in several datagrams:
 *          they are put together per peer (`ip:port`, the label C_UDP_S
 *          gives each datagram), in a buffer of its own. What a peer can
 *          hold is capped, because it costs this yuno memory before it is a
 *          frame: `max_channels` peers at once (each forgotten after
 *          `seconds_inactivity`), `max_frame_size` for one frame, and
 *          `max_pending_bytes` for all the unfinished frames together.
 *          Beyond a cap the datagram is dropped: logged on the transition
 *          (peers), or once per PEER_LOG_INTERVAL_MS with the count (pending
 *          bytes, frames cut), since a sender can fill and drain it at will.
 *          Up to 7.25.4 every peer shared one channel; the per-peer label of
 *          7.25.5 reserved 1 MB on the first byte of each source port, so a
 *          sender of one-byte datagrams from many ports took the yuno to its
 *          memory ceiling.
 *
    TODO review, dl_list is not a good choice for performance (bounded by
    max_channels)

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
 *          Copyright (c) 2025-2026, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>
#include "c_timer.h"
#include "c_udp_s.h"
#include "c_gss_udp_s.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define FRAME_INITIAL_SIZE  (4*1024)    // a frame buffer starts here and doubles up to max_frame_size
#define PEER_LOG_INTERVAL_MS (10*1000)  // one log of a frame cut, or of a drop of pending bytes, per interval

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
PRIVATE void publish_frame(hgobj gobj, UDP_CHANNEL *ch);
PRIVATE void free_channels(hgobj gobj);


/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
SDATA (DTP_STRING,      "url",                  SDF_RD, 0, "url of udp server"),
SDATA (DTP_INTEGER,     "timeout_base",         SDF_RD,  "5000", "timeout base"),
SDATA (DTP_INTEGER,     "seconds_inactivity",   SDF_RD,  "300", "Seconds to consider a gossamer close"),
SDATA (DTP_BOOLEAN,     "disable_end_of_frame", SDF_RD|SDF_STATS, 0, "Disable null as end of frame"),
SDATA (DTP_INTEGER,     "max_channels",         SDF_RD,  "1024", "Maximum peers (source ip:port) held at once, 0 no limit. A datagram of a new peer beyond it is dropped"),
SDATA (DTP_INTEGER,     "max_frame_size",       SDF_RD,  "1048576", "Maximum size of a frame; a bigger one is delivered cut at this size"),
SDATA (DTP_INTEGER,     "max_pending_bytes",    SDF_RD,  "8388608", "Maximum bytes of all the unfinished frames together, 0 no limit. Beyond it the datagram is dropped, with the unfinished frame of its peer"),
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
    json_int_t max_channels;
    size_t max_frame_size;
    json_int_t max_pending_bytes;

    hgobj gobj_udp_s;
    hgobj timer;
    dl_list_t dl_channel;
    json_int_t n_channels;
    json_int_t pending_bytes;       // bytes of the unfinished frames, all peers
    BOOL channels_capped;           // logged, until a peer goes
    uint64_t t_pending_log;         // msectimer: no log of a pending drop before it
    json_int_t pending_drops;       // not logged since the last log
    uint64_t t_frame_cut_log;       // msectimer: no log of a frame cut before it
    json_int_t frames_cut;          // not logged since the last log
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
    SET_PRIV(max_channels,          gobj_read_integer_attr)
    SET_PRIV(max_pending_bytes,     gobj_read_integer_attr)

    json_int_t max_frame_size = gobj_read_integer_attr(gobj, "max_frame_size");
    if(max_frame_size <= 0) {
        max_frame_size = 1*1024L*1024L;
    }
    priv->max_frame_size = MIN((size_t)max_frame_size, gbmem_get_maximum_block()/2);

    json_t *kw_udps = json_pack("{s:s}",
        "url", gobj_read_str_attr(gobj, "url")
    );
    priv->gobj_udp_s = gobj_create("", C_UDP_S, kw_udps, gobj);

    dl_init(&priv->dl_channel, gobj);

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
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
            "msgset",               "%s", MSGSET_MEMORY,
            "msg",                  "%s", "no memory",
            "sizeof(UDP_CHANNEL)",  "%d", sizeof(UDP_CHANNEL),
            NULL
        );
        return 0;
    }
    GBMEM_STRDUP(ch->name, name);
    dl_add(&priv->dl_channel, ch);
    priv->n_channels++;

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
    if(ch->gbuf) {
        priv->pending_bytes -= (json_int_t)gbuffer_totalbytes(ch->gbuf);
        GBUFFER_DECREF(ch->gbuf);
    }
    GBMEM_FREE(ch->name);
    GBMEM_FREE(ch);
    priv->n_channels--;
    priv->channels_capped = FALSE;
}

/***************************************************************************
 *  The frame of this peer, finished or cut, goes to the subscriber
 ***************************************************************************/
PRIVATE void publish_frame(hgobj gobj, UDP_CHANNEL *ch)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->pending_bytes -= (json_int_t)gbuffer_totalbytes(ch->gbuf);
    json_t *kw_ev = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)ch->gbuf
    );
    ch->gbuf = 0;
    gobj_publish_event(gobj, EV_ON_MESSAGE, kw_ev);
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
PRIVATE int ac_rx_data(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);
    const char *udp_channel = gbuffer_getlabel(gbuf);

    UDP_CHANNEL *ch = find_udp_channel(gobj, udp_channel);
    if(!ch) {
        if(priv->max_channels > 0 && priv->n_channels >= priv->max_channels) {
            if(!priv->channels_capped) {
                priv->channels_capped = TRUE;
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PROTOCOL,
                    "msg",          "%s", "Too many peers, datagrams of new peers dropped",
                    "max_channels", "%ld", (long)priv->max_channels,
                    "peername",     "%s", udp_channel?udp_channel:"",
                    NULL
                );
            }
            KW_DECREF(kw);
            return 0;
        }
        if(gobj_trace_level(gobj) & TRACE_DEBUG) {
            gobj_log_debug(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INFO,
                "msg",          "%s", "new channel",
                "name",         "%s", udp_channel,
                NULL
            );
        }
        ch = new_udp_channel(gobj, udp_channel);
        if(!ch) {
            // Error already logged
            KW_DECREF(kw);
            return 0;
        }

        gobj_publish_event(gobj, EV_ON_OPEN, 0);
    }
    ch->t_inactivity = start_sectimer(priv->seconds_inactivity);

    if(gobj_trace_level(gobj) & TRACE_DEBUG) {
        gobj_log_debug(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INFO,
            "msg",          "%s", "rx data",
            "channel",      "%d", (int)ch->__id__,
            "name",         "%s", ch->name,
            NULL
        );
        gobj_trace_dump_gbuf(gobj, gbuf, "rx data");
    }

    if(priv->disable_end_of_frame) {
        gobj_publish_event(gobj, EV_ON_MESSAGE, kw);
        return 0;
    }

    char *p;
    while((p=gbuffer_get(gbuf, 1))) {
        if(*p==0) {
            /*
             *  End of frame
             */
            if(!ch->gbuf) {
                ch->gbuf = gbuffer_create(0, 1);
                if(!ch->gbuf) {
                    // Error already logged
                    break;
                }
            }
            publish_frame(gobj, ch);
            continue;
        }

        /*
         *  Room for one byte more, of all the peers together
         */
        if(priv->max_pending_bytes > 0 && priv->pending_bytes >= priv->max_pending_bytes) {
            priv->pending_drops++;
            if(!priv->t_pending_log || test_msectimer(priv->t_pending_log)) {
                priv->t_pending_log = start_msectimer(PEER_LOG_INTERVAL_MS);
                gobj_log_warning(gobj, 0,
                    "function",             "%s", __FUNCTION__,
                    "msgset",               "%s", MSGSET_PROTOCOL,
                    "msg",                  "%s", "Too many bytes in unfinished frames, datagram dropped with the unfinished frame of its peer",
                    "max_pending_bytes",    "%ld", (long)priv->max_pending_bytes,
                    "peername",             "%s", ch->name,
                    "drops",                "%ld", (long)priv->pending_drops,
                    NULL
                );
                priv->pending_drops = 0;
            }
            if(ch->gbuf) {
                priv->pending_bytes -= (json_int_t)gbuffer_totalbytes(ch->gbuf);
                GBUFFER_DECREF(ch->gbuf);
            }
            break;
        }

        /*
         *  A frame buffer starts small and grows (doubling) up to
         *  max_frame_size. Up to 7.25.4 it was 1 MB from the first byte.
         */
        if(!ch->gbuf) {
            size_t size = MIN((size_t)FRAME_INITIAL_SIZE, priv->max_frame_size);
            ch->gbuf = gbuffer_create(size, 2*priv->max_frame_size + 1);
            if(!ch->gbuf) {
                // Error already logged
                break;
            }
        }
        if(gbuffer_totalbytes(ch->gbuf) >= priv->max_frame_size) {
            /*
             *  No end of frame within max_frame_size: deliver what came,
             *  cut, as always, and go on with a new frame.
             */
            priv->frames_cut++;
            if(!priv->t_frame_cut_log || test_msectimer(priv->t_frame_cut_log)) {
                priv->t_frame_cut_log = start_msectimer(PEER_LOG_INTERVAL_MS);
                gobj_log_warning(gobj, 0,
                    "function",         "%s", __FUNCTION__,
                    "msgset",           "%s", MSGSET_PROTOCOL,
                    "msg",              "%s", "Frame without end within max_frame_size, delivered cut",
                    "max_frame_size",   "%lu", (unsigned long)priv->max_frame_size,
                    "peername",         "%s", ch->name,
                    "frames_cut",       "%ld", (long)priv->frames_cut,
                    NULL
                );
                priv->frames_cut = 0;
            }
            publish_frame(gobj, ch);
            ch->gbuf = gbuffer_create(
                MIN((size_t)FRAME_INITIAL_SIZE, priv->max_frame_size),
                2*priv->max_frame_size + 1
            );
            if(!ch->gbuf) {
                // Error already logged
                break;
            }
        }
        if(gbuffer_append(ch->gbuf, p, 1)!=1) {
            // Error already logged
            break;
        }
        priv->pending_bytes++;
    }

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_send_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);
    const char *udp_channel = gbuffer_getlabel(gbuf);

    UDP_CHANNEL *ch = find_udp_channel(gobj, udp_channel);
    if(ch) {
    } else {
        gobj_log_error(gobj, 0,
            "function",             "%s", __FUNCTION__,
            "msgset",               "%s", MSGSET_PARAMETER,
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
PRIVATE int ac_transmit_ready(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
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

/***************************************************************************
 *                          FSM
 ***************************************************************************/
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
            "msgset",       "%s", MSGSET_INTERNAL,
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
        attrs_table,
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
