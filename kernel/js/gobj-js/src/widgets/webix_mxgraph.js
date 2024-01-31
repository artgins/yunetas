/*******************************************************************
 *  webix_mxgraph.js
 *
 *  Widget for mxgraph: https://github.com/jgraph/mxgraph
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
    name: "mxgraph",
    defaults: {
        all_events: false,
        events: [],
        gobj: null
    },

    $init: function(config) {
        var background_color = "#FFFFFF";
        if(config.background_color) {
            background_color = config.background_color;
        }
        this.$view.innerHTML =
            "<div class='webix_mxgraph_content' style='background-color:" + background_color + ";position:relative;width:100%;height:100%;overflow:auto;'></div>";
        this._contentobj = this.$view.firstChild;

        this.$ready.push(this.render);

        // do not call Webix Touch handlers
        webix.event(this.$view, "touchstart", function(e) {
            e.cancelBubble = true;
        });
    },

    render: function() {
        // Create the graph
        this._mxgraph = new mxGraph(this._contentobj);
        this._mxgraph.gobj = this.config.gobj;
        this._events = [
            mxEvent.MOUSE_DOWN,
            mxEvent.MOUSE_MOVE,
            mxEvent.MOUSE_UP,
            mxEvent.ACTIVATE,
            mxEvent.RESIZE_START,
            mxEvent.RESIZE,
            mxEvent.RESIZE_END,
            mxEvent.MOVE_START,
            mxEvent.MOVE,
            mxEvent.MOVE_END,
            mxEvent.PAN_START,
            mxEvent.PAN,
            mxEvent.PAN_END,
            mxEvent.MINIMIZE,
            mxEvent.NORMALIZE,
            mxEvent.MAXIMIZE,
            mxEvent.HIDE,
            mxEvent.SHOW,
            mxEvent.CLOSE,
            mxEvent.DESTROY,
            mxEvent.REFRESH,
            mxEvent.SIZE,
            mxEvent.SELECT,
            mxEvent.FIRED,
            mxEvent.GESTURE,
            mxEvent.TAP_AND_HOLD,
            mxEvent.GET,
            mxEvent.RECEIVE,
            mxEvent.CONNECT,
            mxEvent.DISCONNECT,
            mxEvent.SUSPEND,
            mxEvent.RESUME,
            mxEvent.MARK,
            mxEvent.ROOT,
            mxEvent.POST,
            mxEvent.OPEN,
            mxEvent.SAVE,
            mxEvent.BEFORE_ADD_VERTEX,
            mxEvent.ADD_VERTEX,
            mxEvent.AFTER_ADD_VERTEX,
            mxEvent.DONE,
            mxEvent.EXECUTE,
            mxEvent.EXECUTED,
            mxEvent.BEGIN_UPDATE,
            mxEvent.START_EDIT,
            mxEvent.END_UPDATE,
            mxEvent.END_EDIT,
            mxEvent.BEFORE_UNDO,
            mxEvent.UNDO,
            mxEvent.REDO,
            mxEvent.CHANGE,
            mxEvent.NOTIFY,
            mxEvent.LAYOUT_CELLS,
            mxEvent.CLICK,
            mxEvent.SCALE,
            mxEvent.TRANSLATE,
            mxEvent.SCALE_AND_TRANSLATE,
            mxEvent.UP,
            mxEvent.DOWN,
            mxEvent.ADD,
            mxEvent.REMOVE,
            mxEvent.CLEAR,
            mxEvent.ADD_CELLS,
            mxEvent.CELLS_ADDED,
            mxEvent.MOVE_CELLS,
            mxEvent.CELLS_MOVED,
            mxEvent.RESIZE_CELLS,
            mxEvent.CELLS_RESIZED,
            mxEvent.TOGGLE_CELLS,
            mxEvent.CELLS_TOGGLED,
            mxEvent.ORDER_CELLS,
            mxEvent.CELLS_ORDERED,
            mxEvent.REMOVE_CELLS,
            mxEvent.CELLS_REMOVED,
            mxEvent.GROUP_CELLS,
            mxEvent.UNGROUP_CELLS,
            mxEvent.REMOVE_CELLS_FROM_PARENT,
            mxEvent.FOLD_CELLS,
            mxEvent.CELLS_FOLDED,
            mxEvent.ALIGN_CELLS,
            mxEvent.LABEL_CHANGED,
            mxEvent.CONNECT_CELL,
            mxEvent.CELL_CONNECTED,
            mxEvent.SPLIT_EDGE,
            mxEvent.FLIP_EDGE,
            mxEvent.START_EDITING,
            mxEvent.EDITING_STARTED,
            mxEvent.EDITING_STOPPED,
            mxEvent.ADD_OVERLAY,
            mxEvent.REMOVE_OVERLAY,
            mxEvent.UPDATE_CELL_SIZE,
            mxEvent.ESCAPE,
            mxEvent.DOUBLE_CLICK,
            mxEvent.START,
            mxEvent.RESET,
            mxEvent.FIRE_MOUSE_EVENT  // WARNING too much events
        ];

        var model = this._mxgraph.getModel();
        if(model.updateLevel < 0) {
            log_error("mxGraph beginUpdate/endUpdate NEGATIVE: " + model.updateLevel);
        }

        if(this.config.all_events) {
            for(var i=0; i<this._events.length-1; i++) {
                var ev = this._events[i];
                this._mxgraph.addListener(ev, function(sender, evt) {
                    var gobj = sender.gobj;
                    var properties = evt.getProperties(); // HACK associative array, merde!!!
                    var kw = {};
                    for(var k in properties) {
                        if(properties.hasOwnProperty(k)) {
                            kw[k] = properties[k];
                        }
                    }
                    gobj.gobj_send_event(
                        "EV_MX_" + evt.name.toUpperCase(),
                        kw,
                        gobj
                    );
                });
            }
        } else {
            for(var i=0; i<this.config.events.length; i++) {
                var ev = this.config.events[i];
                if(elm_in_list(ev, this._events)) {
                    this._mxgraph.addListener(ev, function(sender, evt) {
                        var gobj = sender.gobj;
                        var properties = evt.getProperties(); // HACK associative array, merde!!!
                        var kw = {};
                        for(var k in properties) {
                            if(properties.hasOwnProperty(k)) {
                                kw[k] = properties[k];
                            }
                        }
                        gobj.gobj_send_event(
                            "EV_MX_" + evt.name.toUpperCase(),
                            kw,
                            gobj
                        );
                    });
                }
            }
        }
    },

    getMxgraph: function() {
        return this._mxgraph;
    },

    getContainer: function() {
        return this._contentobj;
    },

    $setSize:function(x,y) {
        webix.ui.view.prototype.$setSize.call(this,x,y);
        if(this._mxgraph) {
            this._mxgraph.doResizeContainer(x, y);
        }
    }
}, webix.ui.view, webix.EventSystem);
