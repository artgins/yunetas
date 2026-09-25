/***********************************************************************
 *          C_IEVENT_SRV.C
 *          Ievent_srv GClass.
 *
 *          Inter-event server side
 *
 *          Copyright (c) 2016 Niyamaka.
 *          Copyright (c) 2024-2026, ArtGins.
 *          All Rights Reserved.
***********************************************************************/
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <gobj.h>
#include <g_ev_kernel.h>
#include <g_st_kernel.h>
#include <helpers.h>

#include "c_timer.h"
#include "msg_ievent.h"
#include "c_ievent_srv.h"

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define MAX_LOG_DUMP_SIZE (256)     // Cap the data dump added to logs, for very large packets

/*
 *  The `__config__` keys a remote peer may set in its subscription. The
 *  framework reads three more (__hard_subscription__, __own_event__,
 *  __rename_event_name__), and they belong to the owner of the subscriber,
 *  never to a peer: see filter_peer_subscription_config().
 */
PRIVATE const char *peer_subscription_config_keys[] = {
    "__first_shot__",   // the publisher sends its current state on subscription
    0
};

/*
 *  Of what a peer can repeat frame after frame (keys it may not set, a
 *  subscription over the size cap, the withdrawal of one it does not hold),
 *  one log of each kind per channel and interval; the next one written says
 *  how many were not.
 */
#define PEER_LOG_INTERVAL_MS    (10*1000)

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef enum {
    PEER_LOG_CONFIG_KEYS = 0,
    PEER_LOG_SUBSCRIPTION_KEYS,
    PEER_LOG_OVERSIZE,
    PEER_LOG_NO_MATCH,
    PEER_LOG_REFUSED_WITHDRAWN,
    PEER_LOG_KINDS
} peer_log_kind_t;

typedef struct {
    uint64_t t_next;        // msectimer: no log of this kind before it
    json_int_t suppressed;  // logs of this kind not written since the last one
} peer_log_t;

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
PRIVATE int send_static_iev(
    hgobj gobj,
    const char *event,
    json_t *kw, // owned and serialized,
    hgobj src
);
PRIVATE json_t *build_srv_ievent_request(
    hgobj gobj,
    const char *src_service,
    const char *dst_service
);
PRIVATE BOOL is_service_authorized(hgobj gobj, hgobj gobj_service);
PRIVATE void filter_peer_subscription_config(
    hgobj gobj,
    json_t *iev_kw,     // not owned
    gobj_event_t event,
    BOOL warn
);
PRIVATE BOOL is_subscription_authorized(
    hgobj gobj,
    hgobj gobj_service,
    gobj_event_t event,
    json_t *kw_subs,    // not owned
    BOOL log_refusal
);
PRIVATE BOOL peer_log_allowed(
    hgobj gobj,
    peer_log_kind_t kind,
    json_int_t *suppressed
);
PRIVATE size_t peer_json_dump(json_t *jn, char *bf, size_t bfsize);
PRIVATE json_int_t count_refused_subscription(
    hgobj gobj,
    hgobj gobj_service,
    gobj_event_t event,
    int add
);
PRIVATE json_t *peer_subscription_kw(
    hgobj gobj,
    json_t *iev_kw,     // not owned
    gobj_event_t event,
    BOOL warn
);
PRIVATE BOOL peer_has_subscription_room(
    hgobj gobj,
    hgobj gobj_service,
    gobj_event_t event,
    json_t *kw_subs     // not owned
);
PRIVATE json_t *find_peer_subscriptions(
    hgobj gobj,
    hgobj gobj_service,
    gobj_event_t event,
    json_t *kw_subs     // not owned
);
PRIVATE int reject_unrouted_iev(
    hgobj gobj,
    gobj_event_t iev_event,
    json_t *iev_kw,     // owned
    json_t *kw,         // owned
    const char *comment,
    hgobj src
);

/***************************************************************************
 *          Data: config, public data, private data
 ***************************************************************************/

/*---------------------------------------------*
 *      Attributes
 *---------------------------------------------*/
PRIVATE sdata_desc_t attrs_table[] = {
/*-ATTR-type------------name--------------------flag---------default-description---------- */

// HACK set by c_authz, this gclass is an external entry gate!
SDATA (DTP_STRING,      "__username__",         SDF_VOLATIL, "", "Username, WARNING set by c_authz"),
SDATA (DTP_STRING,      "__session_id__",       SDF_VOLATIL, "", "Session ID, WARNING set by c_authz"),
SDATA (DTP_JSON,        "jwt_payload",          SDF_VOLATIL, 0, "JWT payload (decoded user data) of authenticated user, WARNING set by c_authz"),

// SEC-06: Cookie header captured from the HTTP Upgrade request.
// Used as a JWT source when no jwt field is present in IDENTITY_CARD.
SDATA (DTP_STRING,      "http_cookie",          SDF_VOLATIL, "", "Cookie header from HTTP Upgrade (SEC-06)"),

SDATA (DTP_STRING,      "client_yuno_role",     SDF_VOLATIL, 0, "yuno role of connected client"),
SDATA (DTP_STRING,      "client_yuno_name",     SDF_VOLATIL, 0, "yuno name of connected client"),
SDATA (DTP_STRING,      "client_yuno_service",  SDF_VOLATIL, 0, "yuno service of connected client"),
SDATA (DTP_STRING,      "this_service",         SDF_VOLATIL, 0, "dst_service at identity_card"),
SDATA (DTP_POINTER,     "gobj_service",         SDF_VOLATIL, 0, "gobj of identity_card dst_service"),
SDATA (DTP_BOOLEAN,     "authenticated",        SDF_VOLATIL, 0, "True if entry was authenticated"),
SDATA (DTP_JSON,        "identity_card",        SDF_VOLATIL, "", "Identity Card of clisrv"),

// Services this channel is authorized to reach: the keys of the
// services_roles dict returned by gobj_authenticate() at identity_card time.
// That set is derived from the user's real roles in treedb (append_role in
// c_authz.c), NOT from the client-supplied required_services, so it cannot be
// spoofed. A single authentication against the primary dst_service can grant
// several services (required_services) — see the multi-service GUI frontends.
SDATA (DTP_JSON,        "authorized_services",  SDF_VOLATIL, "[]", "Service names this channel may reach (keys of services_roles)"),

// Superuser channel: the authenticated user holds a role with service="*"
// (root). Set by c_authz at identity_card time (the "superuser" field of the
// authenticate response, computed from real treedb roles, not client-supplied).
// A superuser bypasses the per-service routing gate (is_service_authorized):
// any realm/service/permission, so it reaches __yuno__ and any sibling service.
SDATA (DTP_BOOLEAN,     "is_superuser",         SDF_VOLATIL, 0, "Channel user holds a wildcard (root) role; bypasses the service gate"),

SDATA (DTP_INTEGER,     "timeout_idgot",        SDF_RD, "5000", "timeout waiting Identity Card"),

// What one peer may hold. Every subscription costs a scan of the publisher's
// subscriptions, when it is made and on every publish, and a peer could make
// them without end (20000 of them blocked the loop for 80 s).
SDATA (DTP_INTEGER,     "max_subscriptions",    SDF_RD, "5000", "Maximum subscriptions a peer may hold on this channel, 0 no limit. Above it a subscription is refused, logged once until the peer is under it again"),
SDATA (DTP_INTEGER,     "max_subscription_size",SDF_RD, "16384", "Maximum size, in bytes of compact json, of the __filter__ and of the __global__ of a peer's subscription, 0 no limit. A bigger one is refused"),

SDATA (DTP_POINTER,     "user_data",            0, 0, "user data"),
SDATA (DTP_POINTER,     "user_data2",           0, 0, "more user data"),
SDATA (DTP_POINTER,     "subscriber",           0, 0, "subscriber of output-events. Not a child gobj."),
SDATA_END()
};

/*---------------------------------------------*
 *      GClass trace levels
 *---------------------------------------------*/
enum {
    TRACE_IEVENTS       = 0x0001,
    TRACE_IEVENTS2      = 0x0002,
    TRACE_IDENTITY_CARD = 0x0004,
};
PRIVATE const trace_level_t s_user_trace_level[16] = {
{"ievents",        "Trace inter-events with metadata of kw"},
{"ievents2",       "Trace inter-events with full kw"},
{"identity-card",  "Trace identity_card messages"},
{0, 0},
};


/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
typedef struct _PRIVATE_DATA {
    int32_t timeout; //TODO
    BOOL inform_on_close;

    const char *client_yuno_name;
    const char *client_yuno_role;
    const char *client_yuno_service;
    const char *this_service;
    hgobj gobj_service;
    hgobj subscriber;

    hgobj timer;

    json_t *refused_subscriptions;  // "service`event" -> subscriptions refused (authz, size, cap) (not a kw path)
    BOOL subscriptions_capped;      // max_subscriptions was logged, until the peer is under it again
    peer_log_t peer_log[PEER_LOG_KINDS];
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
    priv->refused_subscriptions = json_object();

    /*
     *  CHILD subscription model
     */
    hgobj subscriber = (hgobj)gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, NULL, NULL, subscriber);

    /*
     *  Do copy of heavy-used parameters, for quick access.
     *  HACK The writable attributes must be repeated in mt_writing method.
     */
    SET_PRIV(client_yuno_name,          gobj_read_str_attr)
    SET_PRIV(client_yuno_role,          gobj_read_str_attr)
    SET_PRIV(client_yuno_service,       gobj_read_str_attr)
    SET_PRIV(this_service,              gobj_read_str_attr)
    SET_PRIV(subscriber,                gobj_read_pointer_attr)
}

/***************************************************************************
 *      Framework Method writing
 ***************************************************************************/
