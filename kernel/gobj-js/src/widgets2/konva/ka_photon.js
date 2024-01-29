/***********************************************************************
 *          Ka_photon.js
 *
 *          Photon
 *
 *          Based in KonvA
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    'use strict';

    /********************************************
     *      Configuration (C attributes)
     ********************************************/
    let CONFIG = {
        //////////////// Public Attributes //////////////////
        subscriber: null,   // subscriber of publishing messages (Child model: if null will be the parent)
        layer: null,        // Konva layer

        //------------ Own Attributes ------------//
        shape: "circle",   // "circle" of "rect"

        x: 0,
        y: 0,
        width: 12,
        height: 12,
        radius: 6,
        color: "yellow",

        visible: true,

        kw_border_shape: { /* Border shape */
            strokeWidth: 0,
            stroke: "black",
            opacity: 1,
            shadowBlur: 0,
            shadowColor: "black",
            shadowForStrokeEnabled: false // HTML5 Canvas Optimizing Strokes Performance Tip
        },

        //////////////// Private Attributes /////////////////
        _ka_container: null,
        _ka_border_shape: null
    };




                /***************************
                 *      Local Methods
                 ***************************/




    /********************************************
     *
     ********************************************/
    function create_shape(self)
    {
        let width = self.config.width;
        let height = self.config.height;
        let radius = self.config.radius;
        let color = self.config.color;

        /*----------------------------*
         *      Container (Group)
         *----------------------------*/
        let ka_container = new Konva.Group({
            width: width,
            height: height,
            visible: kw_get_bool(self.config, "visible", true)
        });
        ka_container.gobj = self; // cross-link

        /*----------------------------*
         *      Shape
         *----------------------------*/
        let kw_border_shape = __duplicate__(
            kw_get_dict(CONFIG, "kw_border_shape", {})
        );
        json_object_update(kw_border_shape, kw_get_dict(self.config, "kw_border_shape", {}));
        json_object_update_missing(
            kw_border_shape,
            {
                name: "ka_border_shape",
                x: 0,
                y: 0,
                width: width,
                height: height,
                radius: radius,
                fill: color,
            }
        );

        switch(self.config.port_shape) {
            case "rect":
                self.private._ka_border_shape = new Konva.Rect(kw_border_shape);
                break;
            case "circle":
            default:
                self.private._ka_border_shape = new Konva.Circle(kw_border_shape);
        }
        ka_container.add(self.private._ka_border_shape);

        return ka_container;
    }




            /***************************
             *      Actions
             ***************************/




    /********************************************
     *  kw: {
     *      x:
     *      y:
     *  }
     ********************************************/
    function ac_position(self, event, kw, src)
    {
        self.config.x = kw_get_int(kw, "x", self.config.x, false, false);
        self.config.y = kw_get_int(kw, "y", self.config.y, false, false);

        /*
         *  Move container
         */
        let ka_container = self.private._ka_container;
        ka_container.position({
            x: self.config.x,
            y: self.config.y
        });

        return 0;
    }

    /********************************************
     *  kw: {
     *      width:
     *      height:
     *  }
     ********************************************/
    function ac_size(self, event, kw, src)
    {
        self.config.width = kw_get_int(kw, "width", self.config.width, false, false);
        self.config.height = kw_get_int(kw, "height", self.config.height, false, false);
        // TODO

        return 0;
    }




            /***************************
             *      GClass/Machine
             ***************************/




    let FSM = {
        "event_list": [
            "EV_POSITION",
            "EV_SIZE"
        ],
        "state_list": [
            "ST_IDLE"
        ],
        "machine": {
            "ST_IDLE":
            [
                ["EV_POSITION",             ac_position,            undefined],
                ["EV_SIZE",                 ac_size,                undefined]
            ]
        }
    };

    let Ka_photon = GObj.__makeSubclass__();
    let proto = Ka_photon.prototype; // Easy access to the prototype
    proto.__init__= function(name, kw) {
        GObj.prototype.__init__.call(
            this,
            FSM,
            CONFIG,
            name,
            "Ka_photon",
            kw,
            gcflag_no_check_output_events
        );
        return this;
    };
    gobj_register_gclass(Ka_photon, "Ka_photon");




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

        self.private._ka_container = create_shape(self); // WARNING added to layer in mt_child_added of parent
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
    };

    /************************************************
     *      Framework Method stop
     ************************************************/
    proto.mt_stop = function(kw)
    {
        let self = this;
    };

    /************************************************
     *      Framework Method writing
     ************************************************/
    proto.mt_writing = function(name)
    {
        let self = this;

        switch(name) {
            case "color":
                self.private._ka_border_shape.fill(self.config.color);
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


    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.Ka_photon = Ka_photon;

})(this);
