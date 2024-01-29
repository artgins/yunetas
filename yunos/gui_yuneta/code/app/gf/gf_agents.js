/***********************************************************************
 *          gf_agents.js
 *
 *          Grafo principal de la GPU
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    "use strict";

    /********************************************
     *      Configuration (C attributes)
     ********************************************/
    let CONFIG = {
        //////////////// Public Attributes //////////////////
        subscriber: null,       // subscriber of publishing messages (Child model: if null will be the parent)
        layer: null,            // Konva layer

        timeout_polling_agents: 60, // in seconds

        gobj_mw_work_area: null,    // Multiview area, from parent
        gobj_gf_view: null,         // This graph view

        background_color: "#cccccc",
        draggable: false,

        gobj_node_digital_inputs: null,
        gobj_node_digital_outputs: null,
        gobj_node_control: null,
        gobj_node_bms: null,
        gobj_node_inversor: null,

        btn_reset: null,
        btn_start_bms: null,
        btn_stop_bms: null,
        btn_start_inversor: null,
        btn_stop_inversor: null
    };




            /***************************
             *      Local Methods
             ***************************/




    /********************************************
     *
     ********************************************/
    function build_ui(self, gobj_gf_view)
    {
    }

    /********************************************
     *
     ********************************************/
    function get_agents_info(self)
    {
        __yuno__.__remote_service__.gobj_command("list-agents", {"expand": true}, self);
    }

    /********************************************
     *  List of agents, i.e. nodes (machines)
     ********************************************/
    function process_list_agents(self, data)
    {
        for(let i=0; i<data.length; i++)  {
            let agent = data[i];

            let agent_id = kw_get_str(agent, "id", "", false, true);
            let agent_name = kw_get_str(agent, "yuno_name", "", false, true);
            let yuneta_version = kw_get_str(agent, "yuneta_version", "", false, true);
            let id = agent_id + "-" + agent_name;

            // log_warning("agent ==> " + id);   // TODO TEST
            // console.dir(agent); // TODO TEST

            let parent = self.config.gobj_gf_agents;

            let gobj_machine_node = self.yuno.gobj_find_service(id);
            if(!gobj_machine_node) {
                gobj_machine_node = self.yuno.gobj_create_service(
                    id,
                    Ka_machine,
                    {
                        x: 20,
                        y: (60 + 20)*i + 20,
                        width: 280, // TODO sfd_persistent, recover
                        height: 60,
                        data: {
                            remote_service: __yuno__.__remote_service__,
                            agent_id: agent_id,
                            agent_name: agent_name,
                            yuneta_version: yuneta_version
                        }
                    },
                    parent
                );
                gobj_machine_node.gobj_start_tree();
                // continue;

                // TODO esto de abajo es para test
                i++;
                let gobj_machine_node_xx = self.yuno.gobj_create( // TODO TEST
                    "xx",
                    Ka_machine,
                    {
                        x: 20,
                        y: (60 + 20)*i + 20,
                        width: 280, // TODO sfd_persistent, recover
                        height: 60,
                        data: {
                            agent_id: agent_id,
                            agent_name: agent_name,
                            yuneta_version: yuneta_version
                        }
                    },
                    parent
                );

                i++;
                let gobj_machine_node_xxx = self.yuno.gobj_create( // TODO TEST
                    "xxx",
                    Ka_machine,
                    {
                        x: 20,
                        y: (60 + 20)*i + 20,
                        width: 280, // TODO sfd_persistent, recover
                        height: 60,
                        data: {
                            agent_id: agent_id,
                            agent_name: agent_name,
                            yuneta_version: yuneta_version
                        }
                    },
                    parent
                );

                /*
                    Una manera de crear los links sería con eventos,
                    quizá no sea la mejor manera porque dentro de la acción simplemente se crea el gobj link.

                    Mejor así? creados externamente, y preguntándole al gobj qué ports tiene
                    Un gobj que no tiene ports es que no soporta links !!!???

                        self.yuno.gobj_create(
                            id,
                            Ka_link,
                            {
                                draggable: true, // TODO a true cuando se quiera editar el link
                                source_gobj: source_gobj,
                                source_port: source_port,
                                target_gobj: target_gobj,
                                target_port: target_port
                            },
                            __links_root__
                        );

                 */
                gobj_machine_node.gobj_send_event(
                    "EV_LINK",
                    {
                        id: "",
                        target_port: null,
                        source_port: null,
                        source_gobj: gobj_machine_node_xx
                    },
                    self
                );
                gobj_machine_node.gobj_send_event(
                    "EV_LINK",
                    {
                        id: "",
                        target_port: null,
                        source_port: null,
                        source_gobj: gobj_machine_node_xxx
                    },
                    self
                );
            }
        }
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *  Yuneta backend connected
     ********************************************/
    function ac_connected(self, event, kw, src)
    {
        self.set_timeout(1000); // begin to get agent's info

        return 0;
    }

    /********************************************
     *  Yuneta backend disconnected
     ********************************************/
    function ac_disconnected(self, event, kw, src)
    {
        self.clear_timeout();

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_mt_command_answer(self, event, kw, src)
    {
        // console.dir(kw);
        let result = kw_get_int(kw, "result", -1, false, false);
        let comment = kw_get_str(kw, "comment", "", false, false);
        let data = kw_get_dict_value(kw, "data", null, false, false);
        let schema = kw_get_dict(kw, "schema", null, false, false);
        let command = msg_iev_read_key(kw, "__command__");

        if(result < 0) {
            display_error_message(sprintf("Failed to command '%s'", command), "Error");
            return -1;
        }

        switch(command) {
            case "list-agents":
                process_list_agents(self, data);
                break;
            case "command-agent":
                // Ignore, echo of remote command
                break;
            default:
                log_error(sprintf("command unknown: %s", command));
                break;
        }

        return 0;
    }

    /********************************************
     *  Timeout polling agents
     ********************************************/
    function ac_timeout(self, event, kw, src)
    {
        get_agents_info(self);
        self.set_timeout(self.config.timeout_polling_agents*1000);
        return 0;
    }

    /********************************************
     *  Child moving/moved
     *  Event from Button (subscriber is this)
     ********************************************/
    function ac_moving(self, event, kw, src)
    {
    }

    /********************************************
     *
     ********************************************/
    function ac_resize(self, event, kw, src)
    {
        return self.config.gobj_gf_view.gobj_send_event(event, kw, src);
    }




            /***************************
             *      GClass/Machine
             ***************************/




    let FSM = {
        "event_list": [
            "EV_CONNECTED",
            "EV_DISCONNECTED",
            "EV_MT_COMMAND_ANSWER",
            "EV_TIMEOUT",
            "EV_MOVING",
            "EV_MOVED",
            "EV_WINDOW_RESIZE"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_MT_COMMAND_ANSWER",    ac_mt_command_answer,   undefined],
                ["EV_CONNECTED",            ac_connected,           undefined],
                ["EV_DISCONNECTED",         ac_disconnected,        undefined],
                ["EV_TIMEOUT",              ac_timeout,             undefined],
                ["EV_MOVING",               ac_moving,              undefined],
                ["EV_MOVED",                ac_moving,              undefined],
                ["EV_WINDOW_RESIZE",        ac_resize,              undefined]
            ]
        }
    };

    let Gf_agent = GObj.__makeSubclass__();
    let proto = Gf_agent.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Gf_agent",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Gf_agent, "Gf_agent");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        let self = this;

        /*
         *  CHILD subscription model
         */
        let subscriber = self.gobj_read_attr("subscriber");
        if(!subscriber) {
            subscriber = self.gobj_parent();
        }
        self.gobj_subscribe_event(null, null, subscriber);

        if(!self.config.layer) {
            self.config.layer = self.gobj_parent().config.layer;
        }
        if(!self.config.gobj_mw_work_area) {
            log_error("gf_agent(): What multiview work area?");
        }

        /*----------------------------------------*
         *      Graph
         *----------------------------------------*/
        self.config.gobj_gf_view = self.yuno.gobj_create(
            self.name,
            Sw_graph,
            {
                layer: self.config.layer,
                background_color: self.config.background_color
            },
            self.config.gobj_mw_work_area
        );
        self.config.gobj_gf_view.gobj_start();

        build_ui(self, self.config.gobj_gf_view);
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
        // __yuno__.__remote_service__.gobj_subscribe_event(
        //     "EV_CONTROL_STATE",
        //     {
        //     },
        //     self
        // );
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
    exports.Gf_agent = Gf_agent;

})(this);
