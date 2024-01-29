/***********************************************************************
 *          ui_treedb_tables.js
 *
 *          Treedb table
 *          Gestión de los topics del treedb mediante formtables globales.
 *
 *          Mantiene actualizadas tablas globales
 *          de los `topics` del `treedb_name` configurados.
 *
 *          Copyright (c) 2021 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    "use strict";

    /********************************************
     *      Configuration (C attributes)
     ********************************************/
    var CONFIG = {
        /*
         *  Funciones que debe suministrar el padre
         */
        info_wait: function() {},
        info_no_wait: function() {},

        /*
         *  GClass Manager/Viewer of hook data
         */
        hook_data_viewer: null,

        /*
         *  gobj_remote_yuno: Remote yuno to ask data
         *  Must be a connected service
         */
        gobj_remote_yuno: null,
        treedb_name: null,
        topics: null,
        auto_topics: false,
        descs: null,

        default_formtable_properties: {
            with_checkbox: false,
            with_textfilter: true,
            with_sort: true,
            with_top_title: true,
            with_footer: true,
            with_navigation_toolbar: true,
            hide_private_fields: true,
            update_mode_enabled: true,
            create_mode_enabled: true,
            delete_mode_enabled: true
        },
        formtable_properties: {
        },

        __writable_attrs__: [
        ]
    };




            /***************************
             *      Local Methods
             ***************************/




    /********************************************
     *
     ********************************************/
    function get_gobj_formtable(self, topic_name)
    {
        for(var i=0; i<self.config.topics.length; i++) {
            var topic = self.config.topics[i];
            if(topic_name == topic.topic_name) {
                return topic.gobj_formtable;
            }
        }
        return null;
    }

    /********************************************
     *
     ********************************************/
    function treedb_nodes(self, treedb_name, topic_name, options)
    {
        if(!self.config.gobj_remote_yuno) {
            log_error(self.gobj_short_name() + ": No gobj_remote_yuno defined");
            return;
        }

        var command = "nodes";

        var kw = {
            service: treedb_name,
            treedb_name: treedb_name,
            topic_name: topic_name,
            options: options || {}
        };

        msg_iev_write_key(kw, "__topic_name__", topic_name);
        //msg_iev_write_key(kw, "__command__", command);

        self.config.info_wait();

        var ret = self.config.gobj_remote_yuno.gobj_command(
            command,
            kw,
            self
        );
        if(ret) {
            log_error(ret);
        }
    }

    /********************************************
     *
     ********************************************/
    function treedb_create_node(self, treedb_name, topic_name, record, options)
    {
        if(!self.config.gobj_remote_yuno) {
            log_error(self.gobj_short_name() + ": No gobj_remote_yuno defined");
            return;
        }

        var command = "create-node";

        var kw = {
            service: treedb_name,
            treedb_name: treedb_name,
            topic_name: topic_name,
            record: record,
            options: options || {}
        };

        msg_iev_write_key(kw, "__topic_name__", topic_name);
        //msg_iev_write_key(kw, "__command__", command);

        self.config.info_wait();

        var ret = self.config.gobj_remote_yuno.gobj_command(
            command,
            kw,
            self
        );
        if(ret) {
            log_error(ret);
        }
    }

    /********************************************
     *
     ********************************************/
    function treedb_update_node(self, treedb_name, topic_name, record, options)
    {
        if(!self.config.gobj_remote_yuno) {
            log_error(self.gobj_short_name() + ": No gobj_remote_yuno defined");
            return;
        }

        var command = "update-node";

        var kw = {
            service: treedb_name,
            treedb_name: treedb_name,
            topic_name: topic_name,
            record: record,
            options: options || {}
        };

        msg_iev_write_key(kw, "__topic_name__", topic_name);
        //msg_iev_write_key(kw, "__command__", command);

        self.config.info_wait();

        var ret = self.config.gobj_remote_yuno.gobj_command(
            command,
            kw,
            self
        );
        if(ret) {
            log_error(ret);
        }
    }

    /********************************************
     *
     ********************************************/
    function treedb_delete_node(self, treedb_name, topic_name, record, options)
    {
        if(!self.config.gobj_remote_yuno) {
            log_error(self.gobj_short_name() + ": No gobj_remote_yuno defined");
            return;
        }

        var command = "delete-node";

        var kw = {
            service: treedb_name,
            treedb_name: treedb_name,
            topic_name: topic_name,
            record: record,
            options: options || {}
        };

        msg_iev_write_key(kw, "__topic_name__", topic_name);
        //msg_iev_write_key(kw, "__command__", command);

        self.config.info_wait();

        var ret = self.config.gobj_remote_yuno.gobj_command(
            command,
            kw,
            self
        );
        if(ret) {
            log_error(ret);
        }
    }

    /************************************************************
     *  Update treedb tables options that have topic_name ref
     *
     *  reg {
     *      data: {
     *          "$topic_name": {
     *              "$id": {}
     *           }
     *      },
     *
     *      gobjs: {
     *          "$gobj_unique_name":  "$gobj"
     *      }
     *  }
     *
     ************************************************************/
    function update_options(self, updated_topic_name)
    {
        let data = treedb_get_topic_data(self.config.treedb_name, updated_topic_name);
        if(json_object_size(data)==0) {
            return;
        }

        /*
         *  Collect tables to update
         */
        let reg = treedb_get_register(self.config.treedb_name);
        let tables2update = {};
        for(let topic_name in self.config.descs) {
            let desc = self.config.descs[topic_name];
            let cols = desc.cols;
            for(let i=0; i<cols.length; i++) {
                let col = cols[i];
                if(kw_has_key(col, "fkey")) {
                    for(let k in col.fkey) {
                        if(k == updated_topic_name) {
                            let t = kw_get_dict_value(tables2update, topic_name, {}, true);
                            t[col.id] = true;
                            //trace_msg("update " + topic_name + ", fkey " + col.id);
                        }
                    }
                }
            }
        }

        for(let topic_name in tables2update) {
            let cols = tables2update[topic_name];
            for(let gobj_name in reg.gobjs) {
                let gobj = reg.gobjs[gobj_name];
                if(topic_name != gobj.gobj_read_attr("topic_name")) {
                    continue;
                }
                gobj.gobj_send_event(
                    "EV_UPDATE_OPTIONS",
                    {
                        // No hace falta, se reconstruye entero
                        //cols: cols,
                        //data: data
                    },
                    self
                );
            }
        }
    }

    /************************************************************
     *
     ************************************************************/
    function create_topics_formtable(self)
    {
        for(var i=0; i<self.config.topics.length; i++) {
            var topic = self.config.topics[i];
            var topic_name = topic.topic_name;
            var label = topic.label;
            if(!label) {
                label = topic.topic_name;
            }
            if(topic["gobj_formtable"]) {
                log_error("gobj_formtable ALREADY created: "+ topic_name);
                continue;
            }

            var kw_formtable = {
                subscriber: self,  // HACK get all output events

                ui_properties: {
                    hidden: true
                },

                treedb_name: self.config.treedb_name,
                topic_name: topic_name,
                cols: null, // later, when arriving data

                window_properties: {
                    without_window_fullscreen_btn: false,
                    without_window_close_btn: false,
                    without_destroy_window_on_close: true,
                    with_create_window_on_start: false
                },
                is_pinhold_window: true,
                window_title: label,
                window_image: "",
                width: 950,
                height: 600
            };
            json_object_update(kw_formtable, self.config.default_formtable_properties);
            json_object_update(kw_formtable, self.config.formtable_properties);

            topic["gobj_formtable"] = __yuno__.gobj_create_unique(
                build_name(self, topic_name + " formtable"),
                Ui_treedb_formtable,
                kw_formtable,
                self
            );
            if(topic["gobj_formtable"]) {
                topic["gobj_formtable"].gobj_start();
            }
        }
    }

    /********************************************
     *
     ********************************************/
    function treedb_descs(self)
    {
        if(!self.config.gobj_remote_yuno) {
            log_error(self.gobj_short_name() + ": No gobj_remote_yuno defined");
            return;
        }

        var command = "descs";

        var kw = {
            service: self.config.treedb_name,
            treedb_name: self.config.treedb_name
        };

        //msg_iev_write_key(kw, "__command__", command);

        self.config.info_wait();

        var ret = self.config.gobj_remote_yuno.gobj_command(
            command,
            kw,
            self
        );
        if(ret) {
            log_error(ret);
        }
    }

    /************************************************************
     *
     ************************************************************/
    function process_descs(self)
    {
        /*
         *  Auto topics
         */
        if(self.config.auto_topics) {
            self.config.topics = [];
            for(let topic_name in self.config.descs) {
                if(topic_name.substring(0, 2) == "__") {
                    continue;
                }
                self.config.topics.push({topic_name: topic_name});
            }
            create_topics_formtable(self);
        }

        /*
         *  Subcribe events
         */
        for(let i=0; i<self.config.topics.length; i++) {
            let topic_name = self.config.topics[i].topic_name;
            let desc = self.config.descs[topic_name];
            if(!desc) {
                log_error("topic desc unknown: " + topic_name);
                continue;
            }

            self.config.gobj_remote_yuno.gobj_subscribe_event(
                "EV_TREEDB_NODE_CREATED",
                {
                    __service__: self.config.treedb_name,
                    __filter__: {
                        "treedb_name": self.config.treedb_name,
                        "topic_name": topic_name
                    }
                },
                self
            );
            self.config.gobj_remote_yuno.gobj_subscribe_event(
                "EV_TREEDB_NODE_UPDATED",
                {
                    __service__: self.config.treedb_name,
                    __filter__: {
                        "treedb_name": self.config.treedb_name,
                        "topic_name": topic_name
                    }
                },
                self
            );
            self.config.gobj_remote_yuno.gobj_subscribe_event(
                "EV_TREEDB_NODE_DELETED",
                {
                    __service__: self.config.treedb_name,
                    __filter__: {
                        "treedb_name": self.config.treedb_name,
                        "topic_name": topic_name
                    }
                },
                self
            );
        }

        /*
         *  Get data
         */
        refresh_data(self);
    }

    /********************************************
     *
     ********************************************/
    function refresh_data(self)
    {
        let options = {
            list_dict: true
        };

        for(let i=0; i<self.config.topics.length; i++) {
            let topic = self.config.topics[i];
            let topic_name = topic.topic_name;

            topic.gobj_formtable.gobj_send_event(
                "EV_CLEAR_DATA",
                {
                },
                self
            );

            treedb_nodes(
                self,
                self.config.treedb_name,
                topic_name,
                options
            );
        }
    }

    /********************************************
     *
     ********************************************/
    function refresh_treedb(self)
    {
        refresh_data(self);
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *  Remote response
     ********************************************/
    function ac_mt_command_answer(self, event, kw, src)
    {
        var webix_msg = kw;

        self.config.info_no_wait();

        var result;
        var comment;
        var schema;
        var data;
        var __md_iev__;

        try {
            result = webix_msg.result;
            comment = webix_msg.comment;
            schema = webix_msg.schema;
            data = webix_msg.data;
            __md_iev__ = webix_msg.__md_iev__;
        } catch (e) {
            log_error(e);
            return;
        }
        if(result < 0) {
            info_user_warning(comment);
            // HACK don't return, pass errors when need it.
        } else {
            if(comment) {
                // log_info(comment); No pintes
            }
        }

        switch(__md_iev__.__command__) {
            case "descs":
                if(result >= 0) {
                    self.config.descs = data;
                    process_descs(self);
                }
                break;

            case "nodes":
                if(result >= 0) {
                    treedb_register_nodes(
                        self.config.treedb_name,
                        schema.topic_name,
                        data
                    );

                    var gobj_formtable = get_gobj_formtable(self, schema.topic_name);
                    gobj_formtable.gobj_send_event(
                        "EV_REBUILD_TABLE",
                        {
                            topic_name: schema.topic_name,
                            cols: schema.cols
                        },
                        self
                    );

                    update_options(self, schema.topic_name); // This before load data

                    gobj_formtable.gobj_send_event(
                        "EV_LOAD_DATA",
                        data,
                        self
                    );
                }
                break;

            case "create-node":
            case "update-node":
            case "delete-node":
                // Don't process by here, process on subscribed events.
                break;

            default:
                log_error(self.gobj_short_name() + " Command unknown: " + __md_iev__.__command__);
        }

        return 0;
    }

    /********************************************
     *  Remote subscription response
     ********************************************/
    function ac_treedb_node_created(self, event, kw, src)
    {
        var treedb_name = kw_get_str(kw, "treedb_name", "", 0);
        var topic_name = kw_get_str(kw, "topic_name", "", 0);
        var node = kw_get_dict_value(kw, "node", null, 0);

        if(treedb_name == self.config.treedb_name) {
            treedb_register_update_node(
                self.config.treedb_name,
                topic_name,
                node
            );

            update_options(self, topic_name); // This before load data

            var gobj_formtable = get_gobj_formtable(self, topic_name);
            gobj_formtable.gobj_send_event(
                "EV_LOAD_DATA",
                [node],
                self
            );
        }

        return 0;
    }

    /********************************************
     *  Remote subscription response
     ********************************************/
    function ac_treedb_node_updated(self, event, kw, src)
    {
        var treedb_name = kw_get_str(kw, "treedb_name", "", 0);
        var topic_name = kw_get_str(kw, "topic_name", "", 0);
        var node = kw_get_dict_value(kw, "node", null, 0);

        if(treedb_name == self.config.treedb_name) {
            treedb_register_update_node(
                self.config.treedb_name,
                topic_name,
                node
            );

            update_options(self, topic_name); // This before load data

            var gobj_formtable = get_gobj_formtable(self, topic_name);
            gobj_formtable.gobj_send_event(
                "EV_LOAD_DATA",
                [node],
                self
            );
        }

        return 0;
    }

    /********************************************
     *  Remote subscription response
     ********************************************/
    function ac_treedb_node_deleted(self, event, kw, src)
    {
        var treedb_name = kw_get_str(kw, "treedb_name", "", 0);
        var topic_name = kw_get_str(kw, "topic_name", "", 0);
        var node = kw_get_dict_value(kw, "node", null, 0);

        if(treedb_name == self.config.treedb_name) {
            treedb_register_del_node(
                self.config.treedb_name,
                topic_name,
                node
            );
            update_options(self, topic_name); // This before load data

            var gobj_formtable = get_gobj_formtable(self, topic_name);
            gobj_formtable.gobj_send_event(
                "EV_DELETE_DATA",
                node,
                self
            );
        }

        return 0;
    }

    /********************************************
     *  Event from formtable
     *  kw: {
     *      topic_name,
     *      record
     *  }
     ********************************************/
    function ac_create_record(self, event, kw, src)
    {
        var treedb_name = self.config.treedb_name;
        var topic_name = src.gobj_read_attr("topic_name");
        var record = kw.record;

        var options = {
            list_dict: true,
            create: true,
            autolink: true
        };

        return treedb_update_node( // HACK use the powerful update_node
            self,
            treedb_name,
            topic_name,
            record,
            options
        );
    }

    /********************************************
     *  Event from formtable
     *  kw: {
     *      topic_name,
     *      record
     *  }
     ********************************************/
    function ac_update_record(self, event, kw, src)
    {
        var treedb_name = self.config.treedb_name;
        var topic_name = src.gobj_read_attr("topic_name");
        var record = kw.record;

        var options = {
            list_dict: true,
            autolink: true
        };

        return treedb_update_node(
            self,
            treedb_name,
            topic_name,
            record,
            options
        );
    }

    /********************************************
     *  Event from formtable
     *  kw: {
     *      topic_name,
     *      record
     *  }
     ********************************************/
    function ac_delete_record(self, event, kw, src)
    {
        var treedb_name = self.config.treedb_name;
        var topic_name = src.gobj_read_attr("topic_name");
        var record = kw.record;
        var options = {
            force: true
        };

        return treedb_delete_node(
            self,
            treedb_name,
            topic_name,
            record,
            options
        );
    }

    /********************************************
     *  Event from formtable
     *  kw: {
     *      topic_name,
     *  }
     ********************************************/
    function ac_refresh_table(self, event, kw, src)
    {
        var options = {
            list_dict: true
        };

        treedb_nodes(
            self,
            self.config.treedb_name,
            kw.topic_name,
            options
        );

        return 0;
    }

    /********************************************
     *  Event from formtable
     *  kw: {
     *      treedb_name
     *      parent_topic_name,
     *      child_topic_name,
     *      child_field_name,
     *      child_field_value,
     *      x,
     *      y
     *  }
     ********************************************/
    function ac_show_hook_data(self, event, kw, src)
    {
        var treedb_name = kw.treedb_name;
        var parent_topic_name = kw.parent_topic_name;
        var child_topic_name = kw.child_topic_name;
        var child_field_name = kw.child_field_name;
        var child_field_value = kw.child_field_value;
        var click_x = kw.click_x;
        var click_y = kw.click_y;

        if(!self.config.hook_data_viewer) {
            trace_msg(kw);
            return 0;
        }

        var name = "Table Hook>" + treedb_name + ">" +
            parent_topic_name + ">" +
            child_topic_name + ">" +
            child_field_name + ">" +
            child_field_value;
        var gobj = __yuno__.gobj_find_unique_gobj(name);
        if(!gobj) {
            gobj = self.yuno.gobj_create_unique(
                name,
                self.config.hook_data_viewer,
                kw,
                self
            );
            gobj.gobj_start();
        } else {
            gobj.gobj_send_event("EV_TOGGLE", {}, self);
        }

        return 0;
    }

    /********************************************
     *  From formtable,
     *  when window is destroying or minififying
     *  kw
     *      treedb_name
     *      topic_name
     *      {destroying: true}   Window destroying
     *      {destroying: false}  Window minifying
     ********************************************/
    function ac_close_formtable(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *  Return gobj of topic's formtable
     ********************************************/
    function ac_get_topic_formtable(self, event, kw, src)
    {
        var topic_name = kw.topic_name;

        for(var i=0; i<self.config.topics.length; i++) {
            var topic = self.config.topics[i];
            if(topic_name == topic.topic_name) {
                return topic.gobj_formtable;
            }
        }
        return null;
    }

    /********************************************
     *
     ********************************************/
    function ac_select(self, event, kw, src)
    {

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_refresh(self, event, kw, src)
    {

        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    var FSM = {
        "event_list": [
            "EV_MT_COMMAND_ANSWER",
            "EV_TREEDB_NODE_CREATED",
            "EV_TREEDB_NODE_UPDATED",
            "EV_TREEDB_NODE_DELETED",
            "EV_CREATE_RECORD",
            "EV_UPDATE_RECORD",
            "EV_DELETE_RECORD",
            "EV_REFRESH_TABLE",
            "EV_SHOW_HOOK_DATA",
            "EV_CLOSE_WINDOW",
            "EV_GET_TOPIC_FORMTABLE",
            "EV_SELECT",
            "EV_REFRESH"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_MT_COMMAND_ANSWER",    ac_mt_command_answer,   undefined],
                ["EV_TREEDB_NODE_CREATED",  ac_treedb_node_created, undefined],
                ["EV_TREEDB_NODE_UPDATED",  ac_treedb_node_updated, undefined],
                ["EV_TREEDB_NODE_DELETED",  ac_treedb_node_deleted, undefined],
                ["EV_CREATE_RECORD",        ac_create_record,       undefined],
                ["EV_UPDATE_RECORD",        ac_update_record,       undefined],
                ["EV_DELETE_RECORD",        ac_delete_record,       undefined],
                ["EV_REFRESH_TABLE",        ac_refresh_table,       undefined],
                ["EV_SHOW_HOOK_DATA",       ac_show_hook_data,      undefined],
                ["EV_CLOSE_WINDOW",         ac_close_formtable,     undefined],
                ["EV_GET_TOPIC_FORMTABLE",  ac_get_topic_formtable, undefined],
                ["EV_SELECT",               ac_select,              undefined],
                ["EV_REFRESH",              ac_refresh,             undefined]
            ]
        }
    };

    var Ui_treedb_tables = GObj.__makeSubclass__();
    var proto = Ui_treedb_tables.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ui_treedb_tables",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Ui_treedb_tables, "Ui_treedb_tables");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        var self = this;

        if(!self.config.auto_topics) {
            create_topics_formtable(self);
        }
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
        var self = this;

        treedb_descs(self);
    };

    /************************************************
     *      Framework Method stop
     ************************************************/
    proto.mt_stop = function(kw)
    {
        var self = this;
    };


    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Ui_treedb_tables = Ui_treedb_tables;

})(this);
