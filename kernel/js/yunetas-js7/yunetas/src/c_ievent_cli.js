/**************************************************************************
 *          c_ievent_cli.js
 *          Ievent_cli GClass.
 *
 *          Inter-event (client side)
 *          Simulate a remote service like a local gobj.
 *
 *      Licence: MIT (http://www.opensource.org/licenses/mit-license)
 *      Copyright (c) 2014,2024 Niyamaka.
 *      Copyright (c) 2025, ArtGins.
 **************************************************************************/
import {
    YUNETA_VERSION,
    SDATA,
    SDATA_END,
    data_type_t,
    gclass_flag_t,
    sdata_flag_t,
    event_flag_t,
    gclass_create,
    gobj_parent,
    gobj_short_name,
    gobj_yuno,
    gobj_send_event,
    gobj_publish_event,
    gobj_read_integer_attr,
    gobj_read_str_attr,
    gobj_read_pointer_attr,
    gobj_write_str_attr,
    gobj_subscribe_event,
    gobj_is_pure_child,
    gobj_create_pure_child,
    gobj_name,
    gobj_current_state,
    gobj_yuno_name,
    gobj_yuno_role,
    gobj_find_subscriptions,
    gobj_is_running,
    gobj_is_playing,
    gobj_read_attr,
    gobj_yuno_id,
    gobj_has_event,
    gobj_find_service,
    gobj_change_state,
    gobj_start,
    gobj_stop,
    gobj_read_bool_attr,

} from "./gobj.js";

import {
    log_error,
    log_debug,
    trace_msg,
    log_warning,
    current_timestamp,
    empty_string,
    msg_iev_push_stack,
    msg_iev_get_stack,
    kw_get_str,
    json_object_del,
    kw_get_int,
    kw_get_dict_value,
    node_uuid,
    msg_iev_set_msg_type,
    msg_iev_get_msg_type,
    trace_json,
} from "./helpers.js";

import {
    clear_timeout,
    set_timeout
} from "./c_timer.js";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_IEVENT_CLI";

const IEVENT_MESSAGE_AREA_ID = "ievent_gate_stack";

/***************************************************************
 *              Data
 ***************************************************************/
/*---------------------------------------------*
 *          Attributes
 *---------------------------------------------*/
const attrs_table = [
/*-ATTR-type--------------------name----------------flag------------------------default-----description---------- */
SDATA (data_type_t.DTP_STRING,  "wanted_yuno_role", sdata_flag_t.SDF_RD,        "",         "wanted yuno role"),
SDATA (data_type_t.DTP_STRING,  "wanted_yuno_name", sdata_flag_t.SDF_RD,        "",         "wanted yuno name"),
SDATA (data_type_t.DTP_STRING,  "wanted_yuno_service",sdata_flag_t.SDF_RD,      "",         "wanted yuno service"),
SDATA (data_type_t.DTP_STRING,  "remote_yuno_role", sdata_flag_t.SDF_RD,        "",         "confirmed remote yuno role"),
SDATA (data_type_t.DTP_STRING,  "remote_yuno_name", sdata_flag_t.SDF_RD,        "",         "confirmed remote yuno name"),
SDATA (data_type_t.DTP_STRING,  "remote_yuno_service",sdata_flag_t.SDF_RD,      "",         "confirmed remote yuno service"),
SDATA (data_type_t.DTP_STRING,  "url",              sdata_flag_t.SDF_RD,        "",         "Url to connect"),
SDATA (data_type_t.DTP_STRING,  "jwt",              sdata_flag_t.SDF_PERSIST,   "",         "JWT"),
SDATA (data_type_t.DTP_STRING,  "cert_pem",         sdata_flag_t.SDF_PERSIST,   "",         "SSL server certification, PEM str format"),
SDATA (data_type_t.DTP_JSON,    "extra_info",       sdata_flag_t.SDF_RD,        "{}",       "dict data set by user, added to the identity card msg."),
SDATA (data_type_t.DTP_INTEGER, "timeout_retry",    sdata_flag_t.SDF_RD,        "5000",     "timeout waiting idAck"),
SDATA (data_type_t.DTP_INTEGER, "timeout_idack",    sdata_flag_t.SDF_RD,        "5000",     "timeout waiting idAck"),
SDATA (data_type_t.DTP_POINTER, "subscriber",       0,              0,          "subscriber of output-events. If null then subscriber is the parent"),
SDATA_END()
];

/*---------------------------------------------*
 *      GClass trace levels
 *  HACK strict ascendant value!
 *  required paired correlative strings
 *  in s_user_trace_level
 *---------------------------------------------*/
