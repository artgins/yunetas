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
} from "./gobj.js";

import {
    log_error,
    log_debug,
    trace_msg,
    log_warning,
    get_current_datetime,
    empty_string,
    json_array_get,
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
    inform_on_close:    false
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
    return 0;
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    return 0;
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************
 *  Setup WebSocket
 *  Mixin DOM events -> Yuneta events
 ***************************************************************/
function setup_websocket(gobj) {
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
function close_websocket(websocket, code = 1000, reason = "") {
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

/*****************************************
 *      Trace intra event
 *****************************************/
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
function InterEvent(
        event,
        kw) {
    this.event = event;
    this.kw = kw || {};
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

    return {
        event: event,
        kw: kw || {}
    };
}

/**************************************
 *  Send jsonify inter-event message
 **************************************/
function send_iev(self, iev)
{
    var msg = JSON.stringify(iev);

    if (self.yuno.config.trace_inter_event) {
        var url = self.config.urls[self.config.idx_url];
        var prefix = self.yuno.yuno_name + ' ==> ' + url;
        if(self.yuno.config.trace_ievent_callback) {
            var size = msg.length;
            self.yuno.config.trace_ievent_callback(prefix, iev, 1, size);
        } else {
            trace_inter_event(self, prefix, iev);
        }
    }

    try {
        self.websocket.send(msg);
    } catch (e) {
        log_error(self.gobj_short_name() + ": send_iev(): " + e);
        log_error(msg);
    }
    return 0;
}

/**************************************
 *
 **************************************/
function send_static_iev(self, event, kw)
{
    var iev = iev_create(
        event,
        kw
    );

    return send_iev(self, iev);
}

/**************************************
 *
 **************************************/
function build_ievent_request(self, src_service, dst_service)
{
    var jn_ievent_chain = {
        dst_yuno: self.config._wanted_yuno_name,
        dst_role: self.config._wanted_yuno_role,
        dst_service: dst_service?dst_service:self.config._wanted_yuno_service,
        src_yuno: self.yuno.yuno_name,
        src_role: self.yuno.yuno_role,
        src_service: src_service
    };
    return jn_ievent_chain;
}

/**************************************
 *
 **************************************/
function ievent_answer_filter(self, kw_answer, area_key, ivent_gate_stack, src)
{
    var ievent = json_array_get(ievent_gate_stack, 0);

    /*
    *  Dale la vuelta src->dst dst->src
    */
    var iev_src_service = kw_get_str(ievent, "src_service", "");

    ievent["dst_yuno"] = self.config.remote_yuno_name;
    ievent["dst_role"] = self.config.remote_yuno_role;
    ievent["dst_service"] = iev_src_service;

    ievent["src_yuno"] = self.yuno.yuno_name;
    ievent["src_role"] = self.yuno.yuno_role;
    ievent["src_service"] = src.name;
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
