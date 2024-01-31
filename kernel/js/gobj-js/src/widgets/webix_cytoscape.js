/*******************************************************************
 *  webix_cytoscape.js
 *
 *  Widget for Cytoscape: https://js.cytoscape.org/
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
    name:"cytoscape",

    defaults: {
        gobj: null,
        cytoscape_options: {}
    },

    $init: function(config) {
        this.$view.innerHTML =
            "<div class='webix_cytoscape_content' style='background-color:#FFFFFF;width:100%;height:100%;z-index:0'></div>";
        this._contentobj = this.$view.firstChild;

        this._waitCytoscape = webix.promise.defer();
        this.$ready.push(this.render);

        // do not call Webix Touch handlers
        webix.event(this.$view, "touchstart", function(e){
            e.cancelBubble = true;
        });
    },

    render: function() {
        webix.require([
            "/static/cytoscape/cytoscape.umd.js"
        ])
        .then(
            webix.bind(this._initCytoscape, this)
        ).catch(function(e){
            console.log(e);
        });
    },

    _initCytoscape: function(define) {
        var c = this.config;

        if(this.isVisible(c.id)) {
            c.cytoscape_options["container"] = this._contentobj;
            var cy = this._cy = cytoscape(c.cytoscape_options);
            this._waitCytoscape.resolve(this._cy);
            this._cy.on('mousedown', function(e) {
                // stop propagation
                e.originalEvent.preventDefault();
                e.originalEvent.stopPropagation();
            });

            var self = c.gobj;
            cy.on('click',
                function(e) {
                    self.gobj_send_event("EV_CYTOSCAPE_EVENT", {e: e}, self);
                }
            );

//  TODO individualiza los eventos
//            cy.on('click', 'mouseover', 'touchstart', 'touchmove', 'touchend',
//                   'layoutstart', 'layoutready', 'layoutstop', 'ready', 'destroy',
//                   'render', 'pan', 'zoom', 'viewport', 'resize', 'dragfree',
//                   'dragfreeon', 'position', 'select', 'unselect', 'add', 'remove',
//                   'move', 'boxselect', 'box',
//                 function(e) {
//                     self.gobj_send_event("EV_CYTOSCAPE_EVENT", {e: e}, self);
//                 }
//             );

            self.gobj_send_event("EV_CYTOSCAPE_INITIALIZED", {_cy: this._cy}, self);

        }
    },

    $setSize: function() {
        webix.ui.view.prototype.$setSize.apply(this, arguments);
        if(this._cy) {
            this._cy.resize();
        }
    }
}, webix.ui.view, webix.EventSystem);