// enum {
//     TRACE_IEVENTS       = 0x0001,
//     TRACE_IEVENTS2      = 0x0002,
//     TRACE_IDENTITY_CARD = 0x0004,
// };
// PRIVATE const trace_level_t s_user_trace_level[16] = {
//     {"ievents",        "Trace inter-events with metadata of kw"},
//     {"ievents2",       "Trace inter-events with full kw"},
//     {"identity-card",  "Trace identity_card messages"},
//     {0, 0},
// };

/*---------------------------------------------*
 *              Private data
 *---------------------------------------------*/
let PRIVATE_DATA = {
    url:                null,
    remote_yuno_name:   null,
    remote_yuno_role:   null,
    remote_yuno_service:null,
    gobj_timer:         null,
    inform_on_close:    false,
    websocket:          null,
    inside_on_open:     false,  // avoid duplicates, no subscriptions while in on_open,
                                // they will send in resend_subscriptions
};

let __gclass__ = null;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************
 *          Framework Method: Create
 ***************************************************************/
function mt_create(gobj)
{
    let priv = gobj.priv;

    /*
     *  Create childs
     */
    priv.gobj_timer = gobj_create_pure_child(gobj_name(gobj), "C_TIMER", {}, gobj);

    priv.url = gobj_read_str_attr(gobj, "url");
    priv.remote_yuno_name = gobj_read_str_attr(gobj, "remote_yuno_name");
    priv.remote_yuno_role = gobj_read_str_attr(gobj, "remote_yuno_role");
    priv.remote_yuno_service = gobj_read_str_attr(gobj, "remote_yuno_service");

    gobj_write_str_attr(gobj, "wanted_yuno_name", priv.remote_yuno_name);
    gobj_write_str_attr(gobj, "wanted_yuno_role", priv.remote_yuno_role);
    gobj_write_str_attr(gobj, "wanted_yuno_service", priv.remote_yuno_service);

    /*
     *  SERVICE subscription model
     */
    const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, null, {}, subscriber);
    }
}

/***************************************************************
 *          Framework Method: Writing
 ***************************************************************/
function mt_writing(gobj, path)
{
    let priv = gobj.priv;

    switch(path) {
        case "url":
            priv.url = gobj_read_str_attr(gobj, "url");
            break;
        case "remote_yuno_name":
            priv.remote_yuno_name = gobj_read_str_attr(gobj, "remote_yuno_name");
            break;
        case "remote_yuno_role":
            priv.remote_yuno_role = gobj_read_str_attr(gobj, "remote_yuno_role");
            break;
        case "remote_yuno_service":
            priv.remote_yuno_service = gobj_read_str_attr(gobj, "remote_yuno_service");
            break;
    }
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    let priv = gobj.priv;

    gobj_start(priv.gobj_timer);

    priv.websocket = setup_websocket(gobj);
    return 0;
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    let priv = gobj.priv;

    clear_timeout(priv.gobj_timer);

    gobj_stop(priv.gobj_timer);

    if(priv.websocket) {
        close_websocket(gobj);
    }

    return 0;
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
}

/***************************************************************
 *          Framework Method: Stats
 *  Ask for stats to remote `kw.service` (or wanted_yuno_service)
 ***************************************************************/
function mt_stats(gobj, stats, kw, src)
{
    if(gobj_current_state(gobj) !== "ST_SESSION") {
        log_error(`Not in session`);
        return null;
    }

    if(!kw) {
        kw = {};
    }

    /*
     *      __REQUEST__ __MESSAGE__
     */
    let jn_ievent_id = build_ievent_request(
        gobj,
        gobj_name(src),
        kw_get_str(gobj, kw, "__service__", null, 0)
    );
    json_object_del(kw, "__service__");

    msg_iev_push_stack(
        gobj,
        kw,
        IEVENT_MESSAGE_AREA_ID,
        jn_ievent_id
    );

    kw["__stats__"] = stats; // TODO deprecated

    msg_iev_write_key(gobj, kw, "__stats__");

    msg_iev_push_stack(
        gobj,
        kw,
        "__stats__",
        stats
    );

    msg_iev_set_msg_type(gobj, kw, "__stats__");

    send_static_iev(gobj, "EV_MT_STATS", kw, src);

    return null;   // return null on asynchronous response.
}

/***************************************************************
 *          Framework Method: Command
 *  Ask for command to remote `kw.service` (or wanted_yuno_service)
 ***************************************************************/
