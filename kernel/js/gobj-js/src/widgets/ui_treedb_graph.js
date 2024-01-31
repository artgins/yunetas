/***********************************************************************
 *          ui_treedb_graph.js
 *
 *          NOTE Wrapper on (Mix "Container Panel" & "Pinhold Window")
 *
 *          Manage treedb topics with mxgraph
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
        //////////////// WRAPPER Common Attributes //////////////////
        is_pinhold_window:false,// CONF: Select default: window or container panel
        panel_properties: {},   // CONF: creator can set "Container Panel" properties
        window_properties: {},  // CONF: creator can set "Pinhold Window" properties
        ui_properties: null,    // CONF: creator can set webix properties
        window_image: "",       // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        window_title: "",       // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        left: 0,                // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        top: 0,                 // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        width: 600,             // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"
        height: 500,            // CONF: Used by pinhold_window_top_toolbar "Pinhold Window"

        $ui: null,              // HACK $ui from wrapped window or panel
        subscriber: null,       // Subscriber of published events, by default the parent.

        //////////////// Particular Attributes //////////////////
        with_treedb_tables: false,
        auto_topics: false,
        topics_style: [],

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
        topics: [],
        descs: [],

        gobj_window: null,
        gobj_nodes_tree: null,
        gobj_treedb_tables: null,

        //////////////////////////////////
        __writable_attrs__: [
            ////// Common /////

            ////// Particular /////
        ]
    };




            /***************************
             *      Local Methods
             ***************************/




    /********************************************
     *
     ********************************************/
    function build_default_graph_topics_style(topics)
    {
        var default_styles = [
            {
                node:
                "html=1;strokeColor=DarkOrange;fillColor=#fff2cc;whiteSpace=wrap;shadow=0;strokeWidth=2;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",

                hook:
                "html=1;strokeColor=DarkOrange;fillColor=#fff2cc;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",

                fkey:
                "ellipse;html=1;strokeColor=DarkOrange;fillColor=#fff2cc;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=25;opacity=60;",

                arrow:
                "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=DarkOrange;"
            },
            {
                node:
                "ellipse;html=1;fillColor=#f8cecc;strokeColor=#b85450;whiteSpace=wrap;shadow=0;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",

                hook:
                "html=1;fillColor=#f8cecc;strokeColor=#b85450;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",

                fkey:
                "ellipse;html=1;fillColor=#f8cecc;strokeColor=#b85450;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",

                arrow:
                "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=#b85450;"
            },
            {
                node:
                "ellipse;html=1;strokeColor=#6c8ebf;fillColor=#dae8fc;whiteSpace=wrap;shadow=0;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",

                hook:
                "html=1;strokeColor=#6c8ebf;fillColor=#dae8fc;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",

                fkey:
                "ellipse;html=1;strokeColor=#6c8ebf;fillColor=#dae8fc;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",

                arrow:
                "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=#6c8ebf;"
            },
            {
                node:
                "ellipse;html=1;strokeColor=#82b366;fillColor=#d5e8d4;whiteSpace=wrap;shadow=0;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",

                hook:
                "html=1;strokeColor=#82b366;fillColor=#d5e8d4;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",

                fkey:
                "ellipse;html=1;strokeColor=#82b366;fillColor=#d5e8d4;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",

                arrow:
                "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=#82b366;"
            },
            {
                node:
                "ellipse;html=1;fillColor=#f5f5f5;strokeColor=#666666;whiteSpace=wrap;shadow=0;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",

                hook:
                "html=1;fillColor=#f5f5f5;strokeColor=#666666;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",

                fkey:
                "ellipse;html=1;fillColor=#f5f5f5;strokeColor=#666666;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",

                arrow:
                "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=#666666;"
            },
            {
                node:
                "ellipse;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;whiteSpace=wrap;shadow=0;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",

                hook:
                "html=1;fillColor=#e1d5e7;strokeColor=#9673a6;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",

                fkey:
                "ellipse;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",

                arrow:
                "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=#9673a6;"
            },
            {
                node:
                "ellipse;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;whiteSpace=wrap;shadow=0;spacingLeft=10;spacingTop=5;fontSize=12;verticalAlign=top;spacingTop=20;opacity=60;",

                hook:
                "html=1;fillColor=#ffe6cc;strokeColor=#d79b00;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",

                fkey:
                "ellipse;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;shadow=0;strokeWidth=2;fontSize=12;horizontal=0;labelPosition=center;verticalLabelPosition=bottom;align=center;spacingLeft=80;spacingTop=-5;opacity=60;",

                arrow:
                "edgeStyle=topToBottomEdgeStyle;html=1;rounded=1;curved=1;strokeWidth=2;strokeColor=#d79b00;"
            }
        ];

        var topics_style = [];

        for(var i=0; i<topics.length && i<default_styles.length; i++) {
            var topic_style = {
                topic_name: topics[i].topic_name,
                run_event: false,
                default_cx: 200,
                default_cy: 180,
                default_alt_cx: 110,
                default_alt_cy: 80,
                graph_styles: default_styles[i]
            };

            topics_style.push(topic_style);
        }

        return topics_style;
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

    /********************************************
     *
     ********************************************/
    function treedb_link_nodes(
        self,
        treedb_name,
        parent_ref,
        child_ref,
        options
    )
    {
        if(!self.config.gobj_remote_yuno) {
            log_error(self.gobj_short_name() + ": No gobj_remote_yuno defined");
            return;
        }

        var command = "link-nodes";

        var kw = {
            service: treedb_name,
            treedb_name: treedb_name,
            parent_ref: parent_ref,
            child_ref: child_ref,
            options: options || {}
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

    /********************************************
     *
     ********************************************/
    function treedb_unlink_nodes(
        self,
        treedb_name,
        parent_ref,
        child_ref,
        options
    )
    {
        if(!self.config.gobj_remote_yuno) {
            log_error(self.gobj_short_name() + ": No gobj_remote_yuno defined");
            return;
        }

        var command = "unlink-nodes";

        var kw = {
            service: treedb_name,
            treedb_name: treedb_name,
            parent_ref: parent_ref,
            child_ref: child_ref,
            options: options || {}
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

    /********************************************
     *
     ********************************************/
    function treedb_descs(self, treedb_name)
    {
        if(!self.config.gobj_remote_yuno) {
            log_error(self.gobj_short_name() + ": No gobj_remote_yuno defined");
            return;
        }

        var command = "descs";

        var kw = {
            service: treedb_name,
            treedb_name: treedb_name
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
            if(json_size(self.config.topics)==0) {
                for(let topic_name in self.config.descs) {
                    if(topic_name.substring(0, 2) == "__") {
                        continue;
                    }
                    self.config.topics.push({topic_name: topic_name});
                }
            }
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
         *  Create default styles if not defined
         */
        if(json_size(self.config.topics_style)==0) {
            self.config.topics_style = build_default_graph_topics_style(self.config.topics);
            self.config.gobj_nodes_tree.gobj_send_event(
                "EV_CREATE_GRAPH_STYLES",
                {
                    topics_style: self.config.topics_style,
                },
                self
            );
        }

        /*
         *  Treedb tables on auto_topics
         */
        if(self.config.auto_topics) {
            if(self.config.with_treedb_tables) {
                if(self.config.gobj_treedb_tables) {
                    log_error("gobj_treedb_tables ALREADY created");
                }
                self.config.gobj_treedb_tables = self.yuno.gobj_create_unique(
                    build_name(self, "Topics"),
                    Ui_treedb_tables,
                    {
                        subscriber: self,
                        with_treedb_tables: self.config.with_treedb_tables,
                        auto_topics: self.config.auto_topics,
                        hook_data_viewer: Ui_hook_viewer_popup,
                        gobj_remote_yuno: self.config.gobj_remote_yuno,
                        treedb_name: self.config.treedb_name,
                        topics: self.config.topics,
                        info_wait: self.config.info_wait,
                        info_no_wait: self.config.info_no_wait
                    },
                    self
                );
                self.config.gobj_treedb_tables.gobj_start();
            }
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
        self.config.gobj_nodes_tree.gobj_send_event(
            "EV_CLEAR_DATA",
            {
            },
            self
        );

        let options = {
            list_dict: true
        };

        for(let i=0; i<self.config.topics.length; i++) {
            let topic = self.config.topics[i];
            let topic_name = topic.topic_name;

            treedb_nodes(self,
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
        /*
         *  Get data
         */
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
        self.config.info_no_wait();

        let result;
        let comment;
        let schema;
        let data;
        let __md_iev__;

        try {
            result = kw.result;
            comment = kw.comment;
            schema = kw.schema;
            data = kw.data;
            __md_iev__ = kw.__md_iev__;
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
                    self.config.gobj_nodes_tree.gobj_send_event(
                        "EV_DESCS",
                        data,
                        self
                    );
                }
                break;
            case "nodes":
                if(result >= 0) {
                    self.config.gobj_nodes_tree.gobj_send_event(
                        "EV_LOAD_DATA",
                        {
                            schema: schema,
                            data: data
                        },
                        self
                    );
                }
                break;

            case "create-node":
            case "update-node":
            case "delete-node":
            case "link-nodes":
            case "unlink-nodes":
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
        let treedb_name = kw_get_str(kw, "treedb_name", "", 0);
        let topic_name = kw_get_str(kw, "topic_name", "", 0);
        let node = kw_get_dict_value(kw, "node", null, 0);

        if(treedb_name != self.config.treedb_name) {
            log_error("It's not my treedb_name: " + treedb_name);
            return 0;
        }

        let schema = self.config.descs[topic_name];

        self.config.gobj_nodes_tree.gobj_send_event(
            "EV_NODE_CREATED",
            {
                schema: schema,
                topic_name: topic_name,
                node: node
            },
            self
        );

        return 0;
    }

    /********************************************
     *  Remote subscription response
     ********************************************/
    function ac_treedb_node_updated(self, event, kw, src)
    {
        let treedb_name = kw_get_str(kw, "treedb_name", "", 0);
        let topic_name = kw_get_str(kw, "topic_name", "", 0);
        let node = kw_get_dict_value(kw, "node", null, 0);

        if(treedb_name != self.config.treedb_name) {
            log_error("It's not my treedb_name: " + treedb_name);
            return 0;
        }

        self.config.gobj_nodes_tree.gobj_send_event(
            "EV_NODE_UPDATED",
            {
                topic_name: topic_name,
                node: node
            },
            self
        );

        return 0;
    }

    /********************************************
     *  Remote subscription response
     ********************************************/
    function ac_treedb_node_deleted(self, event, kw, src)
    {
        let treedb_name = kw_get_str(kw, "treedb_name", "", 0);
        let topic_name = kw_get_str(kw, "topic_name", "", 0);
        let node = kw_get_dict_value(kw, "node", null, 0);

        if(treedb_name != self.config.treedb_name) {
            log_error("It's not my treedb_name: " + treedb_name);
            return 0;
        }

        self.config.gobj_nodes_tree.gobj_send_event(
            "EV_NODE_DELETED",
            {
                topic_name: topic_name,
                node: node
            },
            self
        );

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_refresh_treedb(self, event, kw, src)
    {
        refresh_treedb(self);
        return 0;
    }

    /********************************************
     *  Event from Mx_nodes_tree
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

        var name = "Graph Hook>" + treedb_name + ">" +
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
     *  Event from Mx_nodes_tree
     ********************************************/
    function ac_show_treedb_topic(self, event, kw, src)
    {
        var topic_name = kw.topic_name;
        var gobj_topic_formtable = self.config.gobj_treedb_tables.gobj_send_event(
            "EV_GET_TOPIC_FORMTABLE",
            {
                topic_name: topic_name
            },
            self
        );

        if(gobj_topic_formtable) {
            gobj_topic_formtable.gobj_send_event("EV_TOGGLE", {}, self);
        } else {
            log_error("Topic Formtable not found: " + topic_name);
        }

        return 0;
    }

    /********************************************
     *  Event from Mx_nodes_tree
     *  kw: {
     *      treedb_name
     *      topic_name,
     *      record
     *  }
     ********************************************/
    function ac_mx_vertex_clicked(self, event, kw, src)
    {
        //trace_msg(kw);
        return 0;
    }

    /********************************************
     *  Event from Mx_nodes_tree
     *  kw: {
     *      child_topic_name,
     *      child_topic_id,
     *      child_fkey,
     *      parent_topic_name,
     *      parent_topic_id,
     *      parent_hook
     * }
     ********************************************/
    function ac_mx_edge_clicked(self, event, kw, src)
    {
        //trace_msg(kw);
        return 0;
    }

    /********************************************
     *  Message from Mx_nodes_tree
     *  Send to backend, speaking of node
     ********************************************/
    function ac_create_record(self, event, kw, src)
    {
        var treedb_name = kw.treedb_name;
        var topic_name = kw.topic_name;
        var record = kw.record;
        var options = kw.options || {};

        return treedb_create_node(
            self,
            treedb_name,
            topic_name,
            record,
            options
        );
    }

    /********************************************
     *  Message from Mx_nodes_tree
     *  Send to backend, speaking of node
     ********************************************/
    function ac_update_record(self, event, kw, src)
    {
        var treedb_name = kw.treedb_name;
        var topic_name = kw.topic_name;
        var record = kw.record;
        var options = kw.options || {};

        return treedb_update_node(
            self,
            treedb_name,
            topic_name,
            record,
            options
        );
    }

    /********************************************
     *  Message from Mx_nodes_tree
     *  Send to backend, speaking of node
     ********************************************/
    function ac_delete_record(self, event, kw, src)
    {
        var treedb_name = kw.treedb_name;
        var topic_name = kw.topic_name;
        var record = kw.record;
        var options = kw.options || {};

        return treedb_delete_node(
            self,
            treedb_name,
            topic_name,
            record,
            options
        );
    }

    /********************************************
     *  Message from Mx_nodes_tree
     *  Send to backend, speaking of node
     ********************************************/
    function ac_link_records(self, event, kw, src)
    {
        var treedb_name = kw.treedb_name;
        var parent_ref = kw.parent_ref;
        var child_ref = kw.child_ref;
        var options = kw.options || {};

        return treedb_link_nodes(
            self,
            treedb_name,
            parent_ref,
            child_ref,
            options
        );
    }

    /********************************************
     *  Message from Mx_nodes_tree
     *  Send to backend, speaking of node
     ********************************************/
    function ac_unlink_records(self, event, kw, src)
    {
        var treedb_name = kw.treedb_name;
        var parent_ref = kw.parent_ref;
        var child_ref = kw.child_ref;
        var options = kw.options || {};

        return treedb_unlink_nodes(
            self,
            treedb_name,
            parent_ref,
            child_ref,
            options
        );
    }

    /********************************************
     *  Message from Mx_nodes_tree
     ********************************************/
    function ac_run_node(self, event, kw, src)
    {
        var treedb_name = kw.treedb_name;
        var topic_name = kw.topic_name;
        var record = kw.record;
        var options = kw.options || {};

        var url = record.url;
        var dst_role = record.dst_role;
        var dst_service = record.dst_service;
        var dst_yuno = record.dst_yuno;
        var viewer_engine = record.viewer_engine;

        var gclass = gobj_find_gclass(viewer_engine);
        if(!gclass) {
            log_error("Viewer engine (gclass) not found: " + viewer_engine);
            return -1;
        }

        var name = viewer_engine + ">" + url + ">" + dst_role + ">" + dst_service;
        var gobj = __yuno__.gobj_find_unique_gobj(name);
        if(!gobj) {
            gobj = __yuno__.gobj_create_unique(
                name,
                gclass,
                {
                    is_pinhold_window: true,
                    window_title: name,
                    window_image: "", // TODO /static/app/images/yuneta/topic_schema.svg",

                    dst_role: dst_role,
                    dst_service: dst_service,
                    dst_yuno: dst_yuno,
                    url: url
                },
                self
            );
            gobj.gobj_start();
        } else {
            gobj.gobj_send_event("EV_TOGGLE", {}, self);
        }

        return 0;
    }

    /********************************************
     *  From wrapped $ui, destroy self
     *  - Top toolbar informing of window close
     *      {destroying: true}   Window destroying
     *      {destroying: false}  Window minifying
     *
     ********************************************/
    function ac_close_window(self, event, kw, src)
    {
        if(self.config.is_pinhold_window) {
            __yuno__.gobj_destroy(self);
        }
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_toggle(self, event, kw, src)
    {
        if(self.config.$ui.isVisible()) {
            self.config.$ui.hide();
        } else {
            self.config.$ui.show();
        }
        return self.config.$ui.isVisible();
    }

    /********************************************
     *
     ********************************************/
    function ac_show(self, event, kw, src)
    {
        self.config.$ui.show();
        return self.config.$ui.isVisible();
    }

    /********************************************
     *
     ********************************************/
    function ac_hide(self, event, kw, src)
    {
        self.config.$ui.hide();
        return self.config.$ui.isVisible();
    }

    /********************************************
     *
     ********************************************/
    function ac_select(self, event, kw, src)
    {
        return 0;
    }

    /*************************************************************
     *  Refresh, order from container
     *  provocado por entry/exit de fullscreen
     *  o por redimensionamiento del panel, propio o de hermanos
     *************************************************************/
    function ac_refresh(self, event, kw, src)
    {
        return 0;
    }

    /********************************************
     *  "Container Panel"
     *  Order from container (parent): re-create
     ********************************************/
    function ac_rebuild_panel(self, event, kw, src)
    {
        rebuild(self);
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
            "EV_REFRESH_TREEDB",
            "EV_SHOW_HOOK_DATA",
            "EV_SHOW_TREEDB_TOPIC",
            "EV_MX_VERTEX_CLICKED",
            "EV_MX_EDGE_CLICKED",
            "EV_CREATE_RECORD",
            "EV_DELETE_RECORD",
            "EV_UPDATE_RECORD",
            "EV_LINK_RECORDS",
            "EV_UNLINK_RECORDS",
            "EV_RUN_NODE",
            "EV_CLOSE_WINDOW",
            "EV_TOGGLE",
            "EV_SHOW",
            "EV_HIDE",
            "EV_SELECT",
            "EV_REFRESH",
            "EV_REBUILD_PANEL"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_MT_COMMAND_ANSWER",        ac_mt_command_answer,       undefined],
                ["EV_TREEDB_NODE_CREATED",      ac_treedb_node_created,     undefined],
                ["EV_TREEDB_NODE_UPDATED",      ac_treedb_node_updated,     undefined],
                ["EV_TREEDB_NODE_DELETED",      ac_treedb_node_deleted,     undefined],
                ["EV_REFRESH_TREEDB",           ac_refresh_treedb,          undefined],
                ["EV_SHOW_HOOK_DATA",           ac_show_hook_data,          undefined],
                ["EV_SHOW_TREEDB_TOPIC",        ac_show_treedb_topic,       undefined],
                ["EV_MX_VERTEX_CLICKED",        ac_mx_vertex_clicked,       undefined],
                ["EV_MX_EDGE_CLICKED",          ac_mx_edge_clicked,         undefined],
                ["EV_CREATE_RECORD",            ac_create_record,           undefined],
                ["EV_DELETE_RECORD",            ac_delete_record,           undefined],
                ["EV_UPDATE_RECORD",            ac_update_record,           undefined],
                ["EV_LINK_RECORDS",             ac_link_records,            undefined],
                ["EV_UNLINK_RECORDS",           ac_unlink_records,          undefined],
                ["EV_RUN_NODE",                 ac_run_node,                undefined],
                ["EV_CLOSE_WINDOW",             ac_close_window,            undefined],
                ["EV_TOGGLE",                   ac_toggle,                  undefined],
                ["EV_SHOW",                     ac_show,                    undefined],
                ["EV_HIDE",                     ac_hide,                    undefined],
                ["EV_SELECT",                   ac_select,                  undefined],
                ["EV_REFRESH",                  ac_refresh,                 undefined],
                ["EV_REBUILD_PANEL",            ac_rebuild_panel,           undefined]
            ]
        }
    };

    var Ui_treedb_graph = GObj.__makeSubclass__();
    var proto = Ui_treedb_graph.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ui_treedb_graph",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Ui_treedb_graph, "Ui_treedb_graph");




            /***************************
             *      Framework Methods
             ***************************/




    /************************************************
     *      Framework Method create
     ************************************************/
    proto.mt_create = function(kw)
    {
        var self = this;

        var subscriber = self.gobj_read_attr("subscriber");
        if(!subscriber)
            subscriber = self.gobj_parent();
        self.gobj_subscribe_event(null, null, subscriber);

        if(!self.config.treedb_name) {
            log_error(self.name + " -> treedb_name not configured");
        }
        var is_pinhold_window = self.config.is_pinhold_window;

        if(!is_pinhold_window) {
            /*
             *  Create container
             */
            self.config.gobj_container = self.yuno.gobj_create_unique(
                build_name(self, "ct"),
                Ui_container,
                {
                    mode: "horizontal"
                },
                self
            );
        }

        /*
         *  Nodes tree
         */
        self.config.gobj_nodes_tree = self.yuno.gobj_create_unique(
            build_name(self, "nodes-tree"),
            Mx_nodes_tree,
            {
                is_pinhold_window: is_pinhold_window,
                panel_properties: self.config.panel_properties,
                window_properties: self.config.window_properties,
                ui_properties: self.config.ui_properties,
                window_image: self.config.window_image,
                window_title: self.config.window_title,
                left: self.config.left,
                top: self.config.top,
                width: self.config.width,
                height: self.config.height,

                info_wait: self.config.info_wait,
                info_no_wait: self.config.info_no_wait,

                subscriber: self,
                treedb_name: self.config.treedb_name,
                topics: self.config.topics,
                topics_style: self.config.topics_style,
                with_treedb_tables: self.config.with_treedb_tables,
                hook_port_position: "bottom",
                fkey_port_position: "top"
            },
            is_pinhold_window? self:self.config.gobj_container
        );

        if(!is_pinhold_window) {
            self.config.$ui = self.config.gobj_container.gobj_read_attr("$ui");
        } else {
            self.config.$ui = self.config.gobj_nodes_tree.gobj_read_attr("$ui");
        }

        /*
         *  Treedb tables at start
         */
        if(self.config.with_treedb_tables) {
            if(!self.config.auto_topics) {
                self.config.gobj_treedb_tables = self.yuno.gobj_create_unique(
                    build_name(self, "Topics"),
                    Ui_treedb_tables,
                    {
                        subscriber: self,
                        with_treedb_tables: self.config.with_treedb_tables,
                        auto_topics: self.config.auto_topics,
                        hook_data_viewer: Ui_hook_viewer_popup,
                        gobj_remote_yuno: self.config.gobj_remote_yuno,
                        treedb_name: self.config.treedb_name,
                        topics: self.config.topics,
                        info_wait: self.config.info_wait,
                        info_no_wait: self.config.info_no_wait
                    },
                    self
                );
            }
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

        self.config.gobj_nodes_tree.gobj_start();
        if(self.config.gobj_treedb_tables) {
            self.config.gobj_treedb_tables.gobj_start();
        }

        treedb_descs(self, self.config.treedb_name);
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
    exports.Ui_treedb_graph = Ui_treedb_graph;

})(this);
