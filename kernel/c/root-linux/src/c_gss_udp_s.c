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
 *          Up to 7.25.4 every peer shared one channel. A frame buffer starts
 *          at 4 KB and grows up to max_frame_size, so a sender of one-byte
 *          datagrams from many ports cannot take the yuno to its memory
 *          ceiling.
 *
 *          When its C_UDP_S stops by itself (its read failed, or could not
 *          start again: EV_STOPPED while this gobj AND the C_UDP_S run), the
 *          service would be deaf, and every send would reach a stopped
 *          C_UDP_S ("Event NOT DEFINED in state"). So it is said once, and
 *          the C_UDP_S is started again after a backoff: timeout_base
 *          first, doubled (up to RESTART_BACKOFF_MAX_MS) each time it stops
 *          again, or cannot start, within RESTART_HOLD_MS of the last try.
 *          Up to 7.25.20 the EV_STOPPED was taken with no action.
 *
 *          A send is refused (one warning a stop, the count when it starts
 *          again or when this gobj stops) whenever the C_UDP_S is not in
 *          ST_IDLE: stopped, or still stopping (ST_WAIT_STOPPED, a send in
 *          flight), the window before its EV_STOPPED.
 *
 *          A C_UDP_S stopped from outside (gobj_stop() of it alone: it no
 *          longer runs when its EV_STOPPED comes) is not started again: that
 *          stop was somebody's decision. It is said once, INFO.
 *
 *          timeout_base <= 0 would leave the periodic timer unarmed: no peer
 *          ever forgotten, and no base for the backoff. It is refused with a
 *          warning, and the default (5000) used.
 *
    TODO review, dl_list is not a good choice for performance (bounded by
    max_channels)

            Api Gossamer
            ------------

            Input Events:
            - EV_SEND_MESSAGE
                A gbuffer to a peer: to its ADDRESS (gbuffer_setaddr()), or,
                with no address, to the known peer its LABEL names ("ip:port",
                the label of the frames of that peer). A published frame
                carries both, so a host answers a peer by sending with the
                label (or the address) of its frame. Neither: refused.

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
#define TIMEOUT_BASE_DEFAULT 5000       // the default of timeout_base, used when it is <= 0
#define RESTART_HOLD_MS     (60*1000)   // a C_UDP_S that runs this long since its restart resets the backoff
#define RESTART_BACKOFF_MAX_MS (5*60*1000) // the backoff of a restart doubles up to this

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct _UDP_CHANNEL {
    DL_ITEM_FIELDS

    const char *name;
    time_t t_inactivity;
    gbuffer_t *gbuf;
    struct sockaddr_storage addr;   // the peer, as its datagrams came
    socklen_t addrlen;
} UDP_CHANNEL;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE UDP_CHANNEL *find_udp_channel(hgobj gobj, const char *name);
PRIVATE UDP_CHANNEL *new_udp_channel(hgobj gobj, const char *name);
PRIVATE void del_udp_channel(hgobj gobj, UDP_CHANNEL *ch);
PRIVATE void publish_frame(hgobj gobj, UDP_CHANNEL *ch);
PRIVATE void free_channels(hgobj gobj);
PRIVATE void restart_udp_server(hgobj gobj);
PRIVATE json_int_t schedule_restart(hgobj gobj);
GOBJ_DECLARE_EVENT(EV_START_UDP_SERVER);


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
    hgobj timer_restart;            // the backoff of a restart of the C_UDP_S
    BOOL udp_stopped;               // the C_UDP_S stopped by itself, started again by timer_restart
    json_int_t tx_dropped;          // sends refused while it is not in ST_IDLE
    json_int_t restart_backoff_ms;  // delay of the last restart scheduled, 0 none yet
    uint64_t t_restart_holds;       // msectimer: a stop before it doubles the backoff
    BOOL start_when_stopped;        // started while its C_UDP_S still stopped: started at its EV_STOPPED
    BOOL start_posted;              // EV_START_UDP_SERVER posted and still wanted
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
    priv->timer_restart = gobj_create_pure_child("restart", C_TIMER, 0, gobj);

    /*
     *  Do copy of heavy used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(timeout_base,          gobj_read_integer_attr)
    if(priv->timeout_base <= 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "timeout_base <= 0 would arm no timer (no peer forgotten, no restart of the UDP server): the default is used",
            "timeout_base", "%d", (int)priv->timeout_base,
            "default",      "%d", TIMEOUT_BASE_DEFAULT,
            NULL
        );
        priv->timeout_base = TIMEOUT_BASE_DEFAULT;
        gobj_write_integer_attr(gobj, "timeout_base", priv->timeout_base);
    }
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

    priv->restart_backoff_ms = 0;
    priv->t_restart_holds = 0;

    /*
     *  Its C_UDP_S still stopping (its read being canceled: a stop and a
     *  start of this gobj in the same turn, as a pause and a play of its
     *  host can be): it cannot start yet ("yev_server_udp ALREADY
     *  exists"). It is started when its stop ends (EV_STOPPED).
     */
    if(gobj_in_this_state(priv->gobj_udp_s, ST_WAIT_STOPPED)) {
        priv->start_when_stopped = TRUE;
        return 0;
    }

    if(gobj_start(priv->gobj_udp_s) < 0) {
        // Error already logged
        json_int_t backoff_ms = schedule_restart(gobj);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "UDP server cannot start, it is tried again after a backoff",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            "backoff_ms",   "%ld", (long)backoff_ms,
            NULL
        );
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);         // a C_TIMER runs while it has a timeout: this stops it
    clear_timeout(priv->timer_restart);
    if(gobj_is_running(priv->gobj_udp_s)) {
        gobj_stop(priv->gobj_udp_s);    // not running: stopped from outside
    }
    priv->start_when_stopped = FALSE;
    priv->start_posted = FALSE;
    free_channels(gobj);

    if(priv->tx_dropped > 0) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "UDP server stops with sends refused while it was stopped",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            "tx_dropped",   "%ld", (long)priv->tx_dropped,
            NULL
        );
    }
    priv->udp_stopped = FALSE;
    priv->tx_dropped = 0;
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

    /*
     *  The peer of the frame, as C_UDP_S gives it with each datagram: what
     *  a host needs to answer it (EV_SEND_MESSAGE). Up to 7.25.4 a frame
     *  carried neither.
     */
    gbuffer_setlabel(ch->gbuf, ch->name);
    if(ch->addrlen > 0) {
        gbuffer_setaddr(ch->gbuf, (struct sockaddr *)&ch->addr, ch->addrlen);
    }

    json_t *kw_ev = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)ch->gbuf
    );
    ch->gbuf = 0;
    gobj_publish_event(gobj, EV_ON_MESSAGE, kw_ev);
}