PRIVATE void mt_writing(hgobj gobj, const char *path)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    IF_EQ_SET_PRIV(client_yuno_name,        gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(client_yuno_role,      gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(client_yuno_service,   gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(this_service,          gobj_read_str_attr)
    ELIF_EQ_SET_PRIV(gobj_service,          gobj_read_pointer_attr)
    END_EQ_SET_PRIV()
}

/***************************************************************************
 *      Framework Method destroy
 ***************************************************************************/
PRIVATE void mt_destroy(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    JSON_DECREF(priv->refused_subscriptions)
}

/***************************************************************************
 *      Framework Method start
 ***************************************************************************/
PRIVATE int mt_start(hgobj gobj)
{
    /*
     *  The mirror of mt_stop: the protocol gobj (C_WEBSOCKET, C_PROT_TCP4H)
     *  is started with its gate. Since mt_stop stops it, a channel that is
     *  disabled and enabled again (C_IOGATE disable-channel, enable-channel)
     *  came back with its protocol gobj stopped, and every later client was
     *  accepted, its handshake answered, and then never read.
     */
    hgobj gobj_bottom = gobj_bottom_gobj(gobj);
    if(gobj_bottom && !gobj_is_running(gobj_bottom)) {
        gobj_start(gobj_bottom);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stop
 ***************************************************************************/
PRIVATE int mt_stop(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    clear_timeout(priv->timer);

    /*
     *  Each layer of a channel stops the one below it (C_CHANNEL, C_IEVENT_CLI,
     *  C_WEBSOCKET, C_PROT_TCP4H), so a plain gobj_stop() of the gate reaches
     *  the transport. This one did not: the protocol gobj (C_WEBSOCKET,
     *  C_PROT_TCP4H) stayed running after its gate was stopped with
     *  gobj_stop() -- the way the yuno stops an autostart service -- and the
     *  yuno died with "Destroying a RUNNING gobj".
     *
     *  NOT when the peer leaves: the protocol gobj of a static channel tree
     *  lives as long as its gate, and serves every connection accepted on it.
     */
    hgobj gobj_bottom = gobj_bottom_gobj(gobj);
    if(gobj_bottom && gobj_is_running(gobj_bottom)) {
        gobj_stop(gobj_bottom);
    }
    return 0;
}

/***************************************************************************
 *      Framework Method stats
 ***************************************************************************/
PRIVATE json_t *mt_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src)
{
    if(gobj_current_state(gobj) != ST_SESSION) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Not in session",
            "stats",        "%s", stats?stats:"",
            NULL
        );
        KW_DECREF(kw)
        return NULL;
    }

    if(!kw) {
        kw = json_object();
    }

    /*
     *      __REQUEST__ __MESSAGE__
     */
    json_t *jn_ievent_id = build_srv_ievent_request(
        gobj,
        gobj_name(src),
        kw_get_str(gobj, kw, "service", "", 0)
    );

    json_t *__md_stats__ = kw_get_dict_value(gobj, kw, "__md_stats__", NULL, KW_EXTRACT);
    if(!__md_stats__) {
        __md_stats__ = json_object();
    }

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        STATS_STACK_ID,
        json_pack("{s:s, s:o}",   // owned
            "stats", stats,
            "kw", __md_stats__
        )
    );

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        IEVENT_STACK_ID,
        jn_ievent_id   // owned
    );

    json_object_set_new(kw, "__stats__", json_string(stats));
    msg_iev_set_msg_type(gobj, kw, "__stats__");

    send_static_iev(gobj, EV_MT_STATS, kw, src);

    return NULL;   // return NULL on asynchronous response.
}

/***************************************************************************
 *      Framework Method command
 ***************************************************************************/
PRIVATE json_t *mt_command(hgobj gobj, const char *command, json_t *kw, hgobj src)
{
    if(gobj_current_state(gobj) != ST_SESSION) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Not in session",
            "command",      "%s", command,
            NULL
        );
        KW_DECREF(kw)
        return NULL;
    }

    if(!kw) {
        kw = json_object();
    }

    /*
     *      __REQUEST__ __MESSAGE__
     */
    json_t *jn_ievent_id = build_srv_ievent_request(
        gobj,
        gobj_name(src),
        kw_get_str(gobj, kw, "service", "", 0)
    );

    json_t *__md_command__ = kw_get_dict_value(gobj, kw, "__md_command__", NULL, KW_EXTRACT);
    if(!__md_command__) {
        __md_command__ = json_object();
    }

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        COMMAND_STACK_ID,
        json_pack("{s:s, s:o}",   // owned
            "command", command,
            "kw", __md_command__
        )
    );

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        IEVENT_STACK_ID,
        jn_ievent_id   // owned
    );

    json_object_set_new(kw, "__command__", json_string(command));
    msg_iev_set_msg_type(gobj, kw, "__command__");

    send_static_iev(gobj, EV_MT_COMMAND, kw, src);

    return NULL;   // return NULL on asynchronous response.
}

/***************************************************************************
 *      Framework Method inject_event
 ***************************************************************************/
PRIVATE int mt_inject_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    if(gobj_current_state(gobj) != ST_SESSION) {
        gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Not in session",
            "event",        "%s", event,
            NULL
        );
        KW_DECREF(kw)
        return -1;
    }
    if(!kw) {
        kw = json_object();
    } else if(kw->refcount > 1) {
        /*
         *  Below, the kw is changed (the ievent stack, the message type, and
         *  the binary fields serialized by send_static_iev()): change a kw
         *  of our own. A kw somebody else holds too is, above all, a
         *  published one, which gobj_publish_event() hands the SAME to
         *  every subscriber that does not rewrite it: up to 7.25.4 what
         *  this gobj wrote in it reached every subscriber after it, and
         *  the publisher. Its top level is enough (kw_twin()): the one
         *  nested value changed in place, __md_iev__, is copied below.
         */
        json_t *kw_own = kw_twin(gobj, kw);
        KW_DECREF(kw)
        if(!kw_own) {
            // Error already logged
            return -1;
        }
        kw = kw_own;
    }

    /*
     *  The ievent stack and the message type are written INTO __md_iev__:
     *  a copy of our own when anybody else holds it (the kw this one was
     *  twinned from, or the __global__ of the subscription that put it).
     */
    json_t *md_iev = json_object_get(kw, "__md_iev__");
    if(md_iev && md_iev->refcount > 1) {
        json_object_set_new(kw, "__md_iev__", json_deep_copy(md_iev));
    }

    /*
     *      __MESSAGE__
     *  Put the ievent if it doesn't come with it,
     *  if it does come with it, it's because it will be a response
     */
    json_t *jn_request = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, FALSE);
    if(jn_request) {
        /*
         *      __RESPONSE__
         */
    } else {
        /*
         *      __REQUEST__
         */
        json_t *jn_ievent_id = build_srv_ievent_request(
            gobj,
            gobj_name(src),
            kw_get_str(gobj, kw, "__service__", 0, 0)
        );
        json_object_del(kw, "__service__");

        msg_iev_push_stack(
            gobj,
            kw,         // not owned
            IEVENT_STACK_ID,
            jn_ievent_id   // owned
        );

        msg_iev_push_stack(
            gobj,
            kw,         // not owned
            "__message__",
            json_string(event)   // owned
        );
    }

    msg_iev_set_msg_type(gobj, kw, "__message__");

    return send_static_iev(gobj, event, kw, src);
}




            /***************************
             *      Commands
             ***************************/




            /***************************
             *      Local Methods
             ***************************/




/***************************************************************************
 *  __MESSAGE__
 ***************************************************************************/
PRIVATE json_t *build_srv_ievent_request(
    hgobj gobj,
    const char *src_service,
    const char *dst_service
) {
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    if(empty_string(dst_service)) {
        dst_service = priv->client_yuno_service;
    }
    json_t *jn_ievent_chain = json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "dst_yuno", priv->client_yuno_name,
        "dst_role", priv->client_yuno_role,
        "dst_service", dst_service,

        "src_yuno", gobj_yuno_name(),
        "src_role", gobj_yuno_role(),
        "src_service", src_service,

        "user", gobj_read_str_attr(gobj, "__username__"),
        "host", get_hostname()
    );
    return jn_ievent_chain;
}

/***************************************************************************
 *  Get bottom level gobj
 ***************************************************************************/
PRIVATE hgobj get_bottom_gobj(hgobj gobj)
{
    hgobj bottom_gobj = gobj_bottom_gobj(gobj);
    if(bottom_gobj) {
        return bottom_gobj;
    } else {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "NOT below gobj",
            NULL
        );
    }
    return 0;
}

/***************************************************************************
 *  Drop
 ***************************************************************************/
PRIVATE int drop(hgobj gobj)
{
    hgobj below_gobj = get_bottom_gobj(gobj);
    if(!below_gobj) {
        // Error already logged
        return -1;
    }

    gobj_send_event(below_gobj, EV_DROP, 0, gobj);
    return 0;
}

/***************************************************************************
 *  Route must be connected
 ***************************************************************************/
