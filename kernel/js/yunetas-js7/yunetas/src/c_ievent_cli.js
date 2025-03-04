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
    SDATA,
    SDATA_END,
    data_type_t,
    gclass_flag_t,
    sdata_flag_t,
    event_flag_t,
    GObj,
    gclass_create,
    gobj_parent,
    gobj_short_name,
    gobj_yuno,
    gobj_send_event,
    gobj_publish_event,
    gobj_read_bool_attr,
    gobj_read_integer_attr,
    gobj_read_str_attr,
    gobj_read_pointer_attr,
    gobj_write_str_attr,
    gobj_subscribe_event,
    gobj_is_pure_child,
    gobj_gclass_name,
    gobj_write_bool_attr,
    gobj_write_integer_attr,
    gobj_create_pure_child,
    gobj_name,
    gobj_current_state,
    gobj_yuno_name,
    gobj_yuno_role, gobj_find_subscriptions,

} from "./gobj.js";

import {
    log_error,
    log_debug,
    trace_msg,
    log_warning,
    get_current_datetime,
    empty_string,
    msg_iev_push_stack,
    msg_iev_get_stack,
    kw_get_str,
    json_object_del, kw_set_dict_value, msg_set_msg_type,
} from "./utils.js";

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
    remote_yuno_name:   null,
    remote_yuno_role:   null,
    remote_yuno_service:null,
    gobj_timer:         null,
    inform_on_close:    false,
    websocket:          null,
    inside_on_open:     false,  // avoid duplicates, no subscriptions while in on_open,
                                // will send in resend_subscriptions
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
    gobj.priv.gobj_timer = gobj_create_pure_child(gobj_name(gobj), "C_TIMER", {}, gobj);

    priv.periodic = gobj_read_bool_attr(gobj, "periodic");

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
        gobj_subscribe_event(gobj, null, null, subscriber);
    }
}

/***************************************************************
 *          Framework Method: Writing
 ***************************************************************/
function mt_writing(gobj, path)
{
    let priv = gobj.priv;

    switch(path) {
        case "remote_yuno_name":
            priv.remote_yuno_name = gobj_read_bool_attr(gobj, "remote_yuno_name");
            break;
        case "remote_yuno_role":
            priv.remote_yuno_role = gobj_read_bool_attr(gobj, "remote_yuno_role");
            break;
        case "remote_yuno_service":
            priv.remote_yuno_service = gobj_read_bool_attr(gobj, "remote_yuno_service");
            break;
    }
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    let priv = gobj.priv;

    priv.websocket = setup_websocket(gobj);

    return 0;
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    let priv = gobj.priv;

    close_websocket(priv.websocket);

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
        kw_get_str(gobj, kw, "service", null, 0)
    );
    msg_iev_push_stack(
        gobj,
        kw,
        IEVENT_MESSAGE_AREA_ID,
        jn_ievent_id
    );

    kw["__stats__"] = stats;
    msg_iev_push_stack( // TODO review, before was msg_iev_write_key
        gobj,
        kw,
        "__stats__",
        stats
    );

    send_static_iev(gobj, "EV_MT_STATS", kw);

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
        kw_get_str(gobj, kw, "service", null, 0)
    );
    msg_iev_push_stack(
        gobj,
        kw,
        IEVENT_MESSAGE_AREA_ID,
        jn_ievent_id
    );

    kw["__command__"] = command;
    msg_iev_push_stack( // TODO review, before was msg_iev_write_key
        gobj,
        kw,         // not owned
        "__command__",
        command   // owned
    );

    send_static_iev(gobj, "EV_MT_COMMAND", kw);

    return null;   // return null on asynchronous response.
}

/***************************************************************
 *          Framework Method: inject_event
 *  Send event to remote `kw.service` (or wanted_yuno_service)
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
    let jn_request = msg_iev_get_stack(kw, IEVENT_MESSAGE_AREA_ID);
    if(!jn_request) {
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
    }

    return send_static_iev(gobj, event, kw);
}

/***************************************************************
 *      Framework Method subscription_added
 ***************************************************************/