function mt_command(gobj, command, kw, src)
{
    if(gobj_current_state(gobj) !== "ST_SESSION") {
        log_error(`Not in session`);
        return null;
    }

    if(!kw) {
        kw = {};
    }

    /*
     *      __REQUEST__ __MESSAGE__
     */
    let jn_ievent_id = build_ievent_request(
        gobj,
        gobj_name(src),
        kw_get_str(gobj, kw, "__service__", null, 0)
    );
    json_object_del(kw, "__service__");

    msg_iev_push_stack(
        gobj,
        kw,
        IEVENT_MESSAGE_AREA_ID,
        jn_ievent_id
    );

    kw["__command__"] = command; // TODO deprecated

    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        "__command__",
        command   // owned
    );

    msg_iev_set_msg_type(gobj, kw, "__command__");

    send_static_iev(gobj, "EV_MT_COMMAND", kw, src);

    return null;   // return null on asynchronous response.
}

/***************************************************************
 *          Framework Method: inject_event
 *  Send event to remote `kw.__service__` (or wanted_yuno_service)
 ***************************************************************/
function mt_inject_event(gobj, event, kw, src)
{
    if(gobj_current_state(gobj) !== "ST_SESSION") {
        log_error(`Not in session`);
        return -1;
    }

    if(!kw) {
        kw = {};
    }

    /*
     *      __MESSAGE__
     */
    // let jn_request = msg_iev_get_stack(gobj, kw, IEVENT_MESSAGE_AREA_ID);
    // if(!jn_request) {
        /*
         * Put the ievent if it doesn't come with it,
         * if it does come with it, it's because it will be some kind of response/redirect
         */
        let jn_ievent_id = build_ievent_request(
            gobj,
            gobj_name(src),
            kw_get_str(gobj, kw, "__service__", 0, 0)
        );
        json_object_del(kw, "__service__");

        msg_iev_push_stack(
            gobj,
            kw,
            IEVENT_MESSAGE_AREA_ID,
            jn_ievent_id
        );

        // json_object_set_new(kw, "__message__", json_string(event)); // TODO deprecated

        msg_iev_push_stack(
            gobj,
            kw,
            "__message__",
            event
        );

    // }

    msg_iev_set_msg_type(gobj, kw, "__message__");

    return send_static_iev(gobj, event, kw, src);
}

/***************************************************************
 *      Framework Method subscription_added
 ***************************************************************/
function mt_subscription_added(gobj, subs)
{
    let priv = gobj.priv;

    if(gobj_current_state(gobj) !== "ST_SESSION") {
        // on_open will send all subscriptions
        return 0;
    }

    if(priv.inside_on_open) {
        // avoid duplicates of subscriptions
        return 0;
    }
    return send_remote_subscription(gobj, subs);
}

/***************************************************************
 *      Framework Method subscription_deleted
 ***************************************************************/
function mt_subscription_deleted(gobj, subs)
{
    let priv = gobj.priv;
    // in C: return 0;
    // TODO hay algo mal, las subscripciones locales se interpretan como remotas

    if(gobj_current_state(gobj) !== "ST_SESSION") {
        // Nothing to do. On open this subscription will be not sent.
        return 0;
    }

    if(priv.inside_on_open) {
        // avoid duplicates of subscriptions
        return 0;
    }
    if(empty_string(subs.event)) {
        // HACK only resend explicit subscriptions
        return;
    }

    let kw = {};

    let __global__ = subs.__global__;
    let __config__ = subs.__config__;
    let __filter__ = subs.__filter__;
    let __service__ = subs.__service__;
    let subscriber = subs.subscriber;

    if(__config__) {
        kw["__config__"] = __config__;
    }
    if(__global__) {
        kw["__global__"] = __global__;
    }
    if(__filter__) {
        kw["__filter__"] = __filter__;
    }

    /*
     *      __MESSAGE__
     */
    let jn_ievent_id = build_ievent_request(
        gobj,
        gobj_name(subscriber),
        __service__
    );
    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        IEVENT_MESSAGE_AREA_ID,
        jn_ievent_id   // owned
    );

    msg_iev_set_msg_type(gobj, kw, "__unsubscribing__");

    return send_static_iev(gobj, subs.event, kw, gobj);
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************
 *  Setup WebSocket
 *  Mixin DOM events -> Yuneta events
 ***************************************************************/