PRIVATE int send_static_iev(
    hgobj gobj,
    const char *event,
    json_t *kw, // owned and serialized,
    hgobj src
)
{
    hgobj below_gobj = get_bottom_gobj(gobj);
    if(!below_gobj) {
        // Error already logged
        return -1;
    }

    uint32_t trace_level = gobj_trace_level(gobj);
    if(trace_level) {
        char prefix[256];
        snprintf(prefix, sizeof(prefix),
            "INTRA-EVENT(%s^%s) %s ==> %s",
            gobj_yuno_role(),
            gobj_yuno_name(),
            gobj_short_name(src),
            gobj_short_name(below_gobj)
        );
        if((trace_level & TRACE_IEVENTS2)) {
            trace_inter_event2(gobj, prefix, event, kw);
        } else if((trace_level & TRACE_IEVENTS)) {
            trace_inter_event(gobj, prefix, event, kw);
        } else if((trace_level & TRACE_IDENTITY_CARD)) {
            if(event == EV_IDENTITY_CARD ||
                event == EV_IDENTITY_CARD_ACK ||
                event == EV_PLAY_YUNO ||
                event == EV_PLAY_YUNO_ACK ||
                event == EV_PAUSE_YUNO ||
                event == EV_PAUSE_YUNO_ACK
               ) {
                trace_inter_event2(gobj, prefix, event, kw);
            }
        }
    }

    gbuffer_t *gbuf = iev_create_to_gbuffer(
        gobj,
        event,
        kw     // own
    );
    if(!gbuf) {
        // error already logged
        return -1;
    }
    json_t *kw_send = json_pack("{s:I}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf
    );
    return gobj_send_event(below_gobj,
        EV_SEND_MESSAGE,
        kw_send,
        gobj
    );
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_open(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*-------------------------------*
     *      Reset "client data"
     *-------------------------------*/
    gobj_reset_volatil_attrs(gobj);

    /*
     *  SEC-06: save the Cookie header forwarded by c_websocket.c during
     *  the HTTP Upgrade.  ac_identity_card will use it as a JWT source
     *  when the IDENTITY_CARD message carries no explicit jwt field,
     *  which is the case for browser clients using the BFF/httpOnly-cookie
     *  authentication flow.
     */
    const char *http_cookie = kw_get_str(gobj, kw, "http_cookie", "", 0);
    if(!empty_string(http_cookie)) {
        gobj_write_str_attr(gobj, "http_cookie", http_cookie);
    }

    /*
     *  Route (channel) open.
     *  Wait Identity card
     */
    set_timeout(priv->timer, gobj_read_integer_attr(gobj, "timeout_idgot"));

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_on_close(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*---------------------------------------*
     *  Clear timeout
     *---------------------------------------*/
    clear_timeout(priv->timer);

    /*
     *  Delete external subscriptions
     */
    json_t *kw2match = json_object();

    kw_set_subdict_value(
        gobj,
        kw2match,
        "__local__", "__subscription_reference__",
        json_integer((json_int_t)(uintptr_t)gobj)
    );

    json_t *dl_s = gobj_find_subscribings(gobj, 0, kw2match, 0);

    /*
     *  Forced: nothing a peer subscribed may outlive its session. Up to
     *  7.25.4 a peer could ask `__hard_subscription__`, the cleanup did not
     *  force, and the subscription stayed on this channel -- which is
     *  static, and serves the next user who connects to it.
     */
    gobj_unsubscribe_list(gobj, dl_s, TRUE);
    json_object_clear(priv->refused_subscriptions);
    priv->subscriptions_capped = FALSE;
    memset(priv->peer_log, 0, sizeof(priv->peer_log));

    /*
     *  Route (channel) close.
     */
    if(priv->inform_on_close) {
        priv->inform_on_close = FALSE;
        json_t *kw_on_close = json_pack("{s:s, s:s, s:s}",
            "client_yuno_name", gobj_read_str_attr(gobj, "client_yuno_name"),
            "client_yuno_role", gobj_read_str_attr(gobj, "client_yuno_role"),
            "client_yuno_service", gobj_read_str_attr(gobj, "client_yuno_service")
        );
        kw_update_missing(gobj, kw_on_close, kw);

        gobj_publish_event(gobj, EV_ON_CLOSE, kw_on_close);
    }

    /*-------------------------------*
     *      Reset "client data"
     *-------------------------------*/
    gobj_reset_volatil_attrs(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_timeout_wait_idGot(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    gobj_log_info(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_CONNECT_DISCONNECT,
        "msg",          "%s", "Timeout waiting identity card",
        NULL
    );
    drop(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  remote ask
 *  Somebody wants our services.
 ***************************************************************************/
PRIVATE int ac_identity_card(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *      __MESSAGE__
     */
    /*
     *  Final point of the request
     */
    json_t *jn_request = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, TRUE);
    if(!jn_request) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "no ievent_gate_stack",
            NULL
        );
        gobj_trace_json(gobj, kw, "no ievent_gate_stack");
        KW_DECREF(kw)
        return -1;
    }
    const char *iev_dst_yuno = kw_get_str(gobj, jn_request, "dst_yuno", "", 0);
    const char *iev_dst_role = kw_get_str(gobj, jn_request, "dst_role", "", 0);
    const char *iev_dst_service = kw_get_str(gobj, jn_request, "dst_service", "", 0);
    const char *iev_src_yuno = kw_get_str(gobj, jn_request, "src_yuno", "", 0);
    const char *iev_src_role = kw_get_str(gobj, jn_request, "src_role", "", 0);
    const char *iev_src_service = kw_get_str(gobj, jn_request, "src_service", "", 0);

    /*---------------------------------------*
     *  Clear timeout
     *---------------------------------------*/
    clear_timeout(priv->timer);

    /*------------------------------------*
     *  Analyze
     *------------------------------------*/
    const char *peername = gobj_read_str_attr(src, "peername");
    const char *sockname = gobj_read_str_attr(src, "sockname");

    /*------------------------------------*
     *  Match wanted yuno role. Required.
     *------------------------------------*/
    if(strcasecmp(iev_dst_role, gobj_yuno_role())!=0) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "dst_role NOT MATCH",
            "my_role",      "%s", gobj_yuno_role(),
            "dst_role",     "%s", iev_dst_role,
            "peername",     "%s", peername?peername:"",
            "sockname",     "%s", sockname?sockname:"",
            NULL
        );
        gobj_trace_json(gobj, kw, "dst_role NOT MATCH");
        KW_DECREF(kw)
        return -1;
    }

    /*--------------------------------*
     *  Match wanted yuno name
     *--------------------------------*/
    if(!empty_string(iev_dst_yuno)) {
        if(strcasecmp(iev_dst_yuno, gobj_yuno_name())!=0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL,
                "msg",          "%s", "dst_yuno NOT MATCH",
                "my_yuno",      "%s", gobj_yuno_name(),
                "dst_yuno",     "%s", iev_dst_yuno,
                "peername",     "%s", peername?peername:"",
                "sockname",     "%s", sockname?sockname:"",
                NULL
            );
            gobj_trace_json(gobj, kw, "dst_yuno NOT MATCH");
            KW_DECREF(kw)
            return -1;
        }
    }

    /*------------------------------------*
     *  Save client yuno role. Required.
     *------------------------------------*/
    if(empty_string(iev_src_role)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "GOT identity card without yuno role",
            "peername",     "%s", peername?peername:"",
            "sockname",     "%s", sockname?sockname:"",
            NULL
        );
        gobj_trace_json(gobj, kw, "GOT identity card without yuno role");
        KW_DECREF(kw)
        return -1;
    }

    /*---------------------------------------*
     *  Save client yuno service. Required.
     *---------------------------------------*/
    if(empty_string(iev_src_service)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "GOT identity card without yuno service",
            "peername",     "%s", peername?peername:"",
            "sockname",     "%s", sockname?sockname:"",
            NULL
        );
        gobj_trace_json(gobj, kw, "GOT identity card without yuno service");
        KW_DECREF(kw)
        return -1;
    }

    /*-----------------------------------*
     *  Find wanted service. Required.
     *-----------------------------------*/
    hgobj gobj_service = gobj_find_service(iev_dst_service, FALSE);
    if (!gobj_service) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "dst_srv NOT FOUND in this yuno",
            "dst_service",  "%s", iev_dst_service,
            "peername",     "%s", peername?peername:"",
            "sockname",     "%s", sockname?sockname:"",
            NULL
        );
        gobj_trace_json(gobj, kw, "dst_srv NOT FOUND in this yuno");
        KW_DECREF(kw)
        return -1;
    }

    /*-------------------------*
     *  Do authentication
     *-------------------------*/
    /*
     *  SEC-06: if no jwt was supplied in the IDENTITY_CARD message (browser
     *  clients using the BFF/httpOnly-cookie flow send an empty string),
     *  try to extract the access_token from the Cookie header that was
     *  captured during the HTTP Upgrade and stored in "http_cookie".
     *
     *  Cookie string format: "name1=val1; name2=val2; …"
     *  We look for the "access_token" cookie set by the BFF.
     */
    const char *jwt_in_card = kw_get_str(gobj, kw, "jwt", NULL, 0);
    if(empty_string(jwt_in_card)) {
        const char *http_cookie = gobj_read_str_attr(gobj, "http_cookie");
        if(!empty_string(http_cookie)) {
            const char *search = "access_token=";
            size_t search_len = strlen(search);
            const char *p = strstr(http_cookie, search);
            /*
             *  SEC-06: verify we matched the exact cookie name, not a
             *  substring (e.g. "xaccess_token=").  The match must be at
             *  the start of the string or preceded by "; " / " ".
             */
            while(p) {
                if(p == http_cookie || *(p - 1) == ' ' || *(p - 1) == ';') {
                    break;  /* exact cookie name match */
                }
                p = strstr(p + search_len, search);
            }
            if(p) {
                p += search_len;
                const char *end = strpbrk(p, "; ");
                size_t jwt_len = end ? (size_t)(end - p) : strlen(p);
                if(jwt_len > 0 && jwt_len < 8192) {
                    char jwt_buf[8192];
                    memcpy(jwt_buf, p, jwt_len);
                    jwt_buf[jwt_len] = '\0';
                    json_object_set_new(kw, "jwt", json_string(jwt_buf));
                }
            }
        }
    }

    /*
     *  WARNING if not a localhost connection the authentication must be required!
     *  See mt_authenticate of c_authz.c
     */
    KW_INCREF(kw)
    json_t *jn_resp = gobj_authenticate(gobj_service, kw, gobj);
    if(kw_get_int(gobj, jn_resp, "result", -1, KW_REQUIRED|KW_CREATE)<0) {
        /*
         *  Don't log a warning here — every code path inside
         *  c_authz.c:mt_authenticate that returns result < 0 already
         *  emits its own gobj_log_warning with the precise reason
         *  ("Invalid token", "JWT expired", ...).  Logging again
         *  here just produced a duplicate ("Authentication
         *  rejected") with less detail, two lines per failed
         *  authentication, which inflates the warning counters
         *  monitors use to detect attacks.
         */

        /*
         *      __RESPONSE__ __MESSAGE__
         */
        JSON_INCREF(kw)
        json_t *kw_answer = msg_iev_set_back_metadata(
            gobj,
            kw,
            jn_resp,
            TRUE
        );

        // TODO why don't send a mark in the message, the when transmit is done, drop the connection?
        //      so we avoid create a timer with each ievent_srv (could it be millions)
        send_static_iev(
            gobj,
            EV_IDENTITY_CARD_ACK,
            kw_answer,      // own
            src
        );

        set_timeout(priv->timer, gobj_read_integer_attr(gobj, "timeout_idgot"));

        KW_DECREF(kw)
        return 0; // Don't return -1, don't drop connection, let send negative ack. Drop by timeout.
    }

    /*------------------------*
     *  Gate authenticated
     *------------------------*/
    /*----------------------------------------------*
     *      Save "client data"
     *      Set connection name if name is empty
     *----------------------------------------------*/
    gobj_write_bool_attr(gobj, "authenticated", TRUE);
    gobj_write_str_attr(gobj, "client_yuno_role", iev_src_role);
    gobj_write_str_attr(gobj, "client_yuno_service", iev_src_service);
    if(empty_string(iev_src_yuno)) {
        // anonymous yuno name
        char temp[80];
        snprintf(temp, sizeof(temp), "^%s-%s", peername, sockname);
        gobj_write_str_attr(gobj, "client_yuno_name", temp);
    } else {
        gobj_write_str_attr(gobj, "client_yuno_name", iev_src_yuno);
    }

    gobj_write_str_attr(gobj, "this_service", iev_dst_service);
    gobj_write_pointer_attr(gobj, "gobj_service", gobj_service);

    /*----------------------------------------------------------*
     *  Record the services this channel is authorized to reach.
     *  gobj_authenticate() returns services_roles: a dict keyed by every
     *  service the user holds a role in (the primary dst_service plus any
     *  required_services granted by real treedb roles — see append_role in
     *  c_authz.c). Storing the key set lets ac_on_message / ac_mt_stats
     *  authorize a per-message dst_service without trusting the client's
     *  routing stack. The no-treedb path returns {dst_service:[]}, so the
     *  set degrades to the single primary service (unchanged behavior).
     *----------------------------------------------------------*/
    json_t *jn_authorized = json_array();
    json_t *services_roles = kw_get_dict(gobj, jn_resp, "services_roles", NULL, 0);
    if(services_roles) {
        const char *svc_name; json_t *jn_roles;
        json_object_foreach(services_roles, svc_name, jn_roles) {
            json_array_append_new(jn_authorized, json_string(svc_name));
        }
    }
    gobj_write_new_json_attr(gobj, "authorized_services", jn_authorized); // owned

    /*
     *  Superuser (root) bypasses the per-service gate. Computed by c_authz from
     *  the user's real treedb roles (a role with service="*"), never from the
     *  client-supplied routing stack.
     */
    gobj_write_bool_attr(gobj, "is_superuser",
        kw_get_bool(gobj, jn_resp, "superuser", 0, 0)
    );

    // HACK comment next sentence, already set by gobj_authenticate(), 24-Oct-2023
    // gobj_write_str_attr(gobj, "__username__", kw_get_str(jn_resp, "username", "", KW_REQUIRED));

    // json_object_set(kw, "jwt", kw_get_dict_value(jn_resp, "jwt_payload", json_null(), KW_REQUIRED)); // HACK delete original jwt

    json_t *jwt_payload = gobj_read_json_attr(gobj, "jwt_payload");
    json_object_set(kw, "jwt", jwt_payload?jwt_payload:json_null()); // HACK delete original jwt
    gobj_write_json_attr(gobj, "identity_card", kw);

    /*----------------------------------------------*
     *  Change to state SESSION,
     *  send answer and inform.
     *----------------------------------------------*/
    gobj_change_state(gobj, ST_SESSION);

    /*
     *      __RESPONSE__ __MESSAGE__
     */
    JSON_INCREF(kw)
    json_t *kw_answer = msg_iev_set_back_metadata(
        gobj,
        kw,
        jn_resp,
        TRUE
    );

    send_static_iev(
        gobj,
        EV_IDENTITY_CARD_ACK,
        kw_answer,      // own
        src
    );

    /*-----------------------------------------------------------*
     *  Publish the new client
     *-----------------------------------------------------------*/
    priv->inform_on_close = TRUE;

    json_t *kw_on_open = json_pack("{s:s, s:s, s:s, s:O}",
        "client_yuno_name", gobj_read_str_attr(gobj, "client_yuno_name"),
        "client_yuno_role", gobj_read_str_attr(gobj, "client_yuno_role"),
        "client_yuno_service", gobj_read_str_attr(gobj, "client_yuno_service"),
        "identity_card", kw // REQUIRED for controlcenter, agent!!
    );
    kw_set_dict_value(
        gobj,
        kw_on_open,
        "__username__",
        json_string(gobj_read_str_attr(gobj, "__username__"))
    );

    gobj_publish_event(gobj, EV_ON_OPEN, kw_on_open);

    JSON_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  Is this channel authorized to reach gobj_service?
 *
 *  Authentication is per-service, but a single identity_card can grant
 *  several services (the primary dst_service plus required_services the user
 *  has roles in). The authorized set was captured from services_roles at
 *  identity_card time (see ac_identity_card). The primary gobj_service is
 *  always allowed; before the identity_card it is NULL, in which case the
 *  caller (a pre-session message) is not subject to this gate.
 ***************************************************************************/
