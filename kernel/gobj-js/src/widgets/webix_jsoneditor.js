/*******************************************************************
 *  webix_jsoneditor.js
 *
 *  Widget for jsoneditor: https://github.com/josdejong/jsoneditor/
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
    name: "jsoneditor",
    defaults: {
        gobj: null,
        mode: "view",
        modes: ["form","view","tree","code","text","preview"],
        indentation: 4,
        mainMenuBar: true,
        navigationBar: true,
        statusBar: true,
    },

    $init: function(config) {
        var background_color = "#F6F6EF";
        background_color = "#FFFFFF";
        this.$view.innerHTML =
            "<div class='webix_jsoneditor_content' style='background-color:" + background_color + ";width:100%;height:100%;'></div>";
        this._contentobj = this.$view.firstChild;

        this.$ready.push(this.render);

        // do not call Webix Touch handlers
        webix.event(this.$view, "touchstart", function(e) {
            e.cancelBubble = true;
        });
    },

    render: function() {
        // Create the jsoneditor
        var self = this.config.gobj;
        this._jsoneditor = new JSONEditor(
            this._contentobj,
            {
                mode: this.config.mode,
                modes: this.config.modes,
                indentation: this.config.indentation,
                mainMenuBar: this.config.mainMenuBar,
                navigationBar: this.config.navigationBar,
                statusBar: this.config.statusBar,
                timestampTag: function({field, value, path}) {
                    if (field === '__t__' || field === '__tm__' || field === 'tm' ||
                        field === 'from_t' || field === 'to_t' || field === 't' ||
                        field === 't_input' || field === 't_output' ||
                        field === 'from_tm' || field === 'to_tm' || field === 'time'
                    ) {
                        return true;
                    }
                    return false;
                },
                timestampFormat: function({field, value, path}) {
                    if (field === '__t__' || field === '__tm__' || field === 'tm' ||
                        field === 'from_t' || field === 'to_t' || field === 't' ||
                        field === 't_input' || field === 't_output' ||
                        field === 'from_tm' || field === 'to_tm' || field === 'time'
                    ) {
                        return new Date(value*1000).toISOString();
                    }
                    return null;
                },
//                 onEditable: function({path, field, value}) {
//                     return self.gobj_send_event(
//                         "JE_IS_FIELD_EDITABLE",
//                         {
//                             path: path,
//                             field: field,
//                             value: value
//                         },
//                         self
//                     );
//                 },
                onEvent: function(node, event) {
                    if(event.type=="click") {
                        self.gobj_send_event(
                            "JE_CLICK",
                            {
                                path: node.path,
                                field: node.field,
                                value: node.value?node.value:undefined
                            },
                            self
                        );
                    }
                }
                // onClassName: WARNING no lo uses, demasiados eventos si es un json grande
            }
        );
    },

    getJsoneditor: function() {
        return this._jsoneditor;
    },

    getContainer: function() {
        return this._contentobj;
    },

    $setSize:function(x,y) {
        webix.ui.view.prototype.$setSize.call(this,x,y);
        if(this._jsoneditor) {
            this._jsoneditor.refresh();
        }
    }
}, webix.ui.view, webix.EventSystem);