function setup_websocket(gobj)
{
    if(gobj.priv.websocket) {
        log_error(`Websocket ALREADY setup`);
    }

    const url = gobj_read_str_attr(gobj, "url");
    log_debug(`====> Starting WebSocket to '${url}' (${gobj_short_name(gobj)})`);

    let websocket;
    try {
        websocket = new WebSocket(url);
    } catch (e) {
        log_error(`${gobj_short_name(gobj)}: Cannot open WebSocket to '${url}', Error: ${e.message}`);
        return null;
    }

    if (!websocket) {
        log_error(`${gobj_short_name(gobj)}: Failed to initialize WebSocket.`);
        return null;
    }

    websocket.onopen = () => {
        // log_debug(`${gobj_short_name(gobj)}: WebSocket connection opened.`);
        gobj_send_event(gobj, "EV_ON_OPEN", { url }, gobj);
    };

    websocket.onmessage = (e) => {
        // log_debug(`${gobj_short_name(gobj)}: Received message: ${event.data}`);
        gobj_send_event(gobj, "EV_ON_MESSAGE", { url, data: e.data }, gobj);
    };

    // not useful
    // websocket.onerror = (e) => {
    //     log_error(`${gobj_short_name(gobj)}: WebSocket error occurred.`);
    // };

    websocket.onclose = (e) => {
        if(!gobj_read_bool_attr(gobj_yuno(), "browser_beforeunload")) {
            gobj_send_event(
                gobj,
                "EV_ON_CLOSE",
                {
                    url, e: e
                },
                gobj
            );
        } else {
            log_warning('Close websocket by unload page');
        }
    };

    return websocket; // Return the WebSocket instance
}

/***************************************************************
 *  Closes the WebSocket connection safely.
 ***************************************************************/
function close_websocket(gobj, code = 1000, reason = "")
{
    let priv = gobj.priv;

    let websocket = priv.websocket;
    priv.websocket = null;

    if (!websocket ||
        websocket.readyState === WebSocket.CLOSED ||
        websocket.readyState === WebSocket.CLOSING)
    {
        log_warning("WebSocket is already closed or in the process of closing.");
        return;
    }

    log_debug(`Closing WebSocket (Code: ${code}, Reason: "${reason}")`);

    try {
        websocket.close(code, reason);
    } catch (e) {
        log_error("Error closing WebSocket:", e.message);
    }
}

/***************************************************************
 *      Trace intra event
 ***************************************************************/
function trace_inter_event(gobj, prefix, iev)
{
    let hora = current_timestamp();
    try {
        log_debug("\n" + hora + " " + prefix + "\n");
        //trace_msg(JSON.stringify(iev,  null, 4));
        trace_json(iev);
    } catch (e) {
        log_debug("ERROR in trace_inter_event: " + e);
    }
}

/************************************************************
 *  inter event container
 ************************************************************/
class InterEvent {
    constructor(event, kw) {
        this.event = event;
        this.kw = kw || {};
    }
}

/************************************************************
 *        Create inter-event
 ************************************************************/
function iev_create(
        event,
        kw)
{
    if(empty_string(event)) {
        log_error("iev_create() event NULL");
        return null;
    }

    return new InterEvent(event, kw);
}

/**************************************
 *  Send jsonify inter-event message
 **************************************/
function send_iev(gobj, iev)
{
    let priv = gobj.priv;

    let msg = JSON.stringify(iev);

    if(gobj_read_bool_attr(gobj_yuno(), "trace_inter_event")) {
        let url = gobj_read_str_attr(gobj, "url");
        let prefix = gobj_name(gobj_yuno()) + ' ==> ' + url;
        let trace_ievent_callback = gobj_read_attr(gobj_yuno(), "trace_ievent_callback");
        if(trace_ievent_callback) {
            let size = msg.length;
            trace_ievent_callback(prefix, iev, 1, size);
        } else {
            trace_inter_event(gobj, prefix, iev);
        }
    }

    try {
        priv.websocket.send(msg);
    } catch (e) {
        log_error(`${gobj_short_name(gobj)}: send_iev(): ${e}`);
        //log_error(msg);
    }
    return 0;
}

/**************************************
 *
 **************************************/
function send_static_iev(gobj, event, kw, src)
{
    let iev = iev_create(
        event,
        kw
    );

    return send_iev(gobj, iev);
}

/***************************************************************
 *  __MESSAGE__
 ***************************************************************/
function build_ievent_request(gobj, src_service, dst_service)
{
    if(empty_string(dst_service)) {
        dst_service = gobj_read_str_attr(gobj, "wanted_yuno_service");
    }

    return {
        "dst_yuno": gobj_read_str_attr(gobj, "wanted_yuno_name"),
        "dst_role": gobj_read_str_attr(gobj, "wanted_yuno_role"),
        "dst_service": dst_service,

        "src_yuno": gobj_yuno_name(),
        "src_role": gobj_yuno_role(),
        "src_service": src_service,
        "host": node_uuid()
    };
}