PRIVATE BOOL is_service_authorized(hgobj gobj, hgobj gobj_service)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *  A superuser (root: a role with service="*") reaches any service of the
     *  node, including __yuno__ and sibling services. This is not a cross-service
     *  escalation: root means any realm/service/permission by definition. The
     *  flag is set by c_authz from the user's real treedb roles, so it cannot be
     *  spoofed by the client's routing stack. What a command may actually DO is
     *  still governed by the per-command authz layer (c_authz), not this gate.
     */
    if(gobj_read_bool_attr(gobj, "is_superuser")) {
        return TRUE;
    }

    if(!priv->gobj_service || gobj_service == priv->gobj_service) {
        return TRUE;
    }

    const char *service_name = gobj_name(gobj_service);
    json_t *jn_authorized = gobj_read_json_attr(gobj, "authorized_services"); // not mine
    size_t idx; json_t *jn_svc;
    json_array_foreach(jn_authorized, idx, jn_svc) {
        const char *authorized_name = json_string_value(jn_svc);
        if(!authorized_name) {
            // ac_identity_card fills the list from json_object_foreach keys,
            // always strings: a non-string entry is a broken invariant.
            gobj_log_error(gobj, LOG_OPT_TRACE_STACK,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_INTERNAL,
                "msg",          "%s", "authorized_services entry is not a string",
                "idx",          "%d", (int)idx,
                NULL
            );
            continue;
        }
        if(strcasecmp(authorized_name, service_name)==0) {
            return TRUE;
        }
    }
    return FALSE;
}

/***************************************************************************
 *  The permission that guards a subscription to `gobj_service`: the one of
 *  its gclass whose alias is `__subscribe_event__`, so a subscription asks
 *  what a read of the same data asks (C_NODE: `read`), or the global
 *  `__subscribe_event__` when the gclass names none.
 ***************************************************************************/
PRIVATE const char *subscription_permission(hgobj gobj_service)
{
    const char *global_authz = "__subscribe_event__";
    const sdata_desc_t *it = gclass_authz_desc(gobj_gclass(gobj_service));
    while(it && it->name) {
        const char **alias = it->alias;
        while(alias && *alias) {
            if(strcmp(*alias, global_authz)==0) {
                return it->name;
            }
            alias++;
        }
        it++;
    }
    return global_authz;
}

/***************************************************************************
 *  May the principal of this channel subscribe `event` of `gobj_service`?
 *
 *  Gated opt-in, like the per-command check of command_parser(): it runs
 *  only when the yuno sets `enable_subscription_authz`, and only for an
 *  event its gclass declares EVF_AUTHZ_SUBSCRIBE. This gobj is the external
 *  entry gate, so every subscription arriving here is external and carries
 *  the authenticated `__username__` (set by C_AUTHZ, never by the peer);
 *  an internal gobj_subscribe_event() never comes through here and is never
 *  gated. A refusal is logged here (MSGSET_AUTH) when `log_refusal`: the
 *  caller asks it for the first refusal of the service and event on this
 *  channel, not for a peer that repeats it frame after frame.
 *
 *  Up to 7.25.4 the check was commented out (here since v6, and in
 *  gobj_subscribe_event()), so the EV_TREEDB_NODE_* feed of a treedb was
 *  open to any authenticated user, `read` or not.
 ***************************************************************************/
PRIVATE BOOL is_subscription_authorized(
    hgobj gobj,
    hgobj gobj_service,
    gobj_event_t event,
    json_t *kw_subs,    // not owned
    BOOL log_refusal
)
{
    hgobj yuno = gobj_yuno();
    BOOL authz_enabled = yuno &&
        gobj_has_attr(yuno, "enable_subscription_authz") &&
        gobj_read_bool_attr(yuno, "enable_subscription_authz");
    if(!authz_enabled) {
        return TRUE;
    }
    if(!gobj_has_output_event(gobj_service, event, EVF_AUTHZ_SUBSCRIBE)) {
        return TRUE;
    }

    const char *permission = subscription_permission(gobj_service);
    const char *username = gobj_read_str_attr(gobj, "__username__");
    json_t *kw_authz = json_pack("{s:s, s:s, s:O}",
        "event", event,
        "__username__", username?username:"",
        "kw", kw_subs
    );
    if(gobj_user_has_authz(gobj_service, permission, kw_authz, gobj)) {
        return TRUE;
    }

    if(log_refusal) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "No permission to subscribe event",
            "service",      "%s", gobj_short_name(gobj_service),
            "event",        "%s", event,
            "permission",   "%s", permission,
            "username",     "%s", username?username:"",
            NULL
        );
    }
    return FALSE;
}

/***************************************************************************
 *  Keep, of the `__config__` of a remote (un)subscription, only the keys a
 *  peer may set (peer_subscription_config_keys).
 *
 *  The framework reads three keys of `__config__` in gobj_subscribe_event(),
 *  and each one hands the peer a power over the publisher that it must
 *  never have:
 *    - `__hard_subscription__`: the subscription survives the close of the
 *      channel, so it goes to the NEXT user of this static channel -- who
 *      gets the feed without asking, past the subscription authz;
 *    - `__own_event__`: a failed delivery to this subscription stops the
 *      publish loop, and every subscriber after it misses the event (with
 *      the channel closed, every publish fails with "Not in session");
 *    - `__rename_event_name__`: the event is delivered to this gobj under
 *      another name, one of its own inputs among them.
 *  Up to 7.25.4 the peer's `__config__` went through whole. The keys the
 *  framework owns are dropped, and so is any key not in the list: a key a
 *  publisher reads is added to the list, and documented, on purpose.
 *
 *  `warn`: the subscription logs what it drops; the unsubscription repeats
 *  the same `__config__`, so it filters it the same way, quietly.
 ***************************************************************************/
PRIVATE void filter_peer_subscription_config(
    hgobj gobj,
    json_t *iev_kw,     // not owned
    gobj_event_t event,
    BOOL warn
)
{
    json_t *__config__ = kw_get_dict_value(gobj, iev_kw, "__config__", 0, 0);
    if(!__config__) {
        return;
    }
    if(!json_is_object(__config__)) {
        if(warn) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PROTOCOL,
                "msg",          "%s", "SUBSCRIBING __config__ is not a dict, ignored",
                "event",        "%s", event,
                "username",     "%s", gobj_read_str_attr(gobj, "__username__"),
                NULL
            );
        }
        json_object_del(iev_kw, "__config__");
        return;
    }

    json_t *jn_config = json_object();
    json_t *jn_dropped = json_array();
    const char *key; json_t *jn_value;
    json_object_foreach(__config__, key, jn_value) {
        BOOL allowed = FALSE;
        for(int i=0; peer_subscription_config_keys[i]; i++) {
            if(strcmp(key, peer_subscription_config_keys[i])==0) {
                allowed = TRUE;
                break;
            }
        }
        if(allowed) {
            json_object_set(jn_config, key, jn_value);
        } else {
            json_array_append_new(jn_dropped, json_string(key));
        }
    }

    json_int_t suppressed = 0;
    if(warn && json_array_size(jn_dropped) > 0 &&
            peer_log_allowed(gobj, PEER_LOG_CONFIG_KEYS, &suppressed)) {
        char dump[MAX_LOG_DUMP_SIZE];
        peer_json_dump(jn_dropped, dump, sizeof(dump));
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "SUBSCRIBING __config__ keys a peer may not set, ignored",
            "event",        "%s", event,
            "username",     "%s", gobj_read_str_attr(gobj, "__username__"),
            "keys",         "%s", dump,
            "suppressed",   "%ld", (long)suppressed,
            NULL
        );
    }
    JSON_DECREF(jn_dropped)

    if(json_object_size(jn_config) > 0) {
        json_object_set_new(iev_kw, "__config__", jn_config);
    } else {
        JSON_DECREF(jn_config)
        json_object_del(iev_kw, "__config__");
    }
}

/***************************************************************************
 *  May a log caused by the peer be written now? One of each kind per
 *  PEER_LOG_INTERVAL_MS: a peer that repeats a frame must not write a log
 *  line per frame. `suppressed` returns how many of this kind were not
 *  written since the last one.
 ***************************************************************************/
PRIVATE BOOL peer_log_allowed(
    hgobj gobj,
    peer_log_kind_t kind,
    json_int_t *suppressed
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    peer_log_t *pl = &priv->peer_log[kind];

    if(pl->t_next && !test_msectimer(pl->t_next)) {
        pl->suppressed++;
        return FALSE;
    }
    pl->t_next = start_msectimer(PEER_LOG_INTERVAL_MS);
    *suppressed = pl->suppressed;
    pl->suppressed = 0;
    return TRUE;
}

