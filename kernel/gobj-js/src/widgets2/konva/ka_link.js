/***********************************************************************
 *          Ka_link.js
 *
 *          Node's Link
 *          Link two nodes through their ports
 *
 *          The shape is made with Path class (svg)
 *
 *          source_node             target_node
 *                  ▼                       ▼
 *              source_port ──────────► target_port
 *
 *          A node must be an unique gobj supporting the events:
 *              - publishing of EV_MOVING/EV_MOVED events
 *              - input event EV_GET_DIMENSION
 *
 *          A node is a gobj publishing their movements that has child gobjs with the role of ports.
 *          A node is a gobj that can be connected with another gobj through their ports.
 *
 *          Based in KonvA
 *
 *          Copyright (c) 2022 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    'use strict';

    /********************************************
     *  Configuration (C attributes)
     *  Attributes without underscore prefix
     *      will be set in self.config
     *  Attributes with underscore ("_") prefix:
     *      will be set in self.private
     ********************************************/
    let CONFIG = {
        //////////////// Public Attributes //////////////////
        subscriber: null,   // subscriber of publishing messages (Child model: if null will be the parent)
        layer: null,        // Konva layer
        view: null,         // View containing the link

        //------------ Own Attributes ------------//
        // source_node/target_node must support EV_MOVING/EV_MOVED/EV_GET_DIMENSION events
        // these gobj/ports can be strings or gobjs
        source_node: null,  // unique gobj (or his name) with 'node' role
        source_port: null,  // child gobj (or his name) of source_node with 'port' role
        target_node: null,  // unique gobj (or his name) with 'node' role
        target_port: null,  // child gobj (or his name) of target_node with 'port' role
        photons: 0,
        photon_color: "yellow",

        shape: "bezier",  /* "bezier", "arrow", "line" */
        connection_margin: 0,
        color: "#000000BB",     /* link (path) color */
        speed: 100,             /* animation speed: pixels by second */
        width: 4,                /* kw_border_shape.strokeWidth */

        visible: true,

        fontFamily: "sans-serif", // "OpenSans"
        icon_size: 30,  // Wanted size, but change by checking pixels in browser (_icon_size will be used)
        text_size: 18,  // it's different in mobile with text size larger (_text_size will be used)

        kw_border_shape: { /* Border shape (Path Class)*/
            shadowBlur: 0,
            shadowColor: "black",
            shadowForStrokeEnabled: false // HTML5 Canvas Optimizing Strokes Performance Tip
        },

        kw_photon_shape: {
            shape: "circle",
            width: 12,
            height: 12,
            radius: 6
        },

        timeout_retry: 5,       // timeout retry, in seconds
        timeout_idle: 5,        // idle timeout, in seconds

        //////////////// Private Attributes /////////////////
        _icon_size: 0,      // Calculated by checking browser
        _text_size: 0,      // Calculated by checking browser
        _gobj_source_node: null,
        _gobj_source_port: null,
        _gobj_target_node: null,
        _gobj_target_port: null,

        _ka_container: null,
        _ka_path: null,
        _path_length: 0,
        _ka_animation: null,
        _ka_photons: []
    };




                    /***************************
                     *      Local Methods
                     ***************************/




    /************************************************
     *
     ************************************************/
    function adjust_text_and_icon_size(self)
    {
        self.private._text_size = adjust_font_size(self.config.text_size, self.config.fontFamily);
        self.private._icon_size = adjust_font_size(self.config.icon_size, self.config.fontFamily);
    }

    /********************************************
     *
     ********************************************/
    function get_control_points(from, to)
    {
        let x1 = 0;
        let y1 = 0;
        let x2 = 0;
        let y2 = 0;

        switch(from.port_position) {
            case "left":
                switch(to.port_position) {
                    case "left":
                        // TODO
                        break;
                    case "right":
                        // TODO
                        break;
                    case "top":
                        // TODO
                        break;
                    case "bottom":
                        // TODO
                        break;
                }
                break;
            case "right":
                switch(to.port_position) {
                    case "left":
                        x1 = from.x + (to.x - from.x)/3 * 2;
                        y1 = from.y;
                        x2 = from.x + (to.x - from.x)/3;
                        y2 = to.y;
                        break;
                    case "right":
                        // TODO
                        break;
                    case "top":
                        // TODO
                        break;
                    case "bottom":
                        // TODO
                        break;
                }
                break;
            case "top":
                switch(to.port_position) {
                    case "left":
                        // TODO
                        break;
                    case "right":
                        // TODO
                        break;
                    case "top":
                        // TODO
                        break;
                    case "bottom":
                        // TODO
                        break;
                }
                break;
            case "bottom":
                switch(to.port_position) {
                    case "left":
                        // TODO
                        break;
                    case "right":
                        // TODO
                        break;
                    case "top":
                        // TODO
                        break;
                    case "bottom":
                        // TODO
                        break;
                }
                break;
        }

        return {
            x1: x1,
            y1: y1,
            x2: x2,
            y2: y2
        };
    }

    /********************************************
     *
     ********************************************/
    function get_connector_path(self, from, to)
    {
        let from_x, from_y, to_x, to_y;

        from_x = from.relative_dimension.x + from.relative_dimension.width/2;
        from_y = from.relative_dimension.y + from.relative_dimension.height/2;
        to_x = to.relative_dimension.x + to.relative_dimension.width/2 + 1;
        to_y = to.relative_dimension.y + to.relative_dimension.height/2 + 1;

        switch(self.config.shape) {
            case "bezier": {
                const dx = to_x - from_x;
                const dy = to_y - from_y;
                let angle = Math.atan2(-dy, dx);
                let from_radius = from.relative_dimension.width/2 + self.config.connection_margin;
                let from_ = {
                    x: from_x + -from_radius * Math.cos(angle + Math.PI),
                    y: from_y + from_radius * Math.sin(angle + Math.PI),
                    port_position: from.port_position
                };
                let to_radius = to.relative_dimension.width/2 + self.config.connection_margin;
                let to_ = {
                    x: to_x + -to_radius * Math.cos(angle),
                    y: to_y + to_radius * Math.sin(angle),
                    port_position: to.port_position
                };

                let control_points = get_control_points(from_, to_);

                let points = [
                    from_.x,
                    from_.y,
                    control_points.x1,
                    control_points.y1,
                    control_points.x2,
                    control_points.y2,
                    to_.x,
                    to_.y
                ];
                return sprintf("M %d %d C %d %d %d %d %d %d",
                    points[0],
                    points[1],
                    points[2],
                    points[3],
                    points[4],
                    points[5],
                    points[6],
                    points[7],
                );
            }

            case "arrow":
            case "line":
            default: {
                const dx = to_x - from_x;
                const dy = to_y - from_y;
                let angle = Math.atan2(-dy, dx);
                let from_radius = from.relative_dimension.width/2 + self.config.connection_margin;
                let to_radius = to.relative_dimension.width/2 + self.config.connection_margin;

                let points = [
                    from_x + -from_radius * Math.cos(angle + Math.PI),
                    from_y + from_radius * Math.sin(angle + Math.PI),
                    to_x + -to_radius * Math.cos(angle),
                    to_y + to_radius * Math.sin(angle)
                ];

                return sprintf("M %d %d L %d %d",
                    points[0],
                    points[1],
                    points[2],
                    points[3]
                );
            }
        }
    }

    /********************************************
     *
     ********************************************/
    function update_link_path(self)
    {
        let source_port = self.private._gobj_source_port;
        let target_port = self.private._gobj_target_port;

        let kw_source_dim = {};
        let kw_target_dim = {};
        source_port.gobj_send_event("EV_GET_DIMENSION", kw_source_dim, self);
        target_port.gobj_send_event("EV_GET_DIMENSION", kw_target_dim, self);

        kw_source_dim.port_position = source_port.config.port_position;
        kw_target_dim.port_position = target_port.config.port_position;

        const path = get_connector_path(
            self,
            kw_source_dim,
            kw_target_dim
        );

        for(let i=0; i<self.private._ka_photons.length; i++) {
            let photon = self.private._ka_photons[i];
            photon.photon_idx = (self.private._path_length / self.private._ka_photons.length) * i;
        }

        self.private._ka_path.data(path);
        self.private._path_length = self.private._ka_path.getLength();
        // self.config._ka_path.draw();
    }

    /************************************************
     *
     ************************************************/
    function link_ports(self)
    {
        let name = self.config.source_node;
        self.private._gobj_source_node = get_unique_gobj(self, name);
        if(!self.private._gobj_source_node) {
            log_error(sprintf("%s: unique source node not found: '%s'",
                self.gobj_short_name(),
                is_gobj(name)?name.gobj_short_name():name
            ));
            return null;
        }

        name = self.config.target_node;
        self.private._gobj_target_node = get_unique_gobj(self, name);
        if(!self.private._gobj_target_node) {
            log_error(sprintf("%s: unique target node not found: '%s'",
                self.gobj_short_name(),
                is_gobj(name)?name.gobj_short_name():name
            ));
            return null;
        }

        name = self.config.source_port;
        self.private._gobj_source_port = get_child_gobj(
            self,
            self.private._gobj_source_node,
            name
        );
        if(!self.private._gobj_source_port) {
            log_error(sprintf("%s: source port not found: '%s'",
                self.gobj_short_name(),
                is_gobj(name)?name.gobj_short_name():name
            ));
            return null;
        }

        name = self.config.target_port;
        self.private._gobj_target_port = get_child_gobj(
            self,
            self.private._gobj_target_node,
            name
        );
        if(!self.private._gobj_target_port) {
            log_error(sprintf("%s: target port not found: '%s'",
                self.gobj_short_name(),
                is_gobj(name)?name.gobj_short_name():name
            ));
            return null;
        }

        self.private._gobj_source_node.gobj_subscribe_event("EV_MOVING", {}, self);
        self.private._gobj_source_node.gobj_subscribe_event("EV_MOVED", {}, self);
        self.private._gobj_target_node.gobj_subscribe_event("EV_MOVING", {}, self);
        self.private._gobj_target_node.gobj_subscribe_event("EV_MOVED", {}, self);

        update_link_path(self); // Update the arrow
    }

    /********************************************
     *  Return the unique gobj, null if error
     ********************************************/
    function get_unique_gobj(self, name)
    {
        let node;

        if (is_string(name)) {
            node = self.yuno.gobj_find_unique_gobj(name);
            if (!node) {
                return null;
            }
        } else if(is_gobj(name)) {
            node = name;
            if(!node.gobj_is_unique(node)) {
                node = null;
            }
        } else {
            node = null;
        }

        return node;
    }

    /********************************************
     *  Return the child gobj, null if error
     ********************************************/
    function get_child_gobj(self, gobj_parent, name)
    {
        let child_gobj;

        if (is_string(name)) {
            child_gobj = gobj_parent.gobj_child_by_name(name);
            if (!child_gobj) {
                return null;
            }
        } else if (is_gobj(name)) {
            child_gobj = name;
            if(!gobj_parent.gobj_child_by_name(child_gobj.gobj_name())) {
                child_gobj = null;
            }
        } else {
            child_gobj = null;
        }

        return child_gobj;
    }
    /********************************************
     *
     ********************************************/
    function create_shape(self)
    {
        /*
         *  Container (Group)
         */
        let ka_container = self.private._ka_container = new Konva.Group({
            id: self.gobj_short_name(),
            name: "ka_container",
            visible: self.config.visible,
            listening: true
        });
        ka_container.gobj = self; // cross-link

        /*
         *  Border
         */
        let kw_border_shape = __duplicate__(
            kw_get_dict(self.config, "kw_border_shape", {}, false, false)
        );
        json_object_update_missing(
            kw_border_shape,
            {
                name: "ka_border_path",
                strokeWidth: kw_get_int(self.config, "width", 4),
                stroke: kw_get_str(self.config, "color", null)
            }
        );
        self.private._ka_path = new Konva.Path(kw_border_shape);
        ka_container.add(self.private._ka_path);
    }




                    /***************************
                     *      Actions
                     ***************************/




    /********************************************
     *  Source or target are moving
     ********************************************/
    function ac_moving(self, event, kw, src)
    {
        update_link_path(self); // Update the line
        return 0;
    }

    /********************************************
     *  Source or target are end of moving
     ********************************************/
    function ac_moved(self, event, kw, src)
    {
        update_link_path(self); // Update the line
        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_start_animation(self, event, kw, src)
    {
        if(self.private._ka_photons.length === 0) {
            let kw_photon_shape = __duplicate__(
                kw_get_dict(self.config, "kw_photon_shape", {}, false, false)
            );
            json_object_update_missing(
                kw_photon_shape,
                {
                    name: "ka_border_photon",
                    color: self.config.photon_color
                }
            );

            for(let i=0; i<self.config.photons; i++) {
                let photon = self.yuno.gobj_create(
                    "",
                    Ka_photon,
                    kw_photon_shape,
                    self.config.view
                );
                self.private._ka_photons.push(photon);
                photon.photon_idx = (self.private._path_length/self.config.photons)*i;
            }
        }

        if(self.private._ka_photons.length > 0) {
            if(!self.private._ka_animation) {
                self.private._ka_animation = new Konva.Animation(
                    function(frame) {
                        let increment = self.config.speed * (frame.timeDiff / 1000);

                        for(let i=0; i<self.private._ka_photons.length; i++) {
                            let photon = self.private._ka_photons[i];
                            photon.photon_idx += increment;
                            if(photon.photon_idx < 0) {
                                photon.photon_idx = 0;
                            }
                            let idx = photon.photon_idx % self.private._path_length;
                            let pos = self.private._ka_path.getPointAtLength(idx);
                            photon.get_konva_container().x(pos.x);
                            photon.get_konva_container().y(pos.y);
                        }
                    },
                    self.config.layer
                );
            }
        }

        for(let i=0; i<self.private._ka_photons.length; i++) {
            let photon = self.private._ka_photons[i];
            photon.get_konva_container().visible(true);
        }

        if(self.private._ka_animation) {
            self.private._ka_animation.start();
        }

        return 0;
    }

    /********************************************
     *
     ********************************************/
    function ac_stop_animation(self, event, kw, src)
    {
        if(self.private._ka_animation) {
            self.private._ka_animation.stop();
        }
        for(let i=0; i<self.private._ka_photons.length; i++) {
            let photon = self.private._ka_photons[i];
            photon.get_konva_container().visible(false);
        }
        return 0;
    }




                    /***************************
                     *      GClass/Machine
                     ***************************/




    let FSM = {
        "event_list": [
            "EV_MOVING",
            "EV_MOVED",
            "EV_START_ANIMATION",
            "EV_STOP_ANIMATION"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
                [
                    ["EV_MOVING",               ac_moving,              undefined],
                    ["EV_MOVED",                ac_moved,               undefined],
                    ["EV_START_ANIMATION",      ac_start_animation,     undefined],
                    ["EV_STOP_ANIMATION",       ac_stop_animation,      undefined]
                ]
        }
    };

    let Ka_link = GObj.__makeSubclass__();
    let proto = Ka_link.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ka_link",
            kw,
            0
        );
        return this;
    };
    gobj_register_gclass(Ka_link, "Ka_link");




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
        if(!self.config.view) {
            self.config.view = self.gobj_parent();
        }

        adjust_text_and_icon_size(self);
        create_shape(self);
        link_ports(self);
    };

    /************************************************
     *      Framework Method destroy
     *      In this point, all childs
     *      and subscriptions are already deleted.
     ************************************************/
    proto.mt_destroy = function()
    {
        let self = this;
        if(self.private._ka_container) {
            self.private._ka_container.destroy();
            self.private._ka_container = null;
        }
    };

    /************************************************
     *      Framework Method start
     ************************************************/
    proto.mt_start = function(kw)
    {
        let self = this;
        // self.set_timeout(1*1000);
    };

    /************************************************
     *      Framework Method stop
     ************************************************/
    proto.mt_stop = function(kw)
    {
        let self = this;
        // self.clear_timeout();
    };

    /************************************************
     *      Framework Method writing
     ************************************************/
    proto.mt_writing = function(name)
    {
        let self = this;

        switch(name) {
            case "color":
                self.private._ka_path.stroke(self.config.color);
                break;
            case "width":
                self.private._ka_path.strokeWidth(self.config.width);
                break;
        }
    };

    /************************************************
     *      Local Method
     ************************************************/
    proto.get_konva_container = function()
    {
        let self = this;
        return self.private._ka_container;
    };

    /************************************************
     *      Local Method
     ************************************************/
    proto.get_private_data = function()
    {
        let self = this;
        return {
            gobj_source_node: self.private._gobj_source_node,
            gobj_source_port: self.private._gobj_source_port,
            gobj_target_node: self.private._gobj_target_node,
            gobj_target_port: self.private._gobj_target_port,
            ka_container: self.private._ka_container,
            ka_path: self.private._ka_path,
            ka_photon: self.private._ka_photon
        };
    };

    /************************************************
     *  Return the link gobj, null if error
     ************************************************/
    function _create_link(self, kw, common)
    {
        let id = kw_get_str(kw, "id", kw_get_str(kw, "name", ""));

        /*
         *  Check if link exists
         */
        let gobj_link = self.yuno.gobj_find_unique_gobj(id);
        if(gobj_link) {
            log_error(sprintf("%s: link already exists '%s'", self.gobj_short_name(), id));
            return null;
        }

        // 'id' or 'name' can be use as all port names
        kw["source_port"] = kw_get_dict_value(kw, "source_port", id);
        kw["target_port"] = kw_get_dict_value(kw, "target_port", id);

        json_object_update_missing(kw, common);

        return self.yuno.gobj_create_unique(id, Ka_link, kw, self);
    }

    /************************************************
     *  Public function
     *  Must be call by a graph view
     ************************************************/
    function create_link(self, kw)
    {
        if(self.gobj_gclass_name()!=="Sw_graph") {
            log_error(sprintf("create_link(): Not a Sw_graph gclass"));
            return;
        }

        let links = kw_get_list(kw, "links", null);
        if(!links) {
            /*
             *  Single link
             */
            _create_link(self, kw, {});

        } else {
            for(let i=0; i<links.length; i++) {
                let link =  links[i];
                _create_link(self, link, kw);
            }
        }
    }

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Ka_link = Ka_link;
    exports.create_link = create_link;

})(this);
