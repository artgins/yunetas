/***********************************************************************
 *          yuneta_gui.js
 *
 *          Yuneta GUI
 *
 *          This is the main gobj (__default_service__): create all other services
 *
 *          Copyright (c) 2022 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    "use strict";

    /********************************************
     *      Configuration (C attributes)
     ********************************************/
    let CONFIG = {
        locales: null,
        username: null,

        remote_yuno_role: "controlcenter",
        remote_yuno_service: "controlcenter",
        required_services: ["treedb_controlcenter", "treedb_authzs"], // HACK add others needed services

        header_size: 40,

        url: null,
        urls: urls,     // HACK urls is a global variable from urls.js,

        gobj_login: null,
        gobj_ka_main: null,
        gobj_ui_header: null,
        gobj_mw_work_area: null,
        gobj_gf_agents: null
    };




            /***************************
             *      Local Methods
             ***************************/




    /********************************************
     *  Load i18n
     ********************************************/
    function setup_locale(self)
    {
        let locale = kw_get_local_storage_value("locale", "es", true);

        if(!self.config.locales[locale]) {
            log_error("locale UNKNOWN: " + locale);
            locale = "es";
            kw_set_local_storage_value("locale", locale);
        }

        i18next.init(
            {
                lng: locale,
                debug: false,
                resources: self.config.locales
            },
            function(err, t) {
                // initialized and ready to go!
            }
        );

        switch(locale) {
            case "en":
            case "es":
                luxon.Settings.defaultLocale = locale;
                break;
            default:
                log_error("locale UNKNOWN: " + locale);
                break;
        }
    }

    /********************************************
     *
     ********************************************/
    function build_remote_service(self)
    {
        if((window.location.hostname.indexOf("localhost")>=0 ||
                window.location.hostname.indexOf("127.")>=0) ||
                empty_string(window.location.hostname)) {
            self.config.url = "localhost";
        } else {
            self.config.url = window.location.hostname;
        }

        // HACK punto gatillo: del fichero urls.js recupera la conexión ws/wss asociada a la url location.hostname
        let url = self.config.urls[self.config.url];
        if(empty_string(url)) {
            let msg = t("no url available for") + ":\n" + self.config.url;
            log_error(msg);
            display_error_message(
                msg,
                "Error",
                function() {
                    setTimeout(function() {
                        close_all(self);
                    }, 100);
                }
            );
            return;
        }

        __yuno__.__remote_service__ = self.yuno.gobj_create_unique(
            "iev_client",
            IEvent,
            {
                remote_yuno_role: self.config.remote_yuno_role,
                remote_yuno_service: self.config.remote_yuno_service,
                required_services: self.config.required_services,
                jwt: null,
                urls: [url]
            },
            __yuno__ // remote_service is child of yuno: avoid to start it with self.gobj_start_tree()
        );
        /*
         *  Subscribe to IEvent null, to receive all events of IEvent
         *      EV_ON_OPEN
         *      EV_ON_CLOSE
         *      EV_IDENTITY_CARD_REFUSED
         */
        __yuno__.__remote_service__.gobj_subscribe_event(
            null,
            {
            },
            self
        );
    }

    /********************************************
     *
     ********************************************/
    function do_connect(self, jwt)
    {
        __yuno__.__remote_service__.gobj_write_attr("jwt", jwt);

        /*
         *  Start
         */
        __yuno__.__remote_service__.gobj_start_tree();
    }

    /********************************************
     *
     ********************************************/
    function close_all(self)
    {
        if(__yuno__.__remote_service__) {
            __yuno__.__remote_service__.gobj_stop_tree();
        }
        if(self.config.gobj_login) {
            self.config.gobj_login.gobj_send_event("EV_DO_LOGOUT", {}, self);
        }
    }

    /********************************************
     *
     ********************************************/
    function build_ui(self)
    {
        self.config.gobj_login = self.yuno.gobj_create_service(
            "__login__",
            Login,
            {
            },
            self
        );

        let gobj_ka_main = self.config.gobj_ka_main = self.yuno.gobj_create_service(
            "__ka_main__",
            Ka_main,
            {
            },
            self
        );

        self.config.gobj_ui_header = self.yuno.gobj_create(
            "ui_header",
            Ui_header,
            {
                layer: gobj_ka_main.get_static_layer(),
                x: 0,
                y: 0,
                width: gobj_ka_main.config.width,
                height: self.config.header_size,
                icon_size: 30,
                text_size: 20
            },
            self
        );
        gobj_ka_main.get_static_layer().draw();

        self.config.gobj_mw_work_area = self.yuno.gobj_create(
            "mw_work_area",
            Sw_multiview,
            {
                layer: gobj_ka_main.get_main_layer(),
                x: 0,
                y: self.config.header_size,
                width: gobj_ka_main.config.width,
                height: gobj_ka_main.config.height - self.config.header_size
            },
            self
        );

        self.config.gobj_gf_agents = self.yuno.gobj_create_service(
            "gf_agents",
            Gf_agent,
            {
                layer: gobj_ka_main.get_main_layer(),
                background_color: "#222B45",    // Uncomment to black theme
                draggable: false,  // TODO pon a false en prod
                gobj_mw_work_area: self.config.gobj_mw_work_area
            },
            self
        );

        //gobj_ka_main.get_main_layer().draw();

        if(0) { // TODO TEST
            self.yuno.gobj_create(
                "test",
                Ka_button,
                {
                    id: "Pepe",
                    action: "EV_TEST",
                    x: 200,
                    y: 250,
                    height: 350,
                    width: 350,
                    label: "Hola cómo estás?\nBien, gracias",
                    icon: "\u{EA01}",
                    icon_position: "left",
                    draggable: true,
                    quick_display: false,
                    autosize: true,
                    background_color: "white",

                    kw_border_shape: {
                        strokeWidth: 2
                    },
                    kw_text_font_properties: {
                    },
                    kw_icon_font_properties: {
                    }
                },
                self.config.gobj_gf_agents
            );

            let icons = "";
            for(let i=0; i<60; i++) {
                let icon = sprintf("%c", yuneta_icon_font["chevron-right"]);
                icons += icon;
            }
            self.yuno.gobj_create(
                "test",
                Ka_button,
                {
                    layer: gobj_ka_main.get_static_layer(),
                    action: "EV_TEST",
                    x: 100,
                    y: 100,
                    height: 50,
                    width: 150,
                    // label: "Hola cómo estás?\nBien, gracias",
                    icon: icons,
                    icon_position: "left",
                    draggable: true, // TODO quita
                    quick_display: true, // TODO quita
                    autosize: true,

                    kw_text_font_properties: {
                    },
                    kw_icon_font_properties: {
                    },
                    kw_border_shape: { /* Border shape */
                        strokeWidth: 1
                    }
                },
                self.config.gobj_gf_agents
            );

            icons = "";
            for(let c in yuneta_icon_font) {
                let icon = sprintf("%c", yuneta_icon_font[c]);
                icons += icon;
            }
            for(let c in yuneta_icon_font) {
                let icon = sprintf("%c", yuneta_icon_font[c]);
                icons += icon;
            }
            for(let c in yuneta_icon_font) {
                let icon = sprintf("%c", yuneta_icon_font[c]);
                icons += icon;
            }
            for(let c in yuneta_icon_font) {
                let icon = sprintf("%c", yuneta_icon_font[c]);
                icons += icon;
            }
            for(let c in yuneta_icon_font) {
                let icon = sprintf("%c", yuneta_icon_font[c]);
                icons += icon;
            }
            for(let c in yuneta_icon_font) {
                let icon = sprintf("%c", yuneta_icon_font[c]);
                icons += icon;
            }

            self.yuno.gobj_create(
                "test",
                Ka_button,
                {
                    layer: gobj_ka_main.get_static_layer(),
                    action: "EV_TEST",
                    x: 100,
                    y: 200,
                    height: 50,
                    width: 150,
                    // label: "Hola cómo estás?\nBien, gracias",
                    icon: icons,
                    icon_position: "left",
                    draggable: true, // TODO quita
                    quick_display: true, // TODO quita
                    autosize: true,

                    kw_text_font_properties: {
                    },
                    kw_icon_font_properties: {
                    },
                    kw_border_shape: { /* Border shape */
                        strokeWidth: 1
                    }
                },
                self.config.gobj_gf_agents
            );
            gobj_ka_main.get_main_layer().draw();
        }
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *      Connected to yuneta
     *
     *  Example of kw (connection data of __remote_service__):
         {
             remote_yuno_name: "mlsol",
             remote_yuno_role: "controlcenter",
             remote_yuno_service: "wss-1",
             services_roles: {
                 controlcenter: ["root"],
                 treedb_authzs: ["root"],
                 treedb_controlcenter: ["root"]
             },
             url: "wss://localhost:1911"
         }
     *
     ********************************************/
    function ac_on_open(self, event, kw, src)
    {
        /*----------------------------------------*
         *      Save backend roles
         *----------------------------------------*/
        __yuno__["services_roles"] = kw.services_roles;

        self.config.gobj_ui_header.gobj_send_event("EV_INFO_CONNECTED", kw, self);
        self.config.gobj_gf_agents.gobj_send_event("EV_CONNECTED", {}, self);

        return 0;
    }

    /********************************************
     *  Disconnected from yuneta
     ********************************************/
    function ac_on_close(self, event, kw, src)
    {
        self.config.gobj_ui_header.gobj_send_event("EV_INFO_DISCONNECTED", {}, self);
        self.config.gobj_gf_agents.gobj_send_event("EV_DISCONNECTED", {}, self);

        self.clear_timeout();

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_login_accepted(self, event, kw, src)
    {
        self.config.username = kw.username;
        let jwt = kw.jwt;

        /*---------------------------*
         *  Get roles
         *---------------------------*/
        let developer = kw_get_local_storage_value("developer", "false", false);
        developer = true; // TODO quita cuando esté en opción de menú
        __yuno__["developer"] = developer;

        /*---------------------------*
         *  Opciones de developer
         *---------------------------*/
        if(__yuno__.developer) {
//             $$("bottom_toolbar").addView({
//                 view: "button",
//                 autoWidth: true,
//                 maxWidth: 40,
//                 value: "T",
//                 tooltip: "Traffic",
//                 click: function() {
//                     trace_traffic();
//                 }
//             });
//             $$("bottom_toolbar").addView({
//                 view: "button",
//                 autoWidth: true,
//                 maxWidth: 40,
//                 value: "A",
//                 tooltip: "Automata",
//                 click: function() {
//                     trace_automata();
//                 }
//             });
//             $$("bottom_toolbar").addView({
//                 view: "button",
//                 autoWidth: true,
//                 maxWidth: 40,
//                 value: "NP",
//                 tooltip: "No Poll",
//                 click: function() {
//                     set_no_poll();
//                 }
//             });
        }

        /*-----------------------------------------------------------*
         *  With login done it's time to connect to Yuneta backend
         *-----------------------------------------------------------*/
        if(json_object_size(self.config.urls)===0) {
            display_error_message(
                t("no yuneta backend url available"),
                "Error",
                function() {
                    close_all(self);
                }
            );
        } else {
            do_connect(self, jwt);
        }

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_login_denied(self, event, kw, src)
    {
        close_all(self);
        return 0;
    }

    /********************************************
     *  Refused identity_card
     ********************************************/
    function ac_id_refused(self, event, kw, src)
    {
        close_all(self);
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_login_refreshed(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_logout_done(self, event, kw, src)
    {
        close_all(self);
        return 0;
    }

    /********************************************
     *  We are subscribed to __gobj_ka_main__
     *  We manage spaces
     ********************************************/
    function ac_resize(self, event, kw, src)
    {
        if(self.config.gobj_ui_header) {
            self.config.gobj_ui_header.gobj_send_event(
                event,
                {
                    x: 0,
                    y: 0,
                    height: self.config.header_size,
                    width: kw.width,
                },
                self
            );
        }
        if(self.config.gobj_mw_work_area) {
            self.config.gobj_mw_work_area.gobj_send_event(
                event,
                {
                    x: 0,
                    y: self.config.header_size,
                    width: kw.width,
                    height: kw.height - self.config.header_size
                },
                self
            );
        }

        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    let FSM = {
        "event_list": [
            "EV_ON_OPEN",
            "EV_ON_CLOSE",
            "EV_IDENTITY_CARD_REFUSED",
            "EV_LOGIN_ACCEPTED",
            "EV_LOGIN_REFRESHED",
            "EV_LOGIN_DENIED",
            "EV_LOGOUT_DONE",
            "EV_RESIZE"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_ON_OPEN",                  ac_on_open,             undefined],
                ["EV_ON_CLOSE",                 ac_on_close,            undefined],
                ["EV_IDENTITY_CARD_REFUSED",    ac_id_refused,          undefined],
                ["EV_LOGIN_ACCEPTED",           ac_login_accepted,      undefined],
                ["EV_LOGIN_DENIED",             ac_login_denied,        undefined],
                ["EV_LOGIN_REFRESHED",          ac_login_refreshed,     undefined],
                ["EV_LOGOUT_DONE",              ac_logout_done,         undefined],
                ["EV_RESIZE",                   ac_resize,              undefined]
            ]
        }
    };

    let Yuneta_gui = GObj.__makeSubclass__();
    let proto = Yuneta_gui.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Yuneta_gui",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Yuneta_gui, "Yuneta_gui");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        let self = this;

        setup_locale(self);
        build_remote_service(self);
        build_ui(self);
        //setup_dev();
    };

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
    };

    /************************************************
     *      Framework Method start
     ************************************************/
    proto.mt_start = function(kw)
    {
        let self = this;
        self.gobj_start_tree();
    };

    /************************************************
     *      Framework Method stop
     ************************************************/
    proto.mt_stop = function(kw)
    {
        let self = this;
    };


    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Yuneta_gui = Yuneta_gui;

})(this);