/***************************************************************************
 *  The C_UDP_S is down (stopped by itself, or its start failed): mark it,
 *  and arm its restart. The backoff doubles when the last try did not hold
 *  RESTART_HOLD_MS, else it goes back to timeout_base. Return the delay.
 ***************************************************************************/
PRIVATE json_int_t schedule_restart(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(priv->restart_backoff_ms > 0 &&
            priv->t_restart_holds && !test_msectimer(priv->t_restart_holds)) {
        priv->restart_backoff_ms = MIN(2*priv->restart_backoff_ms, RESTART_BACKOFF_MAX_MS);
    } else {
        priv->restart_backoff_ms = priv->timeout_base;
    }

    priv->udp_stopped = TRUE;
    set_timeout(priv->timer_restart, priv->restart_backoff_ms);
    return priv->restart_backoff_ms;
}

/***************************************************************************
 *  Start again the C_UDP_S that is down. A start that fails is tried
 *  again after the next backoff.
 ***************************************************************************/
PRIVATE void restart_udp_server(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(gobj_is_running(priv->gobj_udp_s)) {
        gobj_stop(priv->gobj_udp_s);
    }

    /*
     *  Marked live BEFORE the start: an EV_STOPPED inside it marks it down
     *  again, and is not overwritten here
     */
    priv->udp_stopped = FALSE;
    priv->t_restart_holds = start_msectimer(RESTART_HOLD_MS);

    if(gobj_start(priv->gobj_udp_s) < 0) {
        // Error already logged
        json_int_t backoff_ms = schedule_restart(gobj);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "UDP server cannot start again, it is tried again after a backoff",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            "backoff_ms",   "%ld", (long)backoff_ms,
            "tx_dropped",   "%ld", (long)priv->tx_dropped,
            NULL
        );
        return;
    }
    if(priv->udp_stopped) {
        return; // stopped again inside its start: said, and scheduled
    }

    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",          "%s", "UDP server started again",
        "url",          "%s", gobj_read_str_attr(gobj, "url"),
        "tx_dropped",   "%ld", (long)priv->tx_dropped,
        NULL
    );
    priv->tx_dropped = 0;
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
        socklen_t addrlen = gbuffer_getaddrlen(gbuf);
        if(addrlen > 0 && addrlen <= sizeof(ch->addr)) {
            memcpy(&ch->addr, gbuffer_getaddr(gbuf), addrlen);
            ch->addrlen = addrlen;
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
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, KW_REQUIRED);
    if(!gbuf) {
        // Error already logged
        KW_DECREF(kw);
        return -1;
    }

    /*
     *  Not only after its EV_STOPPED: a C_UDP_S still stopping
     *  (ST_WAIT_STOPPED, a send in flight) takes no EV_TX_DATA either
     */
    if(priv->udp_stopped || !gobj_in_this_state(priv->gobj_udp_s, ST_IDLE)) {
        if(priv->tx_dropped++ == 0) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
                "msg",          "%s", "EV_SEND_MESSAGE while the UDP server is stopped, dropped",
                "url",          "%s", gobj_read_str_attr(gobj, "url"),
                "udp_state",    "%s", gobj_current_state(priv->gobj_udp_s),
                "len",          "%d", (int)gbuffer_leftbytes(gbuf),
                NULL
            );
        }
        KW_DECREF(kw);
        return -1;  // the first one logged, the count at the restart or at the stop
    }

    /*
     *  C_UDP_S sends to the address of the gbuffer. Without one, the label
     *  names the peer: a known channel, whose address is taken. Up to 7.25.4
     *  the channel was looked up and not used: a send by address logged
     *  "UDP channel NOT FOUND", and a send by label had no address: the
     *  kernel refused it (EINVAL) and C_UDP_S stopped.
     */
    if(gbuffer_getaddrlen(gbuf) == 0) {
        const char *udp_channel = gbuffer_getlabel(gbuf);
        UDP_CHANNEL *ch = find_udp_channel(gobj, udp_channel);
        if(!ch || ch->addrlen == 0) {
            gobj_log_error(gobj, 0,
                "function",             "%s", __FUNCTION__,
                "msgset",               "%s", MSGSET_PARAMETER,
                "msg",                  "%s", "EV_SEND_MESSAGE without a peer: no address, and its label names no known peer; dropped",
                "udp_channel",          "%s", udp_channel?udp_channel:"",
                "len",                  "%d", (int)gbuffer_leftbytes(gbuf),
                NULL
            );
            KW_DECREF(kw);
            return -1;
        }
        gbuffer_setaddr(gbuf, (struct sockaddr *)&ch->addr, ch->addrlen);
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
 *  The C_UDP_S whose stop a start waited for has stopped: start it
 ***************************************************************************/
PRIVATE int ac_start_udp_server(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  Only the start it was posted for: a stop of this gobj after the post
     *  took it back (start_posted cleared), and a start after that stop may
     *  have started the C_UDP_S itself, which must not be started again
     */
    BOOL wanted = priv->start_posted;
    priv->start_posted = FALSE;
    if(wanted && gobj_is_running(gobj) && !gobj_is_running(priv->gobj_udp_s)) {
        restart_udp_server(gobj);   // a start that fails: tried again after a backoff
    }
    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  The backoff of a restart is over
 ***************************************************************************/
PRIVATE int ac_restart_timeout(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!priv->udp_stopped) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Restart timer of the UDP server fired with the server not marked down",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            NULL
        );
        KW_DECREF(kw);
        return -1;
    }
    restart_udp_server(gobj);

    KW_DECREF(kw);
    return 0;
}