/***************************************************************************
 *  A json of the peer as a log field: compact, capped to `bfsize`-1 bytes.
 *  Return its whole size.
 ***************************************************************************/
PRIVATE size_t peer_json_dump(json_t *jn, char *bf, size_t bfsize)
{
    /*
     *  json_dumpb() copies the chunks that fit and skips the rest, leaving
     *  what it skipped as it was: the zeros end the string there.
     */
    memset(bf, 0, bfsize);
    return json_dumpb(jn, bf, bfsize-1, JSON_COMPACT|JSON_ENCODE_ANY);
}

/***************************************************************************
 *  The subscriptions of this channel to `event` of `gobj_service` that were
 *  refused (authz, size, cap): the peer still holds them, and its
 *  withdrawal of one is not an error. `add` is +1 or -1; return the count
 *  before it.
 ***************************************************************************/
PRIVATE json_int_t count_refused_subscription(
    hgobj gobj,
    hgobj gobj_service,
    gobj_event_t event,
    int add
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    char key[NAME_MAX];
    snprintf(key, sizeof(key), "%s`%s", gobj_name(gobj_service), event);
    json_int_t refused = json_integer_value(
        json_object_get(priv->refused_subscriptions, key)
    );
    if(refused + add > 0) {
        json_object_set_new(
            priv->refused_subscriptions, key, json_integer(refused + add)
        );
    } else {
        json_object_del(priv->refused_subscriptions, key);
    }
    return refused;
}

/***************************************************************************
 *  The kw of a subscription, built from the (un)subscription a peer sent:
 *  its `__filter__`, and of its `__config__` and `__global__` only the keys
 *  a peer may set. The rest is dropped:
 *    - `__local__`: the keys removed from each event before it is sent.
 *      The one `__local__` of a remote subscription is the reference to
 *      this channel, set by the caller.
 *    - of `__global__`, the keys of the framework (a leading `_`: the
 *      routing `__md_iev__`, the `__md_yuno__` and `__service__` read when
 *      the event is sent back...) and `gbuffer`, the binary field of every
 *      kw (gobj_start_up()): an integer this gate would take for a pointer
 *      when it serializes the event. A key of the peer's own comes back to
 *      the peer in every event of the subscription, and to nobody else
 *      (gobj_publish_event() gives such a subscription a kw of its own).
 *    - any other key of the frame (`__md_iev__` is the routing of the
 *      frame, read by the caller).
 *  Up to 7.25.4 only `__config__` was filtered, and the publish shared ONE
 *  kw with every subscriber: a peer's `__global__` forged, and its
 *  `__local__` stripped, the event of every subscriber after it.
 *
 *  `warn`: the subscription logs what it drops (rate-limited); the
 *  unsubscription repeats the same frame, and is filtered the same way,
 *  quietly.
 *
 *  Return NULL when the `__filter__` or the `__global__` is bigger than
 *  `max_subscription_size` (logged when `warn`, rate-limited).
 ***************************************************************************/
PRIVATE json_t *peer_subscription_kw(
    hgobj gobj,
    json_t *iev_kw,     // not owned
    gobj_event_t event,
    BOOL warn
)
{
    filter_peer_subscription_config(gobj, iev_kw, event, warn);

    json_t *kw_subs = json_object();
    json_t *jn_dropped = json_array();

    const char *key; json_t *jn_value;
    json_object_foreach(iev_kw, key, jn_value) {
        if(strcmp(key, "__config__")==0 || strcmp(key, "__filter__")==0) {
            json_object_set(kw_subs, key, jn_value);

        } else if(strcmp(key, "__global__")==0) {
            if(!json_is_object(jn_value)) {
                if(!json_is_null(jn_value)) {
                    json_array_append_new(jn_dropped, json_string(key));
                }
                continue;
            }
            json_t *jn_global = json_object();
            const char *gkey; json_t *jn_gvalue;
            json_object_foreach(jn_value, gkey, jn_gvalue) {
                if(gkey[0] == '_' || strcmp(gkey, "gbuffer")==0) {
                    json_array_append_new(
                        jn_dropped, json_sprintf("__global__`%s", gkey)
                    );
                } else {
                    json_object_set(jn_global, gkey, jn_gvalue);
                }
            }
            if(json_object_size(jn_global) > 0) {
                json_object_set_new(kw_subs, "__global__", jn_global);
            } else {
                JSON_DECREF(jn_global)
            }

        } else if(strcmp(key, "__md_iev__")==0) {
            // The routing of the frame, the caller reads it

        } else {
            json_array_append_new(jn_dropped, json_string(key));
        }
    }

    json_int_t suppressed = 0;
    if(warn && json_array_size(jn_dropped) > 0 &&
            peer_log_allowed(gobj, PEER_LOG_SUBSCRIPTION_KEYS, &suppressed)) {
        char dump[MAX_LOG_DUMP_SIZE];
        peer_json_dump(jn_dropped, dump, sizeof(dump));
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "SUBSCRIBING keys a peer may not set, ignored",
            "event",        "%s", event,
            "username",     "%s", gobj_read_str_attr(gobj, "__username__"),
            "keys",         "%s", dump,
            "suppressed",   "%ld", (long)suppressed,
            NULL
        );
    }
    JSON_DECREF(jn_dropped)

    json_int_t max_size = gobj_read_integer_attr(gobj, "max_subscription_size");
    if(max_size > 0) {
        const char *fields[] = {"__filter__", "__global__", 0};
        for(int i=0; fields[i]; i++) {
            json_t *jn_field = json_object_get(kw_subs, fields[i]);
            if(!jn_field) {
                continue;
            }
            size_t size = json_dumpb(jn_field, NULL, 0, JSON_COMPACT|JSON_ENCODE_ANY);
            if(size <= (size_t)max_size) {
                continue;
            }
            if(warn && peer_log_allowed(gobj, PEER_LOG_OVERSIZE, &suppressed)) {
                gobj_log_warning(gobj, 0,
                    "function",     "%s", __FUNCTION__,
                    "msgset",       "%s", MSGSET_PROTOCOL,
                    "msg",          "%s", "SUBSCRIBING refused, bigger than max_subscription_size",
                    "event",        "%s", event,
                    "username",     "%s", gobj_read_str_attr(gobj, "__username__"),
                    "field",        "%s", fields[i],
                    "size",         "%lu", (unsigned long)size,
                    "max_subscription_size", "%ld", (long)max_size,
                    "suppressed",   "%ld", (long)suppressed,
                    NULL
                );
            }
            KW_DECREF(kw_subs)
            return NULL;
        }
    }

    return kw_subs;
}

/***************************************************************************
 *  Has the peer room on this channel for one more subscription
 *  (`max_subscriptions`)? A subscription that repeats one it holds replaces
 *  it, and takes no room. The refusal is logged on the transition: the
 *  first one, and not again until the peer is under the cap again.
 ***************************************************************************/
PRIVATE BOOL peer_has_subscription_room(
    hgobj gobj,
    hgobj gobj_service,
    gobj_event_t event,
    json_t *kw_subs     // not owned
)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    json_int_t max_subscriptions = gobj_read_integer_attr(gobj, "max_subscriptions");
    if(max_subscriptions <= 0) {
        return TRUE;
    }

    json_t *kw2match = json_object();
    kw_set_subdict_value(
        gobj,
        kw2match,
        "__local__", "__subscription_reference__",
        json_integer((json_int_t)(uintptr_t)gobj)
    );
    json_t *dl_s = gobj_find_subscribings(gobj, 0, kw2match, 0);
    json_int_t held = (json_int_t)json_array_size(dl_s);
    JSON_DECREF(dl_s)
    if(held < max_subscriptions) {
        priv->subscriptions_capped = FALSE;
        return TRUE;
    }

    json_t *dl_same = gobj_find_subscriptions(gobj_service, event, kw_incref(kw_subs), gobj);
    size_t same = json_array_size(dl_same);
    JSON_DECREF(dl_same)
    if(same > 0) {
        return TRUE;
    }

    if(!priv->subscriptions_capped) {
        priv->subscriptions_capped = TRUE;
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "SUBSCRIBING refused, the peer holds max_subscriptions",
            "event",        "%s", event,
            "username",     "%s", gobj_read_str_attr(gobj, "__username__"),
            "max_subscriptions", "%ld", (long)max_subscriptions,
            NULL
        );
    }
    return FALSE;
}

/***************************************************************************
 *  The subscriptions of this channel that an unsubscription of the peer
 *  withdraws: `event` of `gobj_service` with its `__config__` and
 *  `__filter__`, and the `__global__` the PEER sent. The stored
 *  `__global__` carries the back-metadata this gate adds to it
 *  (`__md_iev__`, `__md_yuno__`, keys a peer may not set), which the peer
 *  never repeats: up to 7.25.4 it was compared whole, and a subscription
 *  with a `__global__` could not be withdrawn until the channel closed.
 ***************************************************************************/
PRIVATE json_t *find_peer_subscriptions(
    hgobj gobj,
    hgobj gobj_service,
    gobj_event_t event,
    json_t *kw_subs     // not owned
)
{
    json_t *kw_find = json_object();
    json_t *jn_config = json_object_get(kw_subs, "__config__");
    if(jn_config) {
        json_object_set(kw_find, "__config__", jn_config);
    }
    json_t *jn_filter = json_object_get(kw_subs, "__filter__");
    if(jn_filter) {
        json_object_set(kw_find, "__filter__", jn_filter);
    }
    json_t *dl_subs = gobj_find_subscriptions(gobj_service, event, kw_find, gobj);

    json_t *peer_global = json_object_get(kw_subs, "__global__");
    json_t *jn_empty = json_object();
    if(!peer_global) {
        peer_global = jn_empty;
    }

    json_t *dl_matched = json_array();
    size_t idx; json_t *subs;
    json_array_foreach(dl_subs, idx, subs) {
        json_t *stored_global = kw_get_dict(gobj, subs, "__global__", 0, 0);
        json_t *stored_peer_global = json_object();
        const char *key; json_t *jn_value;
        json_object_foreach(stored_global, key, jn_value) {
            if(key[0] != '_') {
                json_object_set(stored_peer_global, key, jn_value);
            }
        }
        if(json_equal(stored_peer_global, peer_global)) {
            json_array_append(dl_matched, subs);
        }
        JSON_DECREF(stored_peer_global)
    }

    JSON_DECREF(jn_empty)
    JSON_DECREF(dl_subs)
    return dl_matched;
}

