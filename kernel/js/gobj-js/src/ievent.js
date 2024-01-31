/**************************************************************************
 *      IEvent GClass

DEBUG: {
    "realm_name": "",
    "yuno_role": "Mobile",
    "yuno_name": "mobile",
    "yuno_version": "1.0.0",
    "yuno_release": "1.0.0",
    "yuneta_version": "4.19.0",
    "playing": false,
    "pid": 0,
    "jwt": "",
    "launch_id": 0,
    "required_services": [],
    "__md_iev__": {
        "ievent_gate_stack": [
            {
                "dst_yuno": null,
                "dst_role": "mobile_gate",
                "dst_service": "agent",
                "src_yuno": "mobile",
                "src_role": "Mobile",
                "src_service": "iev___default_service__"
            }
        ]
    }
}

 *      Interevents to remote yuno
 **************************************************************************/
(function (exports) {
    "use strict";

    /********************************************
     *      Configuration (C attributes)
     ********************************************/
    var CONFIG = {
        timeout_retry: 5,               // retry websocket connection
        timeout_idack: 5,               // timeout waiting idAck

        _wanted_yuno_name: null,        // wanted yuno service names
        _wanted_yuno_role: null,
        _wanted_yuno_service: null,

        remote_yuno_name: null,         // confirmed remote yuno names
        remote_yuno_role: null,
        remote_yuno_service: null,
        required_services: null,

        inside_on_open: false,  // avoid duplicates, no subscriptions while in on_open,
                                // will send in resend_subscriptions

        jwt: null,
        urls: null,
        idx_url: 0
    };

    var IEVENT_MESSAGE_AREA_ID = "ievent_gate_stack";




            /***************************
             *      Local Methods
             ***************************/




    /****************************************
     *      Setup websocket
     *  Mixin DOM events -> Yuneta events
     ****************************************/
    function setup_websocket(self)
    {
        var url = self.config.urls[self.config.idx_url];
        log_debug("====> Starting WebSocket to '" + url + "' (" + self.gobj_short_name() + ")");

        function on_open_event(gobj) {
            return function() {
                gobj.gobj_send_event('EV_ON_OPEN', {url:url}, gobj);
            };
        }
        function on_close_event(gobj) {
            return function() {
                gobj.gobj_send_event('EV_ON_CLOSE', {url:url}, gobj);
            };
        }
        function on_error_event(gobj) {
            return function() {
                gobj.gobj_send_event('EV_ON_CLOSE', {url:url}, gobj);
            };
        }
        function on_message_event(gobj) {
            return function(e) {
                gobj.gobj_send_event(
                    'EV_ON_MESSAGE',
                    {
                        url:url,
                        data:e.data
                    },
                    gobj
                );
            };
        }
        let websocket;
        try {
            websocket = new WebSocket(url);
            if (!websocket) {
                log_error(self.gobj_short_name() + ": Cannot open WebSocket to '" + url + "'");
                return;
            }
        } catch (e) {
            log_error(self.gobj_short_name() + ": Cannot open WebSocket to '" + url + "'");
            return;
        }

        websocket.onopen = on_open_event(self);
        websocket.onclose = on_close_event(self);
        websocket.onerror = on_error_event(self);
        websocket.onmessage = on_message_event(self);
        return websocket;
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
        var iev = new InterEvent(
            event,
            kw
        );

        return iev;
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
        var event = x['event'];
        if(!event) {
            log_error("parsing inter_event: no event");
            return null;
        }
        if(typeof event !== 'string') {
            log_error("parsing inter_event: event not a string");
            return null;
        }

        var kw = x['kw'];
        if(!kw) {
            log_error("parsing inter_kw: no kw");
            return null;
        }
        if(!(kw instanceof Object)) {
            log_error("parsing inter_event: kw not a json object");
            return null;
        }

        var iev = new InterEvent(
            event,
            kw
        );
        return iev;
    }

    /********************************************
     *  Close websocket
     ********************************************/
    function close_websocket(self)
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




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *
     ********************************************/
    function ac_on_open(self, event, kw, src)
    {
        var url = self.config.urls[self.config.idx_url];
        log_debug('Websocket opened: ' + url); // TODO que no se vea en prod
        send_identity_card(self);
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_on_close(self, event, kw, src)
    {
        var url = self.config.urls[self.config.idx_url];
        log_debug('Websocket closed: ' + url); // TODO que no se vea en prod

        if(self.websocket) {
            try {
                if(self.websocket.close) {
                    self.websocket.close();
                } else if(self.websocket.websocket.close) {
                    self.websocket.websocket.close();
                }
            } catch (e) {
                log_error(self.gobj_short_name() + ": ac_on_close(): " + e);
            }
            self.websocket = null;
        }

        if(self.inform_on_close) {
            self.inform_on_close = false;
            // Any interesting information for on_close event?
            self.gobj_publish_event(
                'EV_ON_CLOSE',
                {
                    url: url,
                    remote_yuno_name: self.config.remote_yuno_name,
                    remote_yuno_role: self.config.remote_yuno_role,
                    remote_yuno_service: self.config.remote_yuno_service
                }
            );
        }

        if(self.gobj_is_running()) {
            // point to next url
            var ln = self.config.urls.length;
            self.config.idx_url = (++self.config.idx_url) % ln;
            self.set_timeout(self.config.timeout_retry*1000);
        }

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_timeout_disconnected(self, event, kw, src)
    {
        close_websocket(self);
        if(self.gobj_is_running()) {
            self.websocket = setup_websocket(self);
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_identity_card_ack(self, event, kw, src)
    {
        /*---------------------------------------*
         *  Clear timeout
         *---------------------------------------*/
        self.clear_timeout();

        /*---------------------------------------*
         *  Update remote values
         *---------------------------------------*/
        /*
         *  Here is the end point of the request.
         *  Don't pop the request, because
         *  the event can be publish to serveral users.
         */
        /*
         *      __ANSWER__ __MESSAGE__
         */
        var request = msg_iev_get_stack(kw, IEVENT_MESSAGE_AREA_ID);
        var src_yuno = kw_get_str(request, "src_yuno", "");
        var src_role = kw_get_str(request, "src_role", "");
        var src_service = kw_get_str(request, "src_service", "");

        var result = kw_get_int(kw, "result", -1);
        if(result < 0) {
            close_websocket(self);
            self.gobj_publish_event(
                'EV_IDENTITY_CARD_REFUSED',
                {
                    url: self.config.urls[self.config.idx_url],
                    result: result,
                    remote_yuno_name: src_yuno,
                    remote_yuno_role: src_role,
                    remote_yuno_service: src_service
                }
            );
        } else {
            var services_roles = kw_get_dict_value(kw, "services_roles", {});
            var data = kw_get_dict_value(kw, "data", null);

            self.config.remote_yuno_role = src_role;
            self.config.remote_yuno_name = src_yuno;
            self.config.remote_yuno_service = src_service;

            self.gobj_change_state("ST_SESSION");
            self.config.inside_on_open = true;

            if(!self.inform_on_close) {
                self.inform_on_close = true;
                self.gobj_publish_event(
                    'EV_ON_OPEN',
                    {
                        url: self.config.urls[self.config.idx_url],
                        remote_yuno_name: src_yuno,
                        remote_yuno_role: src_role,
                        remote_yuno_service: src_service,
                        services_roles: services_roles,
                        data: data
                    }
                );
            }
            self.config.inside_on_open = false;

            /*
             *  Resend subscriptions
             */
            self.resend_subscriptions();
        }

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_timeout_wait_idAck(self, event, kw, src)
    {
        send_identity_card(self);
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_on_message(self, event, kw, src)
    {
        let url = self.config.urls[self.config.idx_url];

        /*------------------------------------------*
         *  Create inter_event from received data
         *------------------------------------------*/
        let size = kw.data.length;
        let iev_msg = iev_create_from_json(self, kw.data);

        /*---------------------------------------*
         *          trace inter_event
         *---------------------------------------*/
        if (self.yuno.config.trace_inter_event) {
            let prefix = self.yuno.yuno_name + ' <== ' + url;
            if(self.yuno.config.trace_ievent_callback) {
                self.yuno.config.trace_ievent_callback(prefix, iev_msg, 2, size);
            } else {
                trace_inter_event(self, prefix, iev_msg);
            }
        }

        /*----------------------------------------*
         *
         *----------------------------------------*/
        let iev_event = iev_msg.event;
        let iev_kw = iev_msg.kw;

        /*-----------------------------------------*
         *  If state is not SESSION send self.
         *  Mainly process EV_IDENTITY_CARD_ACK
         *-----------------------------------------*/
        if(!self.gobj_in_this_state("ST_SESSION")) {
            if(self.gobj_event_in_input_event_list(iev_event)) {
                self.gobj_send_event(iev_event, iev_kw, self);
            } else {
                log_error("ignoring event: " + iev_event + " for " + self.name);
            }
            return 0;
        }

        /*------------------------------------*
         *   Analyze inter_event
         *------------------------------------*/
        let msg_type = msg_get_msg_type(iev_kw);

        /*----------------------------------------*
         *  Pop inter-event routing information.
         *----------------------------------------*/
        let event_id = msg_iev_get_stack(iev_kw, IEVENT_MESSAGE_AREA_ID);
        let dst_service = kw_get_str(event_id, "dst_service", "");
        // Chequea tb el nombre TODO
        let dst_role = kw_get_str(event_id, "dst_role", "");

        if(dst_role != self.yuno.yuno_role) {
            log_error("It's not my role, yuno_role: " + dst_role + ", my_role: " + self.yuno.yuno_role);
            return 0;
        }

        /*------------------------------------*
         *   Is the event a subscription?
         *------------------------------------*/
        if(msg_type == '__subscribing__') {
            /*
             *  it's a external subscription
             */
            // TODO subscription
            return 0;
        }

        /*---------------------------------------*
         *   Is the event is a unsubscription?
         *---------------------------------------*/
        if(msg_type == '__unsubscribing__') {
            /*
             *  it's a external unsubscription
             */
            // TODO unsubscription
            return 0;
        }

        /*-------------------------------------------------------*
         *  Filter public events of this gobj
         *-------------------------------------------------------*/
        if(self.gobj_event_in_input_event_list(iev_event)) {
            self.gobj_send_event(iev_event, iev_kw, self);
            return 0;
        }

        /*-------------------------*
         *  Dispatch the event
         *-------------------------*/
        // 4 Dic 2022, WARNING until 6.2.2 version was used gobj_find_unique_gobj(),
        // improving security: only gobj services must be accessed externally,
        // may happen collateral damages
        let gobj_service = self.yuno.gobj_find_service(dst_service);
        if(gobj_service) {
            if(gobj_service.gobj_event_in_input_event_list(iev_event)) {
                gobj_service.gobj_send_event(iev_event, iev_kw, self);
            } else {
                log_error(gobj_service.gobj_short_name() + ": event '" + iev_event + "' not in input event list");
            }
        } else {
            self.gobj_publish_event( /* NOTE original behaviour */
                iev_event,
                iev_kw
            );
        }

        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    var FSM = {
        "event_list": [
            "EV_ON_MESSAGE",
            "EV_IDENTITY_CARD_ACK",
            "EV_ON_OPEN",
            "EV_ON_CLOSE",
            "EV_TIMEOUT"
        ],
        "state_list": [
            "ST_DISCONNECTED",
            "ST_WAIT_IDENTITY_CARD_ACK",
            "ST_SESSION"
        ],
        "machine": {
            "ST_DISCONNECTED":
            [
                ["EV_ON_OPEN",              ac_on_open,             "ST_WAIT_IDENTITY_CARD_ACK"],
                ["EV_ON_CLOSE",             ac_on_close,            undefined],
                ["EV_TIMEOUT",              ac_timeout_disconnected,undefined]
            ],
            "ST_WAIT_IDENTITY_CARD_ACK":
            [
                ["EV_ON_MESSAGE",           ac_on_message,          undefined],
                ["EV_IDENTITY_CARD_ACK",    ac_identity_card_ack,   undefined],
                ["EV_ON_CLOSE",             ac_on_close,            "ST_DISCONNECTED"],
                ["EV_TIMEOUT",              ac_timeout_wait_idAck,  undefined]
            ],
            "ST_SESSION":
            [
                ["EV_ON_MESSAGE",           ac_on_message,          undefined],
                ["EV_ON_CLOSE",             ac_on_close,            "ST_DISCONNECTED"]
            ]
        }
    };

    var IEvent = GObj.__makeSubclass__();
    var proto = IEvent.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "IEvent",
            kw,
            gcflag_no_check_output_events
        );
        return this;
    };




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        var self = this;
        self.config._wanted_yuno_name = self.config.remote_yuno_name;
        self.config._wanted_yuno_role = self.config.remote_yuno_role;
        self.config._wanted_yuno_service = self.config.remote_yuno_service;
    };

    /************************************************
     *      Framework Method destroy
     ************************************************/
    proto.mt_destroy = function()
    {
    };

    /************************************************
     *      Framework Method start
     ************************************************/
    proto.mt_start = function(kw)
    {
        var self = this;

        self.config.idx_url = 0;
        msg_iev_add_answer_filter(self, IEVENT_MESSAGE_AREA_ID, ievent_answer_filter);
        self.websocket = setup_websocket(self);
    };

    /************************************************
     *      Framework Method stop
     ************************************************/
    proto.mt_stop = function(kw)
    {
        var self = this;
        close_websocket(self);
    };

    /************************************************
     *      Framework Method stats
     ************************************************/
    proto.mt_stats = function(stats, kw, src)
    {
        let self = this;
        if(self.gobj_current_state() != "ST_SESSION") {
            return self.gobj_build_webix_answer(
                self,
                -1,
                self.config.remote_yuno_role + "^" + self.config.remote_yuno_name + " not in session.",
                null,
                null,
                kw
            );
        }

        if(!kw) {
            kw = {};
        }

        /*
         *      __REQUEST__ __MESSAGE__
         */
        let jn_ievent_id = build_ievent_request(
            self,
            src.name,
            kw.service?kw.service:null
        );
        msg_iev_push_stack(
            kw,         // not owned
            IEVENT_MESSAGE_AREA_ID,
            jn_ievent_id   // owned
        );

        kw["__stats__"] = stats;
        msg_iev_write_key(
            kw,         // not owned
            "__stats__",
            stats   // owned
        );

        return send_static_iev(self, "EV_MT_STATS", kw);
    };

    /************************************************
     *      Framework Method command
     ************************************************/
    proto.mt_command = function(command, kw, src)
    {
        let self = this;
        if(self.gobj_current_state() != "ST_SESSION") {
            return self.gobj_build_webix_answer(
                self,
                -1,
                self.config.remote_yuno_role + "^" + self.config.remote_yuno_name + " not in session.",
                null,
                null,
                kw
            );
        }

        if(!kw) {
            kw = {};
        }

        /*
         *      __REQUEST__ __MESSAGE__
         */
        let jn_ievent_id = build_ievent_request(
            self,
            src.name,
            kw.service?kw.service:null
        );
        msg_iev_push_stack(
            kw,         // not owned
            IEVENT_MESSAGE_AREA_ID,
            jn_ievent_id   // owned
        );

        kw["__command__"] = command;
        msg_iev_write_key(
            kw,         // not owned
            "__command__",
            command   // owned
        );

        return send_static_iev(self, "EV_MT_COMMAND", kw);
    };

    /************************************************
     *      Framework Method inject_event
     ************************************************/
    proto.mt_inject_event = function(event, kw, src)
    {
        let self = this;
        if(self.gobj_current_state() != "ST_SESSION") {
            log_error("Not in session, ignore event: " + event);
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
             *  Pon el ievent si no viene con él,
             *  si lo trae es que será alguna respuesta/redirección
             */
            let __service__ = null;
            if(kw_has_key(kw, "__service__")) {
                __service__ = kw.__service__;
                delete kw.__service__;
            }
            let jn_ievent_id = build_ievent_request(
                self,
                src.name,
                __service__
            );
            msg_iev_push_stack(
                kw,         // not owned
                IEVENT_MESSAGE_AREA_ID,
                jn_ievent_id   // owned
            );
        }
        return send_static_iev(self, event, kw);
    };

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
    proto.send_remote_subscription = function(subs)
    {
        if(empty_string(subs.event)) {
            // HACK only resend explicit subscriptions
            return;
        }

        var kw = {};

        var __global__ = subs.__global__;
        var __config__ = subs.__config__;
        var __filter__ = subs.__filter__;

        if(__config__) {
            kw["__config__"] = __duplicate__(__config__);
        }
        if(__global__) {
            kw["__global__"] = __duplicate__(__global__);
        }
        if(__filter__) {
            kw["__filter__"] = __duplicate__(__filter__);
        }

        msg_set_msg_type(kw, "__subscribing__");

        /*
         *      __MESSAGE__
         */
        var __service__ = subs.__service__?subs.__service__:null;
        var jn_ievent_id = build_ievent_request(
            this,
            subs.subscriber.name,
            __service__
        );
        msg_iev_push_stack(
            kw,         // not owned
            IEVENT_MESSAGE_AREA_ID,
            jn_ievent_id   // owned
        );

        send_static_iev(this, subs.event, kw);
    };

    /************************************************
     *      Framework Method subscription_added
     ************************************************/
    proto.mt_subscription_added = function(subs)
    {
        // TODO y si quiero subscribirme a los eventos internos de ievent?

        if(!this.gobj_in_this_state("ST_SESSION")) {
            // on_open will send all subscriptions
            return;
        }
        if(this.config.inside_on_open) {
            // avoid duplicates of subscriptions
            return;
        }
        this.send_remote_subscription(subs);
    };

    /************************************************
     *      Framework Method subscription_deleted
     ************************************************/
    proto.mt_subscription_deleted = function(subs)
    {
        if(!this.gobj_in_this_state("ST_SESSION")) {
            // Nothing to do. On open this subscription will be not sent.
            return;
        }

        if(empty_string(subs.event)) {
            // HACK only resend explicit subscriptions
            return;
        }

        var kw = {};

        var __global__ = subs.__global__;
        var __config__ = subs.__config__;
        var __filter__ = subs.__filter__;

        if(__config__) {
            kw["__config__"] = __duplicate__(__config__);
        }
        if(__global__) {
            kw["__global__"] = __duplicate__(__global__);
        }
        if(__filter__) {
            kw["__filter__"] = __duplicate__(__filter__);
        }

        msg_set_msg_type(kw, "__unsubscribing__");

        /*
         *      __MESSAGE__
         */
        var __service__ = subs.__service__?subs.__service__:null;
        var jn_ievent_id = build_ievent_request(
            this,
            subs.subscriber.name,
            __service__
        );
        msg_iev_push_stack(
            kw,         // not owned
            IEVENT_MESSAGE_AREA_ID,
            jn_ievent_id   // owned
        );

        send_static_iev(this, subs.event, kw);
    };

    /**************************************
     *
     **************************************/
    proto.resend_subscriptions = function()
    {
        var dl_subs = this.gobj_find_subscriptions();
        for(var i=0; i<dl_subs.length; i++) {
            var subs = dl_subs[i];
            this.send_remote_subscription(subs);
        }
    };

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.IEvent = IEvent;
})(this);
