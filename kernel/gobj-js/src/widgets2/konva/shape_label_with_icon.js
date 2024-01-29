/***********************************************************************
 *          shape_label_with_icons.js
 *
 *          Draw a text with an icon, return the konva container
 *
 *          Pure Konva
 *
 *          Copyright (c) 2023 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    'use strict';

    let CONFIG = {
        id: "",         // konva id
        name: "shape_label_with_icon",       // konva name
        label: "",      // text of item
        icon: "",       // icon of item (from an icon font)
        icon_position: "left", /* position of icon combined with text: "top", "bottom", "left", "right" */
        background_color: "#FFF7E0",
        color: "black",
        text_color: "black",
        icon_color: "black",

        x: 0,
        y: 0,
        width: 150,
        height: 40,

        visible: true,

        icon_size: 18,  // Wanted size, but change by checking pixels in browser (_icon_size will be used)
        text_size: 18,  // it's different in mobile with text size larger (_text_size will be used)
        autosize: false,    // Change dimension to size of font text

        kw_text_font_properties: {
            // fontSize:    // Override if you don't want it was calculated internally (_text_size)
            fontFamily: "sans-serif", // "OpenSans"
            fontStyle: "normal",
            padding: 10,
            align: "center",
            verticalAlign: "middle",
            wrap: "char"
        },
        kw_icon_font_properties: {
            // fontSize:    // Override if you don't want it was calculated internally (_icon_size)
            fontFamily: "yuneta-icon-font",
            fontStyle: "normal",
            padding: 10,
            align: "center",
            verticalAlign: "middle",
            wrap: "char"
        },

        kw_border_shape: { /* Border shape */
            cornerRadius: 10,
            strokeWidth: 2,
            stroke: "#f5c211ff",
            opacity: 1,
            shadowBlur: 0,
            shadowColor: "black",
            shadowForStrokeEnabled: false // HTML5 Canvas Optimizing Strokes Performance Tip
        }
    };

    /********************************************
     *
     ********************************************/
    function create_shape_label_with_icon(config)
    {
        let width = kw_get_int(config, "width", CONFIG.width);
        let height = kw_get_int(config, "height", CONFIG.height);
        let background_color = kw_get_str(config, "background_color", CONFIG.background_color);

        /*
         *  Container (Group)
         */
        let ka_container = new Konva.Group({
            id: kw_get_str(config, "id", CONFIG.id),
            name: kw_get_str(config, "name", CONFIG.name),
            x: kw_get_int(config, "x", CONFIG.x),
            y: kw_get_int(config, "y", CONFIG.y),
            width: width,
            height: height,
            visible: kw_get_bool(config, "visible", CONFIG.visible)
        });

        /*
         *  Border
         */
        let kw_border_shape = __duplicate__(
            kw_get_dict(CONFIG, "kw_border_shape", {})
        );
        json_object_update(kw_border_shape, kw_get_dict(config, "kw_border_shape", {}));
        json_object_update(
            kw_border_shape,
            {
                name: "ka_border_shape",
                x: 0,
                y: 0,
                width: width,
                height: height,
                fill: background_color,
            }
        );
        let _ka_border_shape = new Konva.Rect(kw_border_shape);
        ka_container.add(_ka_border_shape);

        let label = create_label(config);
        ka_container.add(label);

        if(config.autosize) {
            let font_dimension = label.getClientRect();
            _ka_border_shape.size(font_dimension);
            ka_container.size(font_dimension);
            let ka_container_dimension = ka_container.getClientRect();

            config.width = ka_container_dimension.width;
            config.height = ka_container_dimension.height;
        }

        /*-------------------------------*
         *      Functions
         *-------------------------------*/
        ka_container.shape_label_background_color = function(color)
        {
            let ka = ka_container.find(".ka_border_shape");
            if(ka.length === 0) {
                log_error("ka not found");
                return null;
            }
            if(color) {
                ka[0].fill(color);
            } else {
                color = ka[0].fill();
            }

            return color;
        };

        ka_container.shape_label_icon_color = function(color)
        {
            let ka = ka_container.find(".ka_icon");
            if(ka.length === 0) {
                log_error("ka not found");
                return null;
            }
            if(color) {
                ka[0].fill(color);
            } else {
                color = ka[0].fill();
            }

            return color;
        };

        ka_container.shape_label_text_color = function(color)
        {
            let ka = ka_container.find(".ka_text");
            if(ka.length === 0) {
                log_error("ka not found");
                return null;
            }
            if(color) {
                ka[0].fill(color);
            } else {
                color = ka[0].fill();
            }

            return color;
        };

        ka_container.shape_label_color = function(color)
        {
            let ka = ka_container.find(".ka_text");
            if(ka.length > 0) {
                ka[0].fill(color);
            }

            ka = ka_container.find(".ka_icon");
            if(ka.length > 0) {
                ka[0].fill(color);
            }
            return ka_container;
        };

        ka_container.shape_label_size = function(size)
        {
            ka_container.size(size);

            /*
             *  Resize background rect
             */
            let ka = ka_container.find(".ka_border_shape");
            if(ka.length === 0) {
                log_error("ka not found");
                return ka_container;
            }
            ka[0].size(size);
            return ka_container;
        };

        ka_container.shape_label_text = function(text)
        {
            /*
             *  Rewrite text
             */
            let ka = ka_container.find(".ka_text");
            if(ka.length === 0) {
                log_error("ka not found");
                return ka_container;
            }
            ka[0].text(text);
            return ka_container;
        };

        return ka_container;
    }

    /********************************************
     *
     ********************************************/
    function create_label(config)
    {
        let width = kw_get_int(config, "width", CONFIG.width);
        let height = kw_get_int(config, "height", CONFIG.height);
        let color = kw_get_str(config, "color", CONFIG.color);
        let text_color = kw_get_str(config, "text_color", color);
        let icon_color = kw_get_str(config, "icon_color", color);

        let kw_text_font_properties = __duplicate__(
            kw_get_dict(CONFIG, "kw_text_font_properties", {})
        );
        json_object_update(kw_text_font_properties, kw_get_dict(config, "kw_text_font_properties", {}));
        if(text_color) {
            if(!kw_has_key(kw_text_font_properties, "fill")) {
                kw_text_font_properties.fill = text_color;
            }
        }

        let kw_icon_font_properties = __duplicate__(
            kw_get_dict(CONFIG, "kw_icon_font_properties", {})
        );
        json_object_update(kw_icon_font_properties, kw_get_dict(config, "kw_icon_font_properties", {}));
        if(icon_color) {
            if(!kw_has_key(kw_icon_font_properties, "fill")) {
                kw_icon_font_properties.fill = icon_color;
            }
        }

        let _text_size = adjust_font_size( // Calculated by checking browser
            kw_get_int(config, "text_size", CONFIG.text_size),
            kw_text_font_properties.fontFamily
        );
        let _icon_size = adjust_icon_size( // Calculated by checking browser
            kw_get_int(config, "icon_size", CONFIG.icon_size),
            kw_icon_font_properties.fontFamily
        );

        let text = kw_get_str(config, "label", CONFIG.label);
        let icon = kw_get_str(config, "icon", CONFIG.icon);

        let container = new Konva.Group({
            name: "ka_label_container"
        });

        let icon_element=null, text_element=null;

        if(!empty_string(text) && !empty_string(icon)) {
            let icon_position = kw_get_str(config, "icon_position", CONFIG.icon_position);
            switch(icon_position) {
                case "top": {
                    let kw_icon = { // Common fields
                        name: "ka_icon",
                        text: icon,
                        x: 0,
                        y: 0,
                        fontSize: _icon_size
                    };
                    json_object_update(kw_icon, kw_icon_font_properties);
                    icon_element = new Konva.Text(kw_icon);
                    container.add(icon_element);

                    let kw_text = { // Common fields
                        name: "ka_text",
                        text: text,
                        x: 0,
                        y: icon_element.height() -
                            kw_get_int(kw_text_font_properties, "padding", 0),
                        lineHeight: icon_element.lineHeight(),
                        fontSize: _text_size
                    };
                    json_object_update(kw_text, kw_text_font_properties);
                    text_element = new Konva.Text(kw_text);
                    container.add(text_element);

                    let max_width = Math.max(icon_element.width(), text_element.width());
                    icon_element.width(max_width);
                    text_element.width(max_width);
                }
                break;

                case "bottom": {
                    let kw_text = { // Common fields
                        name: "ka_text",
                        text: text,
                        x: 0,
                        y: 0,
                        fontSize: _text_size
                    };
                    json_object_update(kw_text, kw_text_font_properties);
                    text_element = new Konva.Text(kw_text);
                    container.add(text_element);

                    let kw_icon = { // Common fields
                        name: "ka_icon",
                        text: icon,
                        x: 0,
                        y: text_element.height() -
                            kw_get_int(kw_text_font_properties, "padding", 0),
                        lineHeight: text_element.lineHeight(),
                        fontSize: _icon_size
                    };
                    json_object_update(kw_icon, kw_icon_font_properties);
                    icon_element = new Konva.Text(kw_icon);
                    container.add(icon_element);

                    let max_width = Math.max(icon_element.width(), text_element.width());
                    icon_element.width(max_width);
                    text_element.width(max_width);
                }
                break;

                case "left": {
                    let kw_icon = { // Common fields
                        name: "ka_icon",
                        text: icon,
                        x: 0,
                        y: 0,
                        fontSize: _icon_size
                    };
                    json_object_update(kw_icon, kw_icon_font_properties);
                    icon_element = new Konva.Text(kw_icon);
                    container.add(icon_element);

                    let kw_text = { // Common fields
                        name: "ka_text",
                        text: text,
                        x: icon_element.width(),
                        y: 0,
                        fontSize: _text_size
                    };
                    json_object_update(kw_text, kw_text_font_properties);
                    text_element = new Konva.Text(kw_text);
                    container.add(text_element);

                    let max_height = Math.max(icon_element.height(), text_element.height());
                    icon_element.height(max_height);
                    text_element.height(max_height);
                }
                break;

                case "right": {
                    let kw_text = { // Common fields
                        name: "ka_text",
                        text: text,
                        x: 0,
                        y: 0,
                        fontSize: _text_size
                    };
                    json_object_update(kw_text, kw_text_font_properties);
                    text_element = new Konva.Text(kw_text);
                    container.add(text_element);

                    let kw_icon = { // Common fields
                        name: "ka_icon",
                        text: icon,
                        x: text_element.width(),
                        y: 0,
                        fontSize: _icon_size
                    };
                    json_object_update(kw_icon, kw_icon_font_properties);
                    icon_element = new Konva.Text(kw_icon);
                    container.add(icon_element);

                    let max_height = Math.max(icon_element.height(), text_element.height());
                    icon_element.height(max_height);
                    text_element.height(max_height);
                }
                break;
            }

        } else if(!empty_string(icon)) {
            let kw_icon = { // Common fields
                name: "ka_icon",
                text: icon,
                x: 0,
                y: 0,
                fontSize: _icon_size
            };
            json_object_update(kw_icon, kw_icon_font_properties);
            icon_element = new Konva.Text(kw_icon);
            container.add(icon_element);

        } else {
            let kw_text = { // Common fields
                name: "ka_text",
                text: text?text:"",
                x: 0,
                y: 0,
                fontSize: _text_size
            };
            if(!config.autosize) {
                kw_text.width = width;
                kw_text.height = height;
            }

            json_object_update(kw_text, kw_text_font_properties);
            text_element = new Konva.Text(kw_text);
            container.add(text_element);
        }

        return container;
    }

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.create_shape_label_with_icon = create_shape_label_with_icon;
})(this);