/***************************************************************************
 *  Reject a per-message ievent that cannot be routed (unauthorized service,
 *  or service not found) WITHOUT leaving the channel in a zombie state.
 *
 *  The socket read is only re-armed by c_tcp when the EV_RX_DATA publish
 *  chain returns 0 (see c_tcp.c: "If it's in idle then re-arm", gated on
 *  ret==0). A bare `return -1` here both skips the answer AND leaves the read
 *  un-rearmed: the channel stays connected but deaf, and the peer waits
 *  forever. So we never return -1 silently:
 *    - command / stats have a natural answer channel: reply with a negative
 *      EV_MT_*_ANSWER and return 0 (channel stays alive, read re-arms).
 *    - subscribe / unsubscribe / inject have no answer: drop() the channel so
 *      the peer sees a clean disconnect (the read is intentionally not
 *      re-armed because drop() is closing it).
 *  Consumes iev_kw and kw.
 ***************************************************************************/
PRIVATE int reject_unrouted_iev(
    hgobj gobj,
    gobj_event_t iev_event,
    json_t *iev_kw,     // owned
    json_t *kw,         // owned
    const char *comment,
    hgobj src
)
{
    gobj_event_t answer_event = 0;
    if(iev_event == EV_MT_COMMAND) {
        answer_event = EV_MT_COMMAND_ANSWER;
    } else if(iev_event == EV_MT_STATS) {
        answer_event = EV_MT_STATS_ANSWER;
    }

    if(answer_event) {
        json_t *kw_response = build_command_response(
            gobj,
            -1,                             // result
            json_sprintf("%s", comment),    // jn_comment
            0,                              // jn_schema
            0                               // jn_data
        );
        kw_response = msg_iev_set_back_metadata(
            gobj,
            iev_kw,         // owned, consumed (only __md_iev__ is used)
            kw_response,    // like owned, is returned
            TRUE            // reverse_dst
        );
        send_static_iev(gobj, answer_event, kw_response, src);
        KW_DECREF(kw)
        return 0;   // channel stays alive: c_tcp re-arms the read only on ret==0
    }

    /*
     *  No natural answer channel (subscribe/unsubscribe/inject): close the
     *  channel so the peer sees a clean disconnect instead of a silent stall.
     */
    drop(gobj);
    KW_DECREF(iev_kw)
    KW_DECREF(kw)
    return -1;
}

/***************************************************************************
 *
TODO now the tube is C_IEVENT_SRV -> C_CHANNEL -> C_WEBSOCKET -> C_TCP
     Wouldn't be more logical to be channel->ievent_srv?
     Review too EV_ON_ID,EV_ON_ID_NAK when refactoring authz

 ***************************************************************************/
