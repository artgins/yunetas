/*******************************************************************
 *  webix_uplot.js
 *
 *  Widget for uPlot: https://github.com/leeoniya/uPlot
 *
 *  Version
 *  -------
 *  1.0     Initial release
 *
 *
 *  Copyright 2020 Niyamaka
 *  MIT license.
 *******************************************************************/
webix.protoUI({
    name: "uPlot",

    defaults: {
        gobj: null,
        _uPlot_options: {
            title: "",
            width: 600,
            height: 400,
            series: [
                {}
            ]
        },
        uPlot_options: {
        },
        data: [[]]
    },

    $init: function(config) {
        this.$view.innerHTML =
            "<div class='webix_uplot_content' style='padding:10px;width:100%;height:100%;'></div>";
        this._contentobj = this.$view.firstChild;

        this._waitUPlot = webix.promise.defer();
        this.$ready.push(this.render);

        // do not call Webix Touch handlers
        webix.event(this.$view, "touchstart", function(e){
            e.cancelBubble = true;
        });
    },

    render: function() {
        webix.require([
            "/static/uplot/uPlot.css",
            "/static/uplot/uPlot.iife.js"
        ])
        .then(
            webix.bind(this._initUPlot, this)
        ).catch(function(e){
            log_error(e);
        });
    },

    _initUPlot: function(define) {
        var c = this.config;

        if(this.isVisible(c.id)) {
            json_object_update_missing(c.uPlot_options, c._uPlot_options);
            var uplot = this._uplot = new uPlot(c.uPlot_options, c.data, this._contentobj);
            this._waitUPlot.resolve(this._uplot);
            var self = c.gobj;
            self.gobj_send_event("EV_UPLOT_INITIALIZED", {_uplot: this._uplot}, self);
        }
    },

    $setSize: function(w, h) {
        webix.ui.view.prototype.$setSize.apply(this, arguments);
        if(this._uplot) {
            this._uplot.setSize({width: w-20, height: h-80});
        }
    }
}, webix.ui.view, webix.EventSystem);
