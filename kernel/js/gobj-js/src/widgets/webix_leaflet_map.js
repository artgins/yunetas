/*******************************************************************
 *  webix_leaflet_map.js
 *
 *  Widget for Leaflet: https://leafletjs.com/
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
    name:"leaflet-map",

    defaults: {
        gobj: null,
        leaflet_options: {}
    },

    $init: function(config) {
        this.$view.innerHTML =
            "<div class='webix_map_content' style='width:100%;height:100%;z-index:0'></div>";
        this._contentobj = this.$view.firstChild;

        this._waitMap = webix.promise.defer();
        this.$ready.push(this.render);

        // do not call Webix Touch handlers on the map
        webix.event(this.$view, "touchstart", function(e) {
            e.cancelBubble = true;
        });
    },

    render: function() {
        webix.require([
            "/static/leaflet/leaflet.js",
            "/static/leaflet/leaflet.css",
            "/static/leaflet/Leaflet.GoogleMutant.js",
            "/static/leaflet/leaflet.rotatedMarker.js"
        ])
        .then(
            webix.bind(this._initMap, this)
        ).catch(function(e){
            console.log(e);
        });
    },

    _initMap: function(define) {
        var c = this.config;

        if(this.isVisible(c.id)) {
            this._map = L.map(this._contentobj, c.leaflet_options);
            this._waitMap.resolve(this._map);
            var self = c.gobj;
            self.gobj_send_event("EV_MAPA_INITIALIZED", {_map: this._map}, self);

            /*
             *  convierte las callback en eventos de yuneta
             */
            this._map.on({
                'load': function(e) {
                    self.gobj_send_event("EV_LEAFLET_LOAD", e, self);
                },
                'mousedown': function(e) {
                    // stop propagation
                    e.originalEvent.preventDefault();
                    e.originalEvent.stopPropagation();
                },
                'click': function(e) {
                    self.gobj_send_event("EV_LEAFLET_CLICK", e, self);
                },
                'dblclick': function(e) {
                    self.gobj_send_event("EV_LEAFLET_DBLCLICK", e, self);
                },
                'movestart': function(e) {
                    self.gobj_send_event("EV_LEAFLET_MOVESTART", e, self);
                },
                'move': function(e) {
                    self.gobj_send_event("EV_LEAFLET_MOVE", e, self);
                },
                'moveend': function(e) {
                    self.gobj_send_event("EV_LEAFLET_MOVEEND", e, self);
                },
                'zoomstart': function(e) {
                    self.gobj_send_event("EV_LEAFLET_ZOOMSTART", e, self);
                },
                'zoom': function(e) {
                    self.gobj_send_event("EV_LEAFLET_ZOOM", e, self);
                },
                'zoomend': function(e) {
                    self.gobj_send_event("EV_LEAFLET_ZOOMEND", e, self);
                },
                'viewreset': function(e) {
                    self.gobj_send_event("EV_LEAFLET_VIEWRESET", e, self);
                },
                'resize': function(e) {
                    self.gobj_send_event("EV_LEAFLET_RESIZE", e, self);
                },
                'popupopen': function(e) {
                    self.gobj_send_event("EV_LEAFLET_POPUPOPEN", e, self);
                },
                'popupclose': function(e) {
                    self.gobj_send_event("EV_LEAFLET_POPUPCLOSE", e, self);
                },
                'tooltipopen': function(e) {
                    self.gobj_send_event("EV_LEAFLET_TOOLTIPOPEN", e, self);
                },
                'tooltipclose': function(e) {
                    self.gobj_send_event("EV_LEAFLET_TOOLTIPCLOSE", e, self);
                },
                'contextmenu':  function(e) {
                    self.gobj_send_event("EV_LEAFLET_CONTEXTMENU", e, self);
                },
                'dragstart':  function(e) {
                    self.gobj_send_event("EV_LEAFLET_DRAGSTART", e, self);
                },
                'drag':  function(e) {
                    self.gobj_send_event("EV_LEAFLET_DRAG", e, self);
                },
                'dragend':  function(e) {
                    self.gobj_send_event("EV_LEAFLET_DRAGEND", e, self);
                }
            });
        }
    },

    $setSize: function() {
        webix.ui.view.prototype.$setSize.apply(this, arguments);
        if(this._map) {
            this._map.invalidateSize();
        }
    }
}, webix.ui.view, webix.EventSystem);
