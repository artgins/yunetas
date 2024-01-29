/***********************************************************************
 *          ui_lib_webix.js
 *
 *          Webix Helpers
 *
 *  Version
 *  -------
 *  1.0     Initial release
 *
 *          Copyright (c) 2020 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    "use strict";

    /********************************************
     *
     ********************************************/
    function setup_webix(config)
    {
        /*
         *  Multicombo propio
         */
        webix.protoUI({
            name: "multicombo2",
            $init:function(config) {
                if(config.options) {
                    config.options = __duplicate__(config.options);
                }
            },
            setValue: function(value) {
                if(webix.isArray(value)) {
                    var temp = [];
                    for (var i = 0; i < value.length; i++) {
                        if(is_object(value[i])) {
                            temp.push(value[i].id);
                        } else {
                            temp.push(value[i]);
                        }
                    }
                    value = temp;
                }
                return webix.ui.multicombo.prototype.setValue.call(this, value);
            },
            getValue: function() {
                var value = webix.ui.multicombo.prototype.getValue.call(this);
                return value.split(",");
            }
        }, webix.ui.multicombo);

        /*
         *  Combo propio
         */
        webix.protoUI({
            name: "combo2",
            $init:function(config) {
                if(config.options) {
                    config.options = __duplicate__(config.options);
                }
            },
            setValue: function(value) {
                if(is_object(value)) {
                    value = value.id;
                }
                return webix.ui.combo.prototype.setValue.call(this, value);
            }
        }, webix.ui.combo);

        /*
         *  Multiselect propio
         */
        webix.protoUI({
            name: "multiselect2",
            $init:function(config) {
                if(config.options) {
                    config.options = __duplicate__(config.options);
                }
            },
            setValue: function(value) {
                if(webix.isArray(value)) {
                    var temp = [];
                    for (var i = 0; i < value.length; i++) {
                        if(is_object(value[i])) {
                            temp.push(value[i].id);
                        } else {
                            temp.push(value[i]);
                        }
                    }
                    value = temp;
                }
                return webix.ui.multiselect.prototype.setValue.call(this, value);
            },
            getValue: function() {
                var value = webix.ui.multiselect.prototype.getValue.call(this);
                return value.split(",");
            }
        }, webix.ui.multiselect);

        /*
         *  Select propio
         */
        webix.protoUI({
            name: "select2",
            $init:function(config) {
                if(config.options) {
                    config.options = __duplicate__(config.options);
                }
            },
            setValue: function(value) {
                if(is_object(value)) {
                    value = value.id;
                }
                return webix.ui.select.prototype.setValue.call(this, value);
            }
        }, webix.ui.select);

        /*
         *  Contador de rows para las tablas
         */
        webix.ui.datafilter.countColumn = webix.extend({
            refresh: function (master, node, value) {
                var result = 0;
                master.mapCells(null, value.columnId, null, 1, function (value) {
                    result++;
                    return value;
                });

                node.innerHTML = result;
            }
        }, webix.ui.datafilter.summColumn);

        /*
         *  Esc key to close modal windows
         */
        // las window que llaman a close() se quedan mal con Esc
        //webix.UIManager.addHotKey("esc", function(view) {
        //    if (view) {
        //        var top = view.getTopParentView();
        //        if (top && top.setPosition)
        //        top.hide();
        //    }
        //});

        /*
         *  Habilita scroll de webix, mas delgada
         */
        if(!webix.env.touch && webix.env.scrollSize) {
            webix.CustomScroll.init();
        }

        /*
         *  Habilita full screen
         */
        if(config && config.full_screen) {
            webix.ui.fullScreen();
        }
    }

    /************************************************************
     *   Build name
     ************************************************************/
    function build_name(self, name)
    {
        // We need unique names
        if(empty_string(self.gobj_name())) {
            if(!self._uuid_name) {
                self._uuid_name = get_unique_id(self.gobj_gclass_name());
            }
            return self._uuid_name + "-" + name;
        }
        return self.gobj_escaped_short_name() + "-" + name;
    }

    /************************************************************
     *  Color green
     ************************************************************/
    function set_icon_submmit_state(self, name, set)
    {
        var btn = $$(build_name(self, name));
        if(set) {
            webix.html.addCss(btn.getNode(), "icon_color_submmit");
        } else {
            webix.html.removeCss(btn.getNode(), "icon_color_submmit");
        }
    }

    /************************************************************
     *  Color red
     ************************************************************/
    function set_icon_cancel_state(self, name, set)
    {
        var btn = $$(build_name(self, name));
        if(set) {
            webix.html.addCss(btn.getNode(), "icon_color_cancel");
        } else {
            webix.html.removeCss(btn.getNode(), "icon_color_cancel");
        }
    }

    /************************************************************
     *  Color orange
     ************************************************************/
    function set_icon_active_state(self, name, set)
    {
        var btn = $$(build_name(self, name));
        if(set) {
            webix.html.addCss(btn.getNode(), "icon_color_active");
        } else {
            webix.html.removeCss(btn.getNode(), "icon_color_active");
        }
    }

    /************************************************************
     *  Color green
     ************************************************************/
    function set_btn_submmit_state(self, name, set)
    {
        var btn = $$(build_name(self, name));
        if(set) {
            webix.html.addCss(btn.getNode(), "icon_btn_color_submmit");
        } else {
            webix.html.removeCss(btn.getNode(), "icon_btn_color_submmit");
        }
    }

    /************************************************************
     *  Color red
     ************************************************************/
    function set_btn_cancel_state(self, name, set)
    {
        var btn = $$(build_name(self, name));
        if(set) {
            webix.html.addCss(btn.getNode(), "icon_btn_color_cancel");
        } else {
            webix.html.removeCss(btn.getNode(), "icon_btn_color_cancel");
        }
    }

    /************************************************************
     *  Color orange
     ************************************************************/
    function set_btn_active_state(self, name, set)
    {
        var btn = $$(build_name(self, name));
        if(set) {
            webix.html.addCss(btn.getNode(), "icon_btn_color_active");
        } else {
            webix.html.removeCss(btn.getNode(), "icon_btn_color_active");
        }
    }

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.setup_webix = setup_webix;
    exports.build_name = build_name;
    exports.set_icon_submmit_state = set_icon_submmit_state;
    exports.set_icon_cancel_state = set_icon_cancel_state;
    exports.set_icon_active_state = set_icon_active_state;
    exports.set_btn_submmit_state = set_btn_submmit_state;
    exports.set_btn_cancel_state = set_btn_cancel_state;
    exports.set_btn_active_state = set_btn_active_state;
})(this);