PRIVATE int ac_on_message(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);
    gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw, "gbuffer", 0, FALSE);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "gbuffer NULL, expected gbuf with inter-event",
            "peername",     "%s", gobj_has_bottom_attr(gobj, "peername")?gobj_read_str_attr(gobj, "peername"):"",
            "sockname",     "%s", gobj_has_bottom_attr(gobj, "sockname")?gobj_read_str_attr(gobj, "sockname"):"",
            NULL
        );
        drop(gobj);
        KW_DECREF(kw)
        return -1;
    }

    /*---------------------------------------*
     *  Create inter_event from gbuf
     *---------------------------------------*/
    gbuffer_incref(gbuf);

    gobj_event_t iev_event;
    json_t *iev_kw = iev_create_from_gbuffer(gobj, &iev_event, gbuf, 0);
    if(!iev_kw) {
        gobj_log_warning(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "Bad json",
            "peername",     "%s", gobj_has_bottom_attr(gobj, "peername")?gobj_read_str_attr(gobj, "peername"):"",
            "sockname",     "%s", gobj_has_bottom_attr(gobj, "sockname")?gobj_read_str_attr(gobj, "sockname"):"",
            NULL
        );
        gobj_trace_dump(gobj,
            gbuffer_head_pointer(gbuf),
            MIN(gbuffer_totalbytes(gbuf), MAX_LOG_DUMP_SIZE),
            "Bad json"
        );
        drop(gobj);
        KW_DECREF(kw)
        return -1;
    }
    if(empty_string(iev_event)) {
        // Error already logged
        gobj_trace_json(gobj, iev_kw, "iev_event NULL");
        drop(gobj);
        KW_DECREF(iev_kw)
        KW_DECREF(kw)
        return -1;
    }

    /*---------------------------------------*
     *          trace inter_event
     *---------------------------------------*/
    uint32_t trace_level = gobj_trace_level(gobj);
    char prefix[256];
    if(trace_level) {
        snprintf(prefix, sizeof(prefix),
            "INTRA-EVENT(%s^%s) %s <== %s",
            gobj_yuno_role(),
            gobj_yuno_name(),
            gobj_short_name(gobj),
            gobj_short_name(src)
        );
        if((trace_level & TRACE_IEVENTS2)) {
            trace_inter_event2(gobj, prefix, iev_event, iev_kw);
        } else if((trace_level & TRACE_IEVENTS)) {
            trace_inter_event(gobj, prefix, iev_event, iev_kw);
        } else if((trace_level & TRACE_IDENTITY_CARD)) {
            if(iev_event == EV_IDENTITY_CARD ||
                iev_event == EV_IDENTITY_CARD_ACK ||
                iev_event == EV_PLAY_YUNO ||
                iev_event == EV_PLAY_YUNO_ACK ||
                iev_event == EV_PAUSE_YUNO ||
                iev_event == EV_PAUSE_YUNO_ACK
               ) {
                trace_inter_event2(gobj, prefix, iev_event, iev_kw);
            }
        }
    }

    /*-----------------------------------------*
     *  If state is not SESSION send self.
     *  Mainly process EV_IDENTITY_CARD_ACK
     *-----------------------------------------*/
    if(gobj_current_state(gobj) != ST_SESSION) {
        int ret = -1;
        if(gobj_has_event(gobj, iev_event, EVF_PUBLIC_EVENT)) {
            kw_incref(iev_kw);
            ret = gobj_send_event(gobj, iev_event, iev_kw, gobj);
            if(ret==0) {
                KW_DECREF(iev_kw)
                KW_DECREF(kw)
                return 0;
            }
        }
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "event UNKNOWN in not-session state",
            "event",        "%s", iev_event,
            NULL
        );
        trace_inter_event(gobj, prefix, iev_event, iev_kw);
        drop(gobj);
        KW_DECREF(iev_kw)
        KW_DECREF(kw)
        return ret;
    }

    /*------------------------------------*
     *   Analyze inter_event
     *------------------------------------*/
    const char *msg_type = msg_iev_get_msg_type(gobj, iev_kw);

    /*-----------------------------------------------------------*
     *  Get inter-event routing information.
     *  Version > 2.4.0
     *  Changed msg_iev_get_stack(, , TRUE->FALSE)
     *  because the yuno can autonomously report
     *  a play->pause change, and then it comes without a stack,
     *  because it is not a request that comes from the agent.
     *-----------------------------------------------------------*/
    json_t *jn_ievent_id = msg_iev_get_stack(gobj, iev_kw, IEVENT_STACK_ID, TRUE); // TODO check

    /*----------------------------------------*
     *  Check dst role^name
     *----------------------------------------*/
    const char *iev_dst_role = kw_get_str(gobj, jn_ievent_id, "dst_role", "", 0);
    if(!empty_string(iev_dst_role)) {
        if(strcasecmp(iev_dst_role, gobj_yuno_role())!=0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "It's not my role",
                "yuno_role",    "%s", iev_dst_role,
                "my_role",      "%s", gobj_yuno_role(),
                NULL
            );
            trace_inter_event(gobj, prefix, iev_event, iev_kw);
            drop(gobj);
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return -1;
        }
    }
    const char *iev_dst_yuno = kw_get_str(gobj, jn_ievent_id, "dst_yuno", "", 0);
    if(!empty_string(iev_dst_yuno)) {
        if(strcasecmp(iev_dst_yuno, gobj_yuno_name())!=0) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "It's not my name",
                "yuno_name",    "%s", iev_dst_yuno,
                "my_name",      "%s", gobj_yuno_name(),
                NULL
            );
            trace_inter_event(gobj, prefix, iev_event, iev_kw);
            drop(gobj);
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return -1;
        }
    }

    /*----------------------------------------*
     *  Check dst service
     *----------------------------------------*/
    const char *iev_dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
    hgobj gobj_service = gobj_find_service(iev_dst_service, FALSE);
    if(!gobj_service) {
        gobj_service = priv->gobj_service;
    }
    /*
     *  AUTHZ: the per-message dst_service must be one this channel is
     *  authorized to reach (the primary authenticated service, or one granted
     *  via services_roles at identity_card time). Authentication is per-service
     *  (see c_authz.c mt_authenticate), so a peer must not be able to route
     *  subscribe / unsubscribe / inject at an unauthorized service B by naming
     *  it in the attacker-controlled routing stack.
     */
    if(!is_service_authorized(gobj, gobj_service)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "event ignored, dst_service not authorized for this channel",
            "service",      "%s", iev_dst_service,
            "this_service", "%s", priv->this_service?priv->this_service:"",
            "event",        "%s", iev_event,
            NULL
        );
        gobj_trace_json(gobj, iev_kw, "event ignored, dst_service not authorized for this channel");
        json_t *jn_authorized = gobj_read_json_attr(gobj, "authorized_services"); // not mine
        gobj_trace_json(gobj, jn_authorized, "authorized_services");
        trace_inter_event(gobj, prefix, iev_event, iev_kw);
        char comment[120];
        snprintf(comment, sizeof(comment),
            "Service not authorized for this channel: '%s'", iev_dst_service);
        return reject_unrouted_iev(gobj, iev_event, iev_kw, kw, comment, src);
    }
    if(!gobj_service) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PARAMETER,
            "msg",          "%s", "event ignored, service not found",
            "service",      "%s", iev_dst_service,
            "event",        "%s", iev_event,
            NULL
        );
        trace_inter_event(gobj, prefix, iev_event, iev_kw);
        char comment[120];
        snprintf(comment, sizeof(comment), "Service not found: '%s'", iev_dst_service);
        return reject_unrouted_iev(gobj, iev_event, iev_kw, kw, comment, src);
    }

    /*------------------------------------*
     *   Set channel info
     *------------------------------------*/
    /*
     *  The nearest SERVICE ancestor, and not the immediate parent.
     *
     *  `input_service` is read back to route a DEFERRED answer, with
     *  gobj_find_service(), so it has to name a service. The immediate
     *  parent is one in the agent's topology, and it is the CHANNEL when a
     *  browser connects straight to this yuno through an iogate
     *  (C_IOGATE^__top_side__ -> C_CHANNEL^tcps-N -> C_IEVENT_SRV^tcps-N).
     *  There the answer was looked up under a name that is not a service,
     *  found nothing, and was dropped -- which is why register-idp-user
     *  never answered a SPA, while the same command through the agent did.
     */
    hgobj input_service_gobj = gobj_parent(gobj);
    while(input_service_gobj && !gobj_is_service(input_service_gobj)) {
        input_service_gobj = gobj_parent(input_service_gobj);
    }
    if(!input_service_gobj) {
        input_service_gobj = gobj_parent(gobj);
    }
    json_object_set_new(
        jn_ievent_id,
        "input_service",
        json_string(gobj_name(input_service_gobj))
    );
    json_object_set_new(
        jn_ievent_id,
        "input_channel",
        json_string(gobj_name(gobj))
    );
    json_object_set_new(
        jn_ievent_id,
        "__username__",
        json_string(gobj_read_str_attr(gobj, "__username__"))
    );

    /*------------------------------------*
     *   Dispatch event
     *------------------------------------*/
    if(strcasecmp(msg_type, "__subscribing__")==0) {
        /*-----------------------------------*
         *  It's a external subscription
         *-----------------------------------*/

        /*
         *   Protect: only public events
         */
         if(!gobj_has_output_event(gobj_service, iev_event, EVF_PUBLIC_EVENT)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "SUBSCRIBING event ignored, not PUBLIC or PUBLIC event",
                "service",      "%s", iev_dst_service,
                "gobj_service", "%s", gobj_short_name(gobj_service),
                "event",        "%s", iev_event,
                NULL
            );
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return -1;
        }

        /*-------------------------------------------------*
         *  What a peer may ask
         *-------------------------------------------------*/
        json_t *kw_subs = peer_subscription_kw(gobj, iev_kw, iev_event, TRUE);
        if(!kw_subs) {
            // Error already logged
            count_refused_subscription(gobj, gobj_service, iev_event, +1);
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return 0;   // the channel stays open: only this subscription is refused
        }

        /*-------------------------------------------------*
         *  Check AUTHZ
         *-------------------------------------------------*/
        char key[NAME_MAX];
        snprintf(key, sizeof(key), "%s`%s", gobj_name(gobj_service), iev_event);
        BOOL first_refusal = json_integer_value(
            json_object_get(priv->refused_subscriptions, key)
        ) == 0;
        if(!is_subscription_authorized(gobj, gobj_service, iev_event, kw_subs, first_refusal)) {
            // Error already logged (the first refusal of this event)
            count_refused_subscription(gobj, gobj_service, iev_event, +1);
            KW_DECREF(kw_subs)
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return 0;   // the channel stays open: only this subscription is refused
        }

        // Set locals to remove on publishing
        kw_set_subdict_value(
            gobj,
            kw_subs,
            "__local__", "__subscription_reference__",
            json_integer((json_int_t)(uintptr_t)gobj)
        );

        // Prepare the return of response
        json_t *__md_iev__ = kw_get_dict(gobj, iev_kw, "__md_iev__", 0, 0);
        if(__md_iev__) {
            json_t *kw3 = msg_iev_set_back_metadata(
                gobj,
                kw_incref(iev_kw),
                0,
                TRUE
            );
            json_t *__global__ = kw_get_dict(gobj, kw_subs, "__global__", 0, 0);
            if(__global__) {
                json_object_update_new(__global__, kw3);
            } else {
                json_object_set_new(kw_subs, "__global__", kw3);
            }
        }

        /*-------------------------------------------------*
         *  Room for it
         *-------------------------------------------------*/
        if(!peer_has_subscription_room(gobj, gobj_service, iev_event, kw_subs)) {
            // Error already logged (on the transition)
            count_refused_subscription(gobj, gobj_service, iev_event, +1);
            KW_DECREF(kw_subs)
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return 0;   // the channel stays open: only this subscription is refused
        }

        gobj_subscribe_event(gobj_service, iev_event, kw_subs, gobj);
        KW_DECREF(iev_kw)

    } else if(strcasecmp(msg_type, "__unsubscribing__")==0) {

        /*-----------------------------------*
         *  It's a external unsubscription
         *-----------------------------------*/
        /*
         *  No AUTHZ: it removes only this channel's own subscriptions (the
         *  subscriber is this gobj), which the subscribe already authorized.
         */

        /*
         *   Protect: only public events
         */
        if(!gobj_has_output_event(gobj_service, iev_event, EVF_PUBLIC_EVENT)) {
            gobj_log_error(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "UNSUBSCRIBING event ignored, not PUBLIC or PUBLIC event",
                "service",      "%s", iev_dst_service,
                "gobj_service", "%s", gobj_short_name(gobj_service),
                "event",        "%s", iev_event,
                NULL
            );
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return -1;
        }
        /*
         *  Filtered the same way as the subscription, so what the peer
         *  repeats is compared with what was stored.
         */
        json_t *kw_subs = peer_subscription_kw(gobj, iev_kw, iev_event, FALSE);
        json_t *dl_subs = kw_subs?
            find_peer_subscriptions(gobj, gobj_service, iev_event, kw_subs) :
            json_array();   // refused at the subscription, over the size cap
        KW_DECREF(kw_subs)

        if(json_array_size(dl_subs) == 0) {
            JSON_DECREF(dl_subs)
            /*
             *  Nothing to remove. A subscription that was refused (authz,
             *  size, cap) was never made, but the peer still holds it and
             *  withdraws it: not an error, the refusal was logged when it
             *  was asked. Anything else is a withdrawal of something this
             *  channel never had. Both are the peer's to repeat: logged at
             *  most once per PEER_LOG_INTERVAL_MS, and the kw capped.
             */
            json_int_t suppressed = 0;
            json_int_t refused = count_refused_subscription(
                gobj, gobj_service, iev_event, -1
            );
            if(refused > 0) {
                if(peer_log_allowed(gobj, PEER_LOG_REFUSED_WITHDRAWN, &suppressed)) {
                    gobj_log_info(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_AUTH,
                        "msg",          "%s", "UNSUBSCRIBING event never subscribed, its subscription was refused",
                        "service",      "%s", iev_dst_service,
                        "event",        "%s", iev_event,
                        "username",     "%s", gobj_read_str_attr(gobj, "__username__"),
                        "suppressed",   "%ld", (long)suppressed,
                        NULL
                    );
                }
            } else {
                if(peer_log_allowed(gobj, PEER_LOG_NO_MATCH, &suppressed)) {
                    char dump[MAX_LOG_DUMP_SIZE];
                    size_t size = peer_json_dump(iev_kw, dump, sizeof(dump));
                    gobj_log_warning(gobj, 0,
                        "function",     "%s", __FUNCTION__,
                        "msgset",       "%s", MSGSET_PROTOCOL,
                        "msg",          "%s", "UNSUBSCRIBING event matches no subscription of this channel",
                        "service",      "%s", iev_dst_service,
                        "event",        "%s", iev_event,
                        "username",     "%s", gobj_read_str_attr(gobj, "__username__"),
                        "kw",           "%s", dump,
                        "kw_size",      "%lu", (unsigned long)size,
                        "suppressed",   "%ld", (long)suppressed,
                        NULL
                    );
                }
            }
            KW_DECREF(iev_kw)
            KW_DECREF(kw)
            return 0;
        }

        gobj_unsubscribe_list(gobj_service, dl_subs, FALSE);
        KW_DECREF(iev_kw)

    } else {
        /*----------------------*
         *      Inject event
         *----------------------*/
        /*-------------------------------------------------------*
         *  Filter public events of this gobj
         *-------------------------------------------------------*/

        /*----------------------------------*
         *  Check AUTHZ
         *----------------------------------*/
//         if(ev_desc->authz & EV_AUTHZ_INJECT) {
//             /*
//             *  AUTHZ Required
//             */
//             json_t *kw_authz = json_pack("{s:s}",
//                 "event", event
//             );
//             if(kw) {
//                 json_object_set(kw_authz, "kw", kw);
//             } else {
//                 json_object_set_new(kw_authz, "kw", json_object());
//             }
//             if(!gobj_user_has_authz(
//                     dst, "__inject_event__", kw_authz, src)
//             ) {
//                 gobj_log_error(gobj, 0,
//                     "gobj",         "%s", gobj_full_name(dst),
//                     "function",     "%s", __FUNCTION__,
//                     "msgset",       "%s", MSGSET_AUTH,
//                     "msg",          "%s", "No permission to inject event",
//                     //"user",         "%s", gobj_get_user(src),
//                     "gclass",       "%s", gobj_gclass_name(dst),
//                     "event",        "%s", event?event:"",
//                     NULL
//                 );
//                 KW_DECREF(kw)
//                 return -403;
//             }
//         }

        /*
         *   Set __username__
         */
        kw_set_dict_value(
            gobj,
            iev_kw,
            "__username__",
            json_string(gobj_read_str_attr(gobj, "__username__"))
        );

        if(gobj_has_event(gobj, iev_event, EVF_PUBLIC_EVENT)) {
            /*
            *  It's mine (I manage inter-command and inter-stats)
            */
            int ret = gobj_send_event(
                gobj,
                iev_event,
                iev_kw,
                gobj
            );
            KW_DECREF(kw)
            return ret;
        }

        /*
         *   Send inter-event to subscriber
         */
        json_t *jn_iev = iev_create( // For use within Yuno
            gobj,
            iev_event,
            iev_kw // owned
        );

        gobj_publish_event(gobj, EV_ON_IEV_MESSAGE, jn_iev);
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  remote ask for stats
 ***************************************************************************/
PRIVATE int ac_mt_stats(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *      __RESPONSE__ __MESSAGE__
     */

    /*----------------------------------*
     *  Check AUTHZ
     *----------------------------------*/
    if(!gobj_read_bool_attr(gobj, "authenticated")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Only authenticated users can request stats",
            NULL
        );
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("Only authenticated users can request stats"),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        kw_response = msg_iev_set_back_metadata(
            gobj,
            kw,             // owned, kw request, used to extract ONLY __md_iev__
            kw_response,    // like owned, is returned!, created if null, the body of answer message
            TRUE            // reverse_dst
        );

        return send_static_iev(gobj,
            EV_MT_STATS_ANSWER,
            kw_response,
            src
        );
    }

    /*------------------------------------*
     *   Analyze inter_event
     *------------------------------------*/
    /*-----------------------------------------------------------*
     *  Get inter-event routing information.
     *-----------------------------------------------------------*/
    // json_t *jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, FALSE);

    /*----------------------------------------*
     *  Check dst role^name
     *----------------------------------------*/
    /*----------------------------------------*
     *  Check dst service
     *----------------------------------------*/
    const char *service = kw_get_str(gobj, kw, "service", "", 0);

    hgobj gobj_service;
    if(empty_string(service)) {
        gobj_service = priv->gobj_service;
    } else {
        gobj_service = gobj_find_service(service, FALSE);
        if(!gobj_service) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Service not found",
                "service",      "%s", service,
                "event",        "%s", event,
                NULL
            );
            json_t *kw_response = build_command_response(
                gobj,
                -1,     // result
                json_sprintf("Service not found: '%s'", service),   // jn_comment
                0,      // jn_schema
                0       // jn_data
            );
            kw_response = msg_iev_set_back_metadata(
                gobj,
                kw,             // owned, kw request, used to extract ONLY __md_iev__
                kw_response,    // like owned, is returned!, created if null, the body of answer message
                TRUE            // reverse_dst
            );

            return send_static_iev(gobj,
                EV_MT_STATS_ANSWER,
                kw_response,
                src
            );
        }
    }

    /*----------------------------------------*
     *  Check AUTHZ: cross-service
     *  The channel may only read/reset stats of a service it is authorized to
     *  reach (the primary authenticated service, or one granted via
     *  services_roles at identity_card time). A named "service" outside that
     *  set is denied. The empty-service fallback above already points at
     *  priv->gobj_service, so it passes.
     *----------------------------------------*/
    if(!is_service_authorized(gobj, gobj_service)) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_AUTH,
            "msg",          "%s", "Not authorized to request stats of a different service",
            "service",      "%s", service,
            "event",        "%s", event,
            NULL
        );
        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("Not authorized to request stats of service: '%s'", service),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        kw_response = msg_iev_set_back_metadata(
            gobj,
            kw,             // owned, kw request, used to extract ONLY __md_iev__
            kw_response,    // like owned, is returned!, created if null, the body of answer message
            TRUE            // reverse_dst
        );

        return send_static_iev(gobj,
            EV_MT_STATS_ANSWER,
            kw_response,
            src
        );
    }

    /*------------------------------------*
     *   Dispatch stats
     *------------------------------------*/
    const char *stats = kw_get_str(gobj, kw, "__stats__", 0, 0); // v6
    if(!stats) {
        // v7
        json_t *__stats__ = msg_iev_get_stack(gobj, kw, "__stats__", TRUE);
        stats = kw_get_str(gobj, __stats__, "stats", "", KW_REQUIRED);
    }

    kw_set_dict_value(
        gobj,
        kw,
        "__username__",
        json_string(gobj_read_str_attr(gobj, "__username__"))
    );

    KW_INCREF(kw)
    json_t *kw_response = gobj_stats(
        gobj_service,
        stats,
        kw,
        src
    );
    if(!kw_response) {
        // Asynchronous response
    } else {
        json_t *kw2 = msg_iev_set_back_metadata(
            gobj,
            kw,
            kw_response,
            TRUE
        );

        return send_static_iev(gobj,
            EV_MT_STATS_ANSWER,
            kw2,
            src
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  remote ask for command
 ***************************************************************************/
PRIVATE int ac_mt_command(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    /*
     *      __RESPONSE__ __MESSAGE__
     */

    /*----------------------------------*
     *  Check AUTHZ
     *----------------------------------*/
    if(!gobj_read_bool_attr(gobj, "authenticated")) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_INTERNAL,
            "msg",          "%s", "Only authenticated users can request commands",
            NULL
        );

        json_t *kw_response = build_command_response(
            gobj,
            -1,     // result
            json_sprintf("Only authenticated users can request commands"),   // jn_comment
            0,      // jn_schema
            0       // jn_data
        );
        kw_response = msg_iev_set_back_metadata(
            gobj,
            kw,             // owned, kw request, used to extract ONLY __md_iev__
            kw_response,    // like owned, is returned!, created if null, the body of answer message
            TRUE            // reverse_dst
        );

        return send_static_iev(gobj,
            EV_MT_COMMAND_ANSWER,
            kw_response,
            src
        );
    }

    /*------------------------------------*
     *   Analyze inter_event
     *------------------------------------*/
    /*-----------------------------------------------------------*
     *  Get inter-event routing information.
     *-----------------------------------------------------------*/
    // json_t *jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, FALSE);

    /*----------------------------------------*
     *  Check dst role^name
     *----------------------------------------*/
    /*----------------------------------------*
     *  Check dst service
     *----------------------------------------*/
    const char *service = kw_get_str(gobj, kw, "service", "", 0);

    hgobj gobj_service;
    if(empty_string(service)) {
        gobj_service = priv->gobj_service;
    } else {
        gobj_service = gobj_find_service(service, FALSE);
        if(!gobj_service) {
            gobj_log_warning(gobj, 0,
                "function",     "%s", __FUNCTION__,
                "msgset",       "%s", MSGSET_PARAMETER,
                "msg",          "%s", "Service not found",
                "service",      "%s", service,
                "event",        "%s", event,
                NULL
            );
            json_t *kw_response = build_command_response(
                gobj,
                -1,     // result
                json_sprintf("Service not found: '%s'", service),   // jn_comment
                0,      // jn_schema
                0       // jn_data
            );
            kw_response = msg_iev_set_back_metadata(
                gobj,
                kw,             // owned, kw request, used to extract ONLY __md_iev__
                kw_response,    // like owned, is returned!, created if null, the body of answer message
                TRUE            // reverse_dst
            );

            return send_static_iev(gobj,
                EV_MT_COMMAND_ANSWER,
                kw_response,
                src
            );
        }
    }


    /*------------------------------------*
     *   Dispatch command
     *------------------------------------*/
    const char *command = kw_get_str(gobj, kw, "__command__", 0, 0); // v6
    if(!command) {
        // v7
        json_t *__command__ = msg_iev_get_stack(gobj, kw, "__command__", TRUE);
        command = kw_get_str(gobj, __command__, "command", "", KW_REQUIRED);
    }

    kw_set_dict_value(
        gobj,
        kw,
        "__username__",
        json_string(gobj_read_str_attr(gobj, "__username__"))
    );

    KW_INCREF(kw)
    json_t *kw_response = gobj_command(
        gobj_service,
        command,
        kw,
        src
    );
    if(!kw_response) {
        // Asynchronous response
    } else {
        json_t *kw2 = msg_iev_set_back_metadata(
            gobj,
            kw,
            kw_response,
            TRUE
        );

        return send_static_iev(gobj,
            EV_MT_COMMAND_ANSWER,
            kw2,
            src
        );
    }

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  remote ask
 *  Somebody wants exit
 ***************************************************************************/
PRIVATE int ac_goodbye(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    const char *cause = kw_get_str(gobj, kw, "cause", "", 0);

    uint32_t trace_level = gobj_trace_level(gobj);
    if((trace_level & TRACE_IDENTITY_CARD)) {
        char prefix[256];
        snprintf(prefix, sizeof(prefix),
            "INTRA-EVENT(%s^%s) %s <== %s (cause: %s)",
            gobj_yuno_role(),
            gobj_yuno_name(),
            gobj_short_name(gobj),
            gobj_short_name(src),
            cause
        );
        trace_inter_event2(gobj, prefix, event, kw);
    }
    drop(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *  For asynchronous responses
 ***************************************************************************/
PRIVATE int ac_mt_command_answer(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *      __RESPONSE__ __MESSAGE__
     */
    return send_static_iev(gobj,
        EV_MT_COMMAND_ANSWER,
        kw,
        src
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_drop(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    drop(gobj);

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_remote_log(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    const char *msg = kw_get_str(gobj, kw, "msg", "", 0);
    gobj_log_error(gobj, 0,
        "function",     "%s", __FUNCTION__,
        "msgset",       "%s", MSGSET_APP,
        "msg",          "%s", "Remote Error",
        "remote msg",   "%s", msg,
        NULL
    );
    gobj_trace_json(gobj, kw, "Remote Error");

    KW_DECREF(kw)
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int ac_stopped(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src)
{
    /*
     *  someone or ... has stopped
     */
    KW_DECREF(kw)
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
    .mt_writing = mt_writing,
    .mt_destroy = mt_destroy,
    .mt_start = mt_start,
    .mt_stop = mt_stop,
    .mt_stats = mt_stats,
    .mt_command_parser = mt_command,
    .mt_inject_event = mt_inject_event,
};

/*------------------------*
 *      GClass name
 *------------------------*/
GOBJ_DEFINE_GCLASS(C_IEVENT_SRV);

/*------------------------*
 *      States
 *------------------------*/
GOBJ_DEFINE_STATE(ST_WAIT_IDENTITY_CARD);

/*------------------------*
 *      Events
 *------------------------*/
GOBJ_DEFINE_EVENT(EV_GOODBYE);


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
    ev_action_t st_disconnected[] = {
        {EV_ON_OPEN,            ac_on_open,             ST_WAIT_IDENTITY_CARD},
        {EV_STOPPED,            ac_stopped,             0},
        {0,0,0}
    };
    ev_action_t st_wait_identity_card[] = {
        {EV_ON_MESSAGE,         ac_on_message,          0},
        {EV_IDENTITY_CARD,      ac_identity_card,       0},
        {EV_GOODBYE,            ac_goodbye,             0},
        {EV_ON_CLOSE,           ac_on_close,            ST_DISCONNECTED},
        {EV_DROP,               ac_drop,                0},
        {EV_TIMEOUT,            ac_timeout_wait_idGot,  0},
        {0,0,0}
    };
    ev_action_t st_session[] = {
        {EV_ON_MESSAGE,         ac_on_message,          0},
        {EV_MT_COMMAND_ANSWER,  ac_mt_command_answer,   0},
        {EV_MT_COMMAND,         ac_mt_command,          0},
        {EV_MT_STATS,           ac_mt_stats,            0},
        {EV_IDENTITY_CARD,      ac_identity_card,       0},
        {EV_GOODBYE,            ac_goodbye,             0},
        {EV_ON_CLOSE,           ac_on_close,            ST_DISCONNECTED},
        {EV_DROP,               ac_drop,                0},
        {EV_REMOTE_LOG,         ac_remote_log,          0},
        {EV_STOPPED,            ac_stopped,             0}, // puede llegar por aquí en un gobj_stop_tree()
        {0,0,0}
    };

    states_t states[] = {
        {ST_DISCONNECTED,           st_disconnected},
        {ST_WAIT_IDENTITY_CARD,     st_wait_identity_card},
        {ST_SESSION,                st_session},
        {0, 0}
    };

    event_type_t event_types[] = {
        {EV_ON_MESSAGE,             0},
        {EV_ON_IEV_MESSAGE,         EVF_OUTPUT_EVENT},
        {EV_MT_COMMAND_ANSWER,      0},
        {EV_MT_COMMAND,             EVF_PUBLIC_EVENT},
        {EV_MT_STATS,               EVF_PUBLIC_EVENT},
        {EV_ON_OPEN,                EVF_OUTPUT_EVENT}, // |EVF_NO_WARN_SUBS
        {EV_ON_CLOSE,               EVF_OUTPUT_EVENT}, // |EVF_NO_WARN_SUBS
        {EV_IDENTITY_CARD,          EVF_PUBLIC_EVENT},
        {EV_GOODBYE,                EVF_PUBLIC_EVENT},
        {EV_REMOTE_LOG,             EVF_PUBLIC_EVENT},
        {EV_DROP,                   0},
        // internal
        {EV_TIMEOUT,            0},
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
        0,  // lmt,
        attrs_table,
        sizeof(PRIVATE_DATA),
        0,  // authz_table,
        0,  // command_table,
        s_user_trace_level,
        gcflag_no_check_output_events   // gcflag_t
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
PUBLIC int register_c_ievent_srv(void)
{
    return create_gclass(C_IEVENT_SRV);
}