/***************************************************************
 *      Send identity card
 ***************************************************************/
function send_identity_card(gobj)
{
    let priv = gobj.priv;

    let playing = gobj_is_playing(gobj_yuno());
    let yuno_version = gobj_read_str_attr(gobj_yuno(), "yuno_version");
    let yuno_release = gobj_read_str_attr(gobj_yuno(), "yuno_release");
    let yuno_tag = gobj_read_str_attr(gobj_yuno(), "yuno_tag");

    let kw = {
        "yuno_role": gobj_yuno_role(),
        "yuno_id": gobj_yuno_id(),
        "yuno_name": gobj_yuno_name(),
        "yuno_tag": yuno_tag,
        "yuno_version": yuno_version,
        "yuno_release": yuno_release,
        "yuneta_version": YUNETA_VERSION,
        "playing": playing,
        "pid": 0,
        "watcher_pid": 0,
        "jwt": gobj_read_str_attr(gobj, "jwt"),
        "username": gobj_read_str_attr(gobj_yuno(), "__username__"),
        "launch_id": 0,
        "yuno_startdate": gobj_read_str_attr(gobj_yuno(), "start_date"),
        "id": node_uuid(),
        "user_agent": window?window.navigator.userAgent:"",
        "language": window?window.navigator.language:"",
        "required_services": gobj_read_attr(gobj_yuno(), "required_services")
    };

    /*
     *      __REQUEST__ __MESSAGE__
     */
    let jn_ievent_id = build_ievent_request(
        gobj,
        gobj_name(gobj_parent(gobj)), // TODO ??? in C is gobj_name(gobj),
        null
    );
    msg_iev_push_stack(
        gobj,
        kw,
        IEVENT_MESSAGE_AREA_ID,
        jn_ievent_id   // owned
    );

    msg_iev_set_msg_type(gobj, kw, "__identity__");

    set_timeout(priv.gobj_timer, gobj_read_integer_attr(gobj, "timeout_idack"));

    return send_static_iev(gobj, "EV_IDENTITY_CARD", kw, gobj);
}

/********************************************
 *  Create iev from data received
 *  on websocket connection
 ********************************************/
function iev_create_from_json(gobj, data)
{
    let x;
    try {
        x = JSON.parse(data);
    } catch (e) {
        log_error("parsing inter_event json: " + e);
        return null;
    }

    if(!(x instanceof Object)) {
        log_error("parsing inter_event: websocket data not a json object");
        return null;
    }
    let event = x['event'];
    if(!event) {
        log_error("parsing inter_event: no event");
        return null;
    }
    if(typeof event !== 'string') {
        log_error("parsing inter_event: event not a string");
        return null;
    }

    let kw = x['kw'];
    if(!kw) {
        log_error("parsing inter_kw: no kw");
        return null;
    }
    if(!(kw instanceof Object)) {
        log_error("parsing inter_event: kw not a json object");
        return null;
    }

    return {
        event: event,
        kw: kw || {}
    };
}

/************************************************
 *      Framework Method subscription_added
 *
 *  SCHEMA subs
 *  ===========
 *
    publisher           gobj
    subscriber          gobj
    event               str
    renamed_event       str
    hard_subscription   bool
    __config__          json (dict)
    __global__          json (dict)
    __filter__          json (dict)
    __service__         json (str)

 *
 ************************************************/
function send_remote_subscription(gobj, subs)
{
    if(empty_string(subs.event)) {
        // HACK only resend explicit subscriptions
        return -1;
    }

    let kw = {};

    let __global__ = subs.__global__;
    let __config__ = subs.__config__;
    let __filter__ = subs.__filter__;
    let __service__ = subs.__service__;
    let subscriber = subs.subscriber;

    if(__config__) {
        kw["__config__"] = __config__;
    }
    if(__global__) {
        kw["__global__"] = __global__;
    }
    if(__filter__) {
        kw["__filter__"] = __filter__;
    }

    /*
     *      __MESSAGE__
     */
    let jn_ievent_id = build_ievent_request(
        gobj,
        gobj_name(subscriber),
        __service__
    );
    msg_iev_push_stack(
        gobj,
        kw,         // not owned
        IEVENT_MESSAGE_AREA_ID,
        jn_ievent_id   // owned
    );

    msg_iev_set_msg_type(gobj, kw, "__subscribing__");

    return send_static_iev(gobj, subs.event, kw, gobj);
}