function mt_subscription_added(gobj, subs)
{
    let priv = gobj.priv;

    // esto es en C: return 0;
    // TODO hay algo mal, las subscripciones locales se interpretan como remotas

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
    // esto es en C: return 0;
    // TODO hay algo mal, las subscripciones locales se interpretan como remotas

    if(gobj_current_state(gobj) !== "ST_SESSION") {
        // Nothing to do. On open this subscription will be not sent.
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
        kw,         // not owned
        IEVENT_MESSAGE_AREA_ID,
        jn_ievent_id   // owned
    );

    msg_set_msg_type(kw, "__unsubscribing__");
    // esto en C: kw_set_dict_value(gobj, kw, "__md_iev__`__msg_type__", json_string("__unsubscribing__"));

    return send_static_iev(gobj, subs.event, kw);
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
    const url = gobj_read_str_attr(gobj, "url");
    log_debug(`====> Starting WebSocket to '${url}' (${gobj_short_name(gobj)})`);

    // Define event handlers (bound to `gobj`)
    function handleOpen() {
        gobj_send_event(gobj, "EV_ON_OPEN", { url }, gobj);
    }

    function handleClose(event) {
        gobj_send_event(gobj, "EV_ON_CLOSE", { url, code: event.code, reason: event.reason }, gobj);
    }

    function handleError(event) {
        log_error(`${gobj_short_name(gobj)}: WebSocket error occurred.`);
    }

    function handleMessage(event) {
        gobj_send_event(gobj, "EV_ON_MESSAGE", { url, data: event.data }, gobj);
    }

    // Initialize WebSocket
    let websocket;
    try {
        websocket = new WebSocket(url);
    } catch (e) {
        log_error(`${gobj_short_name(gobj)}: Cannot open WebSocket to '${url}', Error: ${e.message}`);
        return null;
    }

    // Validate WebSocket instance
    if (!websocket) {
        log_error(`${gobj_short_name(gobj)}: Failed to initialize WebSocket.`);
        return null;
    }

    // Assign WebSocket event handlers
    websocket.onopen = handleOpen.bind(gobj);
    websocket.onclose = handleClose.bind(gobj);
    websocket.onerror = handleError.bind(gobj);
    websocket.onmessage = handleMessage.bind(gobj);

    return websocket; // Return WebSocket instance
}

/********************************************
 *  Close websocket
 ********************************************/
function xclose_websocket(self)
{
    self.clear_timeout();
    if(self.websocket) {
        try {
            if(self.websocket) {
                if(self.websocket.close) {
                    self.websocket.close();
                } else if(self.websocket.websocket.close) {
                    self.websocket.websocket.close();
                } else {
                    trace_msg("What fuck*! websocket.close?");
                }
            }
        } catch (e) {
            log_error(self.gobj_short_name() + ": close_websocket(): " + e);
        }
        // self.websocket = null; // HACK wait to on_close
    }
}

/***************************************************************
 *  Closes the WebSocket connection safely.
 ***************************************************************/
function close_websocket(websocket, code = 1000, reason = "")
{
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
function trace_inter_event(self, prefix, iev)
{
    let hora = get_current_datetime();
    try {
        log_debug("\n" + hora + " " + prefix + "\n");
        //trace_msg(JSON.stringify(iev,  null, 4));
        trace_msg(iev);
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

    // if (self.yuno.config.trace_inter_event) {
    //     var url = self.config.urls[self.config.idx_url];
    //     var prefix = self.yuno.yuno_name + ' ==> ' + url;
    //     if(self.yuno.config.trace_ievent_callback) {
    //         var size = msg.length;
    //         self.yuno.config.trace_ievent_callback(prefix, iev, 1, size);
    //     } else {
    //         trace_inter_event(self, prefix, iev);
    //     }
    // }

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
function send_static_iev(gobj, event, kw)
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
        "src_service": src_service
    };
}

/********************************************
 *      Send identity card
 ********************************************/
function send_identity_card(self)
{
    var kw = {
        "yuno_role": self.yuno.yuno_role,
        "yuno_name": self.yuno.yuno_name,
        "yuno_version": self.yuno.yuno_version,
        "yuno_release": self.yuno.yuno_version,
        "yuneta_version": "4.19.0",
        "playing": false,
        "pid": 0,
        "jwt": self.config.jwt,
        "user_agent": navigator.userAgent,
        "launch_id" : 0,
        "yuno_startdate" : "", // TODO
        "required_services": self.config.required_services
    };
    /*
     *      __REQUEST__ __MESSAGE__
     */
    var jn_ievent_id = build_ievent_request(
        self,
        self.parent.name,
        null
    );
    msg_iev_push_stack(
        kw,
        IEVENT_MESSAGE_AREA_ID,
        jn_ievent_id   // owned
    );

    self.set_timeout(self.config.timeout_idack*1000);

    return send_static_iev(self, "EV_IDENTITY_CARD", kw);
}

/********************************************
 *  Create iev from data received
 *  on websocket connection
 ********************************************/
function iev_create_from_json(self, data)
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
        kw,         // not owned
        IEVENT_MESSAGE_AREA_ID,
        jn_ievent_id   // owned
    );

    msg_set_msg_type(kw, "__subscribing__");
    // esto en C: kw_set_dict_value(gobj, kw, "__md_iev__`__msg_type__", json_string("__subscribing__"));

    return send_static_iev(gobj, subs.event, kw);
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
function ac_timeout(gobj, event, kw, src)
{
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
const gmt = {
    mt_create:  mt_create,
    mt_writing: mt_writing,
    mt_start:   mt_start,
    mt_stop:    mt_stop,
    mt_destroy: mt_destroy,
    mt_stats:   mt_stats,
    mt_command: mt_command,
    mt_inject_event: mt_inject_event,
    mt_subscription_added: mt_subscription_added,
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
    const st_idle = [
        ["EV_TIMEOUT_PERIODIC",     ac_timeout,     null]
    ];

    const states = [
        ["ST_IDLE",     st_idle]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_TIMEOUT_PERIODIC",      0]
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
        gclass_flag_t.gcflag_manual_start // gclass_flag
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
function register_c_sample()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_sample };
