/***********************************************************************
 *          Ka_utils.js
 *
 *          Utilities for KonvA
 *
 *          Based in Konva
 *
 *          Copyright (c) 2022 Niyamaka.
 *          All Rights Reserved.
 ***********************************************************************/
(function (exports) {
    "use strict";

    /************************************************************
     *  Search and check if items belongs to konva container
     *  and then call 'callback' function
     *
     *  {
     *      "items": [__ka_node__, ...]
     *  }
     *  {
     *      "items": [{"__ka_node__":__ka_node__}, ...]
     *  }
     *  {
     *      "items": [{"id":?, "name":?}, ...]  // id,name: Mandatory fields to search in Konva group nodes
     *  }
     *
     *  "__ka_node__" has precedence,
     *      if not found then a search
     *      with `id` and/or `name` will be used
     *
     *  Prototype of callback:
     *      function(kw_item, __ka_node__) {
     *          // kw_item: the dict iterated over the list,
     *          //  can be null if items is not a list of dicts
     *      }
     ************************************************************/
    function search_ka_nodes(ka_container, kw, callback)
    {
        if (!container) {
            log_error("search_ka_nodes(): ka_container not defined");
            return -1;
        }

        if(kw_has_key(kw, "items")) {
            let kw_items = kw_get_list(kw, "items", [], 0);

            for (let i = 0; i < kw_items.length; i++) {
                let kw_item = kw_items[i];

                if (kw_item instanceof Konva.Node) {
                    if(kw_item.getParent() === ka_container) {
                        callback(null, kw_item);
                    } else {
                        log_error("search_ka_nodes(): ka_node not belong this container");
                    }
                } else if(is_object(kw_item)) {
                    let __ka_node__ = kw_get_dict_value(kw_item, "__ka_node__", null, false, false);
                    if(__ka_node__) {
                        if(__ka_node__.getParent() === ka_container) {
                            callback(kw_item, __ka_node__);
                        } else {
                            log_error("search_ka_nodes(): ka_node not belong this container");
                        }
                    } else {
                        let id = kw_get_str(kw_item, "id", null, false, false);
                        if(!empty_string(id)) {
                            __ka_node__ = container.find("#" + id);
                            if(__ka_node__) {
                                callback(kw_item, __ka_node__);
                            }
                        } else {
                            let name = kw_get_str(kw_item, "name", null, false, false);
                            if(!empty_string(name)) {
                                let __ka_nodes__ = container.find(name);
                                if(__ka_nodes__) {
                                    for(let i=0; i<__ka_nodes__.length; i++) {
                                        callback(kw_item, __ka_nodes__[i]);
                                    }
                                }
                            } else {
                                log_error("search_ka_nodes(): no params to find ka_node");
                            }
                        }
                    }
                } else {
                    log_error("search_ka_nodes(): what fuck is it?");
                }
            }
        } else {
            log_error("search_ka_nodes(): no items found");
            return -1;
        }

        return 0;
    }

    /************************************************
     *
     ************************************************/
    function display_text(layer, text, x, y)
    {
        let size = get_text_size(text, "sans-serif", 18, 5);
        let ka_text = new Konva.Text({
            text: text,
            fontSize: 18,
            fontFamily: "sans-serif",
            draggable: true,
            name: "text",
            x: is_number(x)?x:100,
            y: is_number(y)?y:100,
            width: size.width,
            height: size.height,
            shadowBlur: 0
        });
        layer.add(ka_text);
        layer.draw();
    }

    /************************************************************
     *        indentation functions.
     ************************************************************/
    let __inside_loop__ = 0;
    function _tab()
    {
        let spaces, pad;
        if (__inside_loop__ <= 0) {
            spaces = 1;
        } else {
            spaces = __inside_loop__ * 4;
        }
        pad = '';
        while (spaces--) {
            pad += ' ';
        }
        return pad;
    }

    function increase_inside()
    {
        __inside_loop__ += 1;
    }
    function decrease_inside()
    {
        __inside_loop__ -= 1;
    }

    /************************************************
     *
     ************************************************/
    function _log_konva_tree0(jn, k, verbose)
    {
        increase_inside();
        if(verbose) {
            let msg = sprintf("%s - k:'%s', gobj:'%s', id:'%s', name:'%s', str:%s, sha:%d, rel: %j, abs: %j",
                _tab(),
                k.getClassName(),
                k.gobj? k.gobj.gobj_short_name():"",
                k.getAttr("id"),
                k.getAttr("name"),
                k.getAttr("strokeWidth")?k.getAttr("strokeWidth"):0,
                k.getAttr("shadowBlur")?k.getAttr("shadowBlur"):0,
                k.getClientRect({relativeTo:k.getParent()}),
                k.getClientRect()
            );
            log_debug(msg);
        }
        json_object_update(jn, {
            konva: k.getClassName(),
            gobj: k.gobj? k.gobj.gobj_short_name():"",
            id: k.getAttr("id"),
            name: k.getAttr("name"),
            config: k.gobj?filter_dict(k.gobj.config, ["x", "y", "width", "height"]):"",
            position_size: Object.assign({}, k.position(), k.size()),
            stroke: k.getAttr("strokeWidth")?k.getAttr("strokeWidth"):0,
            shadowBlue: k.getAttr("shadowBlur")?k.getAttr("shadowBlur"):0,
            clientrect_rel: k.getClientRect({relativeTo:k.getParent()}),
            clientrect_abs: k.getClientRect()
        });

        if(k.getChildren) {
            let list = k.getChildren();
            let childs = jn["zchilds"] = [];
            for(let i=0; i<list.length; i++) {
                let child = list[i];
                let _jn = {};
                childs.push(_jn);
                _log_konva_tree0(_jn, child, verbose);
            }
        }

        decrease_inside();
    }

    /************************************************
     *
     ************************************************/
    function log_konva_tree0(container, verbose)
    {
        __inside_loop__ = 0;

        let k = null;
        if(is_gobj(container)) {
            k = container.get_konva_container();
        } else if(container instanceof Konva.Container) {
            k = container;
        }
        if(!k) {
            return;
        }
        let jn = {};
        _log_konva_tree0(jn, k, verbose);
        return jn;
    }

    /************************************************
     *
     ************************************************/
    function _log_konva_tree(jn, k, verbose)
    {
        increase_inside();
        if(verbose) {
            let msg = sprintf("%s - konva: '%s', gobj: '%s', id: '%s', name: '%s'",
                _tab(),
                k.getClassName(),
                k.gobj? k.gobj.gobj_short_name():"",
                k.getAttr("id"),
                k.getAttr("name")
            );
            log_debug(msg);
        }
        json_object_update(jn, {
            konva: k.getClassName(),
            gobj: k.gobj? k.gobj.gobj_short_name():"",
            id: k.getAttr("id"),
            name: k.getAttr("name"),
            xattrs: k.getAttrs()
        });

        if(k.getChildren) {
            let list = k.getChildren();
            let childs = jn["zchilds"] = [];
            for(let i=0; i<list.length; i++) {
                let child = list[i];
                let _jn = {};
                childs.push(_jn);
                _log_konva_tree(_jn, child, verbose);
            }
        }

        decrease_inside();
    }

    /************************************************
     *
     ************************************************/
    function log_konva_tree(container, verbose)
    {
        __inside_loop__ = 0;

        let k = null;
        if(is_gobj(container)) {
            k = container.get_konva_container();
        } else if(container instanceof Konva.Container) {
            k = container;
        }
        if(!k) {
            return;
        }
        let jn = {};
        _log_konva_tree(jn, k, verbose);
        return jn;
    }

    //=======================================================================
    //      Expose the class via the global object
    //=======================================================================
    exports.search_ka_nodes = search_ka_nodes;
    exports.display_text = display_text;
    exports.log_konva_tree0 = log_konva_tree0;
    exports.log_konva_tree = log_konva_tree;

})(this);