/***************************************************************
 *  resend subscriptions
 ***************************************************************/
function resend_subscriptions(gobj)
{
    let dl_subs = gobj_find_subscriptions(gobj);
    for(let i=0; i<dl_subs.length; i++) {
        let subs = dl_subs[i];
        send_remote_subscription(gobj, subs);
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *
 ***************************************************************/
function ac_timeout_disconnected(gobj, event, kw, src)
{
    let priv = gobj.priv;

    if(gobj_is_running(gobj)) {
        priv.websocket = setup_websocket(gobj);
    }
    return 0;
}

/***************************************************************
 *
 ***************************************************************/
function ac_on_open(gobj, event, kw, src)
{
    log_debug('Websocket opened: ' + gobj.priv.url);
    send_identity_card(gobj);
    return 0;
}

/***************************************************************
 *
 ***************************************************************/
function ac_on_close(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let e = kw.e;
    let url = kw.url;

    log_warning(`${gobj_short_name(gobj)}: WebSocket closed ${url} (Code: ${e.code}, Reason: "${e.reason}", Clean: ${e.wasClean})`);

    // Cleanup WebSocket event handlers
    if(priv.websocket) {
        priv.websocket.onopen = null;
        priv.websocket.onmessage = null;
        priv.websocket.onerror = null;
        priv.websocket.onclose = null;
        priv.websocket = null;
    }

    if(priv.inform_on_close) {
        priv.inform_on_close = false;
        // Any interesting information for on_close event?
        gobj_publish_event(
            gobj,
            'EV_ON_CLOSE',
            {
                url: url,
                remote_yuno_name: priv.remote_yuno_name,
                remote_yuno_role: priv.remote_yuno_role,
                remote_yuno_service: priv.remote_yuno_service
            }
        );
    }

    if(gobj_is_running(gobj)) {
        set_timeout(priv.gobj_timer, gobj_read_integer_attr(gobj, "timeout_retry"));
    }

    return 0;
}

/********************************************
 *
 ********************************************/
function ac_timeout_wait_idAck(gobj, event, kw, src)
{
    send_identity_card(gobj);
    return 0;
}

/***************************************************************
 *
 ***************************************************************/
function ac_identity_card_ack(gobj, event, kw, src)
{
    let priv = gobj.priv;

    /*---------------------------------------*
     *  Clear timeout
     *---------------------------------------*/
    clear_timeout(priv.gobj_timer);

    /*---------------------------------------*
     *  Update remote values
     *---------------------------------------*/
    /*
     *  Here is the end point of the request.
     *  Don't pop the request, because
     *  the event can be publishing to more users.
     */
    /*
     *      __ANSWER__ __MESSAGE__
     */
    let event_id = msg_iev_get_stack(gobj, kw, IEVENT_MESSAGE_AREA_ID, true);
    let src_yuno = kw_get_str(gobj, event_id, "src_yuno", "");
    let src_role = kw_get_str(gobj, event_id, "src_role", "");
    let src_service = kw_get_str(gobj, event_id, "src_service", "");
    gobj_write_str_attr(gobj, "remote_yuno_name", src_yuno);
    gobj_write_str_attr(gobj, "remote_yuno_role", src_role);
    gobj_write_str_attr(gobj, "remote_yuno_service", src_service);

    // WARNING comprueba result, ahora puede venir negativo
    let result = kw_get_int(gobj, kw, "result", -1, 0);
    let comment = kw_get_str(gobj, kw, "comment", "", 0);
    let username_ = kw_get_str(gobj, kw, "username", "", 0);
    let services_roles = kw_get_dict_value(gobj, kw, "services_roles", {});
    if(result < 0) {
        close_websocket(gobj);

        let kw_nak = {
            "url": gobj_read_str_attr(gobj, "url"),
            "result": result,
            "comment": comment,
            "username": username_,
            "remote_yuno_name": src_yuno,
            "remote_yuno_role": src_role,
            "remote_yuno_service": src_service
        };

        /*
         *  SERVICE subscription model
         */
        if(gobj_is_pure_child(gobj)) {
            gobj_send_event(gobj_parent(gobj), "EV_ON_ID_NAK", kw_nak, gobj);
        } else {
            gobj_publish_event(gobj, "EV_ON_ID_NAK", kw_nak);
        }

    } else {
        gobj_change_state(gobj, "ST_SESSION");
        priv.inside_on_open = true;

        if(!priv.inform_on_close) {
            priv.inform_on_close = true;
            let kw_on_open = {
                "username": username_,
                "remote_yuno_name": src_yuno,
                "remote_yuno_role": src_role,
                "remote_yuno_service": src_service,
                "services_roles": services_roles,
            };

            /*
             *  SERVICE subscription model
             */
            if(gobj_is_pure_child(gobj)) {
                gobj_send_event(gobj_parent(gobj), "EV_ON_OPEN", kw_on_open, gobj);
            } else {
                gobj_publish_event(gobj, "EV_ON_OPEN", kw_on_open);
            }
        }

        priv.inside_on_open = false;

        /*
         *  Resend subscriptions
         */
        resend_subscriptions(gobj);
    }

    return 0;
}

/***************************************************************
 *
 ***************************************************************/
function ac_on_message(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let url = priv.url;

    /*------------------------------------------*
     *  Create inter_event from received data
     *------------------------------------------*/
    let iev_msg = iev_create_from_json(gobj, kw.data);

    /*---------------------------------------*
     *          trace inter_event
     *---------------------------------------*/
    if(gobj_read_bool_attr(gobj_yuno(), "trace_inter_event")) {
        let prefix = gobj_name(gobj_yuno()) + ' <== ' + url;
        let size = kw.data.length;
        let trace_ievent_callback = gobj_read_attr(gobj_yuno(), "trace_ievent_callback");
        if(trace_ievent_callback) {
            trace_ievent_callback(prefix, iev_msg, 2, size);
        } else {
            trace_inter_event(gobj, prefix, iev_msg);
        }
    }

    /*----------------------------------------*
     *
     *----------------------------------------*/
    let iev_event = iev_msg.event;
    let iev_kw = iev_msg.kw;

    /*-----------------------------------------*
     *  If state is not in SESSION, send self.
     *  Mainly process EV_IDENTITY_CARD_ACK
     *-----------------------------------------*/
    if(gobj_current_state(gobj) !== "ST_SESSION") {
        if(gobj_has_event(gobj, iev_event, event_flag_t.EVF_PUBLIC_EVENT)) {
            if(gobj_send_event(gobj, iev_event, iev_kw, gobj)===0) {
                // iev_kw consumed
                return 0;
            }
            // iev_kw consumed
        } else {
            log_error(`${gobj_short_name(gobj)}: event UNKNOWN in not-session state ${iev_event}`);
        }
        close_websocket(gobj);
        return -1;
    }

    iev_msg = null;

    /*------------------------------------*
     *   Analyze inter_event
     *------------------------------------*/
    let msg_type = msg_iev_get_msg_type(iev_kw);

    /*----------------------------------------*
     *  Get inter-event routing information.
     *----------------------------------------*/
    let event_id = msg_iev_get_stack(gobj, iev_kw, IEVENT_MESSAGE_AREA_ID);

    /*----------------------------------------*
     *  Check dst role^name
     *----------------------------------------*/
    let iev_dst_role = kw_get_str(gobj, event_id, "dst_role", "");
    // Check yuno_name too

    if(iev_dst_role !== gobj_yuno_role()) {
        log_error(`"It's not my role, dst_role: ${iev_dst_role}, my_role: ${gobj_yuno_role()}`);
        return 0;
    }

    /*----------------------------------------*
     *  Check dst service
     *----------------------------------------*/
    let ret = 0;
    let iev_dst_service = kw_get_str(gobj, event_id, "dst_service", "");

    /*------------------------------------*
     *   Is the event a subscription?
     *------------------------------------*/
    if(msg_type === "__subscribing__") {
        /*
         *  it's a external subscription
         */
        // TODO subscription
        return -1;
    }

    /*---------------------------------------*
     *   Is the event is a unsubscription?
     *---------------------------------------*/
    if(msg_type === "__unsubscribing__") {
        /*
         *  it's a external unsubscription
         */
        // TODO unsubscription
        return -1;
    }

    /*-------------------------------------------------------*
     *  Filter public events of this gobj
     *-------------------------------------------------------*/
    if(gobj_has_event(gobj, iev_event, event_flag_t.EVF_PUBLIC_EVENT)) {
        /*
         *  It's mine (I manage inter-command and inter-stats)
         */
        gobj_send_event(gobj, iev_event, iev_kw, gobj);
        return 0;
    }

    /*-------------------------*
     *  Dispatch the event
     *-------------------------*/
    let gobj_service = gobj_find_service(iev_dst_service, true);

    if(gobj_service) {
        if(gobj_has_event(gobj_service, iev_event, event_flag_t.EVF_PUBLIC_EVENT)) {
            gobj_send_event(gobj_service, iev_event, iev_kw, gobj);
        } else {
            log_error(`${gobj_short_name(gobj)}: Service found but no event or not public, service '${gobj_short_name(gobj_service)}', ev '${iev_event}'`);
            return -1;
        }
    } else {
        /*
         *  SERVICE subscription model
         *  TODO Shouldn't this event be rejected? It may make sense in routing task.
         */
        if(gobj_is_pure_child(gobj)) {
            gobj_send_event(gobj_parent(gobj), iev_event, iev_kw, gobj);
        } else {
            gobj_publish_event(gobj, iev_event, iev_kw);
        }
    }

    return 0;
}

/***************************************************************************
 *  remote asking for stats
 ***************************************************************************/
function ac_mt_stats(gobj, event, kw, src)
{
    // TODO
    return null;
}

/***************************************************************************
 *  remote asking for command
 ***************************************************************************/
function ac_mt_command(gobj, event, kw, src)
{
    // TODO
    return null;
}

/***************************************************************************
 *  remote asking for play yuno
 ***************************************************************************/
function ac_play_yuno(gobj, event, kw, src)
{
    // TODO
    return -1;
}

/***************************************************************************
 *  remote asking for pause yuno
 ***************************************************************************/
function ac_pause_yuno(gobj, event, kw, src)
{
    // TODO
    return -1;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
const gmt = {
    mt_create:              mt_create,
    mt_writing:             mt_writing,
    mt_start:               mt_start,
    mt_stop:                mt_stop,
    mt_destroy:             mt_destroy,
    mt_stats:               mt_stats,
    mt_command_parser:      mt_command,
    mt_inject_event:        mt_inject_event,
    mt_subscription_added:  mt_subscription_added,
    mt_subscription_deleted: mt_subscription_deleted,
};


/***************************************************************
 *          Create the GClass
 ***************************************************************/
function create_gclass(gclass_name)
{
    if(__gclass__) {
        log_error(`GClass ALREADY created: ${gclass_name}`);
        return -1;
    }

    /*---------------------------------------------*
     *          States
     *---------------------------------------------*/
    const st_disconnected = [
        ["EV_ON_OPEN",              ac_on_open,             "ST_WAIT_IDENTITY_CARD_ACK"],
        ["EV_ON_CLOSE",             ac_on_close,            null],
        ["EV_TIMEOUT",              ac_timeout_disconnected,null],
        [null, null, null]
    ];
    const st_wait_identity_card_ack = [
        ["EV_ON_MESSAGE",           ac_on_message,          null],
        ["EV_IDENTITY_CARD_ACK",    ac_identity_card_ack,   null],
        ["EV_ON_CLOSE",             ac_on_close,            "ST_DISCONNECTED"],
        ["EV_TIMEOUT",              ac_timeout_wait_idAck,  null],
        [null, null, null]
    ];
    const st_session = [
        ["EV_ON_MESSAGE",           ac_on_message,          null],
        ["EV_MT_STATS",             ac_mt_stats,            null],
        ["EV_MT_COMMAND",           ac_mt_command,          null],
        ["EV_ON_CLOSE",             ac_on_close,            "ST_DISCONNECTED"],
        [null, null, null]
    ];

    const states = [
        ["ST_DISCONNECTED",           st_disconnected],
        ["ST_WAIT_IDENTITY_CARD_ACK", st_wait_identity_card_ack],
        ["ST_SESSION",                st_session],
        [null, null]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_ON_MESSAGE",           event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_IDENTITY_CARD_ACK",    event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_ON_ID_NAK",            event_flag_t.EVF_PUBLIC_EVENT|event_flag_t.EVF_OUTPUT_EVENT],

        ["EV_MT_STATS",             event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_MT_COMMAND",           event_flag_t.EVF_PUBLIC_EVENT],

        ["EV_ON_OPEN",              event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
        ["EV_ON_CLOSE",             event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
        ["EV_TIMEOUT",              0],
        [null, 0]
    ];

    /*----------------------------------------*
     *          Create the gclass
     *----------------------------------------*/
    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,  // lmt,
        attrs_table,
        PRIVATE_DATA,
        0,  // authz_table,
        0,  // command_table,
        0,  // s_user_trace_level
        gclass_flag_t.gcflag_no_check_output_events // gclass_flag
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
function register_c_ievent_cli()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_ievent_cli };