/***************************************************************************
 *  The C_UDP_S stopped. By our own stop (mt_stop): its end. From outside
 *  (it no longer runs): not started again. By itself (it still runs): see
 *  the header, started again after a backoff.
 ***************************************************************************/
PRIVATE int ac_udp_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    if(!gobj_is_running(gobj)) {
        KW_DECREF(kw);
        return 0;
    }

    if(priv->start_when_stopped) {
        /*
         *  The end of the stop a start of this gobj waited for. Started on
         *  the next cycle: this runs inside the stop of the C_UDP_S.
         */
        priv->start_when_stopped = FALSE;
        priv->start_posted = TRUE;
        gobj_post_event(gobj, EV_START_UDP_SERVER, json_object(), gobj);
        KW_DECREF(kw);
        return 0;
    }

    if(!gobj_is_running(priv->gobj_udp_s)) {
        gobj_log_info(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "UDP server stopped from outside, it is not started again",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            NULL
        );
        KW_DECREF(kw);
        return 0;
    }

    if(!priv->udp_stopped) {
        json_int_t backoff_ms = schedule_restart(gobj);
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
            "msg",          "%s", "UDP server stopped by itself, it is started again after a backoff",
            "url",          "%s", gobj_read_str_attr(gobj, "url"),
            "backoff_ms",   "%ld", (long)backoff_ms,
            NULL
        );
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
GOBJ_DEFINE_EVENT(EV_START_UDP_SERVER);  // a start that waited for the end of a stop of the C_UDP_S

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
        {EV_TIMEOUT,            ac_restart_timeout, 0},
        {EV_STOPPED,            ac_udp_stopped,     0},
        {EV_START_UDP_SERVER,   ac_start_udp_server, 0},
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
        {EV_TIMEOUT,            0},
        {EV_STOPPED,            0},
        {EV_START_UDP_SERVER,   0},
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
