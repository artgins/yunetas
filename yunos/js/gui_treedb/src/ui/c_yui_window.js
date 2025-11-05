/***********************************************************************
 *          c_yui_window.js
 *
 *          Window - position fixed
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

import {
    __yuno__,
    SDATA,
    SDATA_END,
    data_type_t,
    gclass_create,
    event_flag_t,
    log_error,
    gobj_read_pointer_attr,
    gobj_subscribe_event,
    gobj_write_attr,
    gobj_short_name,
    clean_name,
    gobj_find_service,
    createElement2,
    is_gobj,
    kw_get_local_storage_value,
    kw_set_local_storage_value,
    gobj_read_attr,
    gobj_read_bool_attr,
    gobj_is_service,
    gobj_name,
    gobj_read_integer_attr,
    gobj_publish_event,
    gobj_destroy,
    gobj_write_bool_attr,
    gobj_write_integer_attr,
    gobj_unsubscribe_event,
    gobj_stop,
    gobj_stop_children,
    gobj_is_running,
    refresh_language,
} from "yunetas";

import {t} from "i18next";

import {get_yesnocancel} from "./c_yui_main.js";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_WINDOW";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",   0,  null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_POINTER,  "$parent",      0,  null,   "$container will be appended to $parent if not null, else to document.body"),
SDATA(data_type_t.DTP_INTEGER,  "x",            0,  "300",  "X position"),
SDATA(data_type_t.DTP_INTEGER,  "y",            0,  "100",  "Y position"),
SDATA(data_type_t.DTP_INTEGER,  "width",        0,  "700",  "Width of the window"),
SDATA(data_type_t.DTP_INTEGER,  "height",       0,  "500",  "Height of the window"),
SDATA(data_type_t.DTP_BOOLEAN,  "auto_save_size_and_position", 0, false, "Automatically save size and position"),
SDATA(data_type_t.DTP_POINTER,  "header",       0,  null,   "Can be a gobj with $container or any createElement2() 'content' parameter"),
SDATA(data_type_t.DTP_POINTER,  "body",         0,  null,   "Can be a gobj with $container or any createElement2() 'content' parameter"),
SDATA(data_type_t.DTP_POINTER, "footer",       0,  null,   "Can be a gobj with $container or any createElement2() 'content' parameter"),
SDATA(data_type_t.DTP_BOOLEAN,  "center",       0,  true,   "Center the window"),
SDATA(data_type_t.DTP_BOOLEAN,  "force_center", 0,  false,  "After resize, re-center"),
SDATA(data_type_t.DTP_BOOLEAN,  "content_size", 0,  false,  "Height automatic, consider use max-height"),
SDATA(data_type_t.DTP_BOOLEAN,  "resizable",    0,  true,   "Allow resizing"),
SDATA(data_type_t.DTP_BOOLEAN,  "showFooter",   0,  true,   "Show footer"),
SDATA(data_type_t.DTP_BOOLEAN,  "openMaximized",0,  false,  "Open the window maximized"),
SDATA(data_type_t.DTP_BOOLEAN,  "showMax",      0,  true,   "Show maximize button"),
SDATA(data_type_t.DTP_BOOLEAN,  "maximized",    0,  false,  "Flag to indicate if maximized"),
SDATA(data_type_t.DTP_JSON,     "window_style", 0,  "{}",   "Override window style"),
SDATA(data_type_t.DTP_POINTER,  "on_close",     0,  null,   "Callback on destroy"),
// TODO pendiente focus modal keyboard
SDATA(data_type_t.DTP_POINTER,  "focus",        0,  null,   "Brings focus to the element, can be a number or selector"),
SDATA(data_type_t.DTP_BOOLEAN,  "modal",        0,  false,  "Enable modal mode"),
SDATA(data_type_t.DTP_BOOLEAN,  "keyboard",     0,  true,   "Close window on ESC if not modal"),
SDATA(data_type_t.DTP_POINTER,  "$container",   0,  null,   "Internal: Window container element"),
SDATA(data_type_t.DTP_STRING,   "window_id",    0,  "",     "Internal: Window ID"),
SDATA_END()
];

let PRIVATE_DATA = {
    prevSize: 0,
};

let __gclass__ = null;




                    /******************************
                     *      Framework Methods
                     ******************************/




/***************************************************************
 *          Framework Method: Create
 ***************************************************************/
function mt_create(gobj)
{
    /*
     *  SERVICE subscription model
     */
    const subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(subscriber) {
        gobj_subscribe_event(gobj, null, {}, subscriber);
    }

    let window_id = "window-" + clean_name(gobj_short_name(gobj));
    gobj_write_attr(gobj, "window_id", window_id);
    build_ui(gobj);
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    let __yui_main__ = gobj_find_service("__yui_main__", false);
    if(__yui_main__) {
        gobj_subscribe_event(__yui_main__, "EV_RESIZE", {}, gobj);
    }
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    // TODO quita esto para chequear el fallo dl_subscribings not implemented
    let __yui_main__ = gobj_find_service("__yui_main__", false);
    if(__yui_main__) {
        gobj_unsubscribe_event(__yui_main__, "EV_RESIZE", {}, gobj);
    }
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
    destroy_ui(gobj);
}




                    /***************************
                     *      Local Methods
                     ***************************/




/************************************************************
 *   Build UI
 ************************************************************/
function build_ui(gobj)
{
    let header = gobj_read_attr(gobj, "header");
    if(is_gobj(header)) {
        header = gobj_read_attr(header, "$container");
    }
    let body = gobj_read_attr(gobj, "body");
    if(is_gobj(body)) {
        body = gobj_read_attr(body, "$container");
    }
    let footer = gobj_read_attr(gobj, "footer");
    if(is_gobj(footer)) {
        footer = gobj_read_attr(footer, "$container");
    }

    /*----------------------------------------------*
     *  Layout Schema
     *----------------------------------------------*/
    if(gobj_read_bool_attr(gobj, "auto_save_size_and_position")) {
        if(gobj_is_service(gobj)) {
            let rect = kw_get_local_storage_value(`${gobj_name(gobj)}-rect`);
            if(rect) {
                rect.x = rect.x<0?0:rect.x;
                rect.y = rect.y<0?0:rect.y;
                gobj_write_attr(gobj, "center", false);
                gobj_write_attr(gobj, "x", rect.x);
                gobj_write_attr(gobj, "y", rect.y);
                gobj_write_attr(gobj, "width", rect.width);
                gobj_write_attr(gobj, "height", rect.height);
            }
        }
    }

    let rect = do_fix_dimension_to_screen(
        gobj,
        gobj_read_integer_attr(gobj, "x"),
        gobj_read_integer_attr(gobj, "y"),
        gobj_read_integer_attr(gobj, "width"),
        gobj_read_integer_attr(gobj, "height")
    );
    if(gobj_read_bool_attr(gobj, "center")) {
        rect = do_center(gobj, rect.x, rect.y, rect.width, rect.height);
    }

    let window_style = {
        position: "fixed",
        "z-index": 3,
        overflow: "hidden",
        "font-family": "var(--bulma-family-primary)",
        "border-radius": "6px",
        padding: "0px",
        margin: "0px",
        "background-color": "var(--bulma-scheme-main)",
        left: `${rect.x}px`,
        top: `${rect.y}px`,
        width: `${rect.width}px`,
        height: `${rect.height}px`,
        "min-width": "300px",
        "min-height": "200px",
        "box-sizing": "border-box",
    };
    Object.assign(window_style, gobj_read_attr(gobj, "window_style"));

    let $container = createElement2(
        ['div', {
            class: 'yui-window strong-shadow is-flex is-flex-direction-column',
            style: window_style}, [

            /*----------------------------*
             *          Header
             *----------------------------*/
            ['div', {
                class: 'yui-window-header p-1 is-flex-shrink-0 is-flex-wrap-wrap is-flex is-justify-content-space-between is-align-items-center has-text-black has-background-info', style: 'border-bottom:1px solid var(--bulma-border); cursor:move; box-sizing: border-box;'
                }, [
                ['div', { class: 'is-flex-grow-1', style: 'height:100%;min-height:0;'}, [
                    ['div', {class: '', style: 'width: 100%;'}, header]
                ]],
                ['div', {class: 'is-flex-shrink-0 is-flex-wrap-wrap is-flex'}, [
                    /*----------------------------*
                     *      Max/min button
                     *----------------------------*/
                    ['button', {
                        class: 'without-border pr-4',
                        style: {
                            color: 'var(--bulma-text)',
                            "font-size": "1.4em",
                            "display": gobj_read_bool_attr(gobj, "showMax")?'inline-block':'none',
                        }
                    }, '<i class="fa-regular fa-square"></i>', {
                        click: (evt) => {
                            evt.stopPropagation();
                            toggle(gobj);
                        }
                    }],
                    /*----------------------------*
                     *      Close button
                     *----------------------------*/
                    ['button', {
                        class: 'without-border pr-2',
                        style: 'color:var(--bulma-text);font-size:1.6em;',
                    }, '<i class="fas fa-xmark"></i>', {
                        click: (evt) => {
                            evt.stopPropagation();
                            close_window(gobj);
                        }
                    }]
                ]]
            ],
                {
                    pointerdown: (evt) => {
                        if(!evt.target.classList.contains("fa-xmark")) {
                            mvStart(gobj, evt);
                        }
                    }
                }
            ],

            /*----------------------------*
             *          Body
             *----------------------------*/
            ['div', {
                class: 'yui-window-body is-flex-grow-1 p-1',
                style: {
                    "position": "relative", // to resize button (absolute position) when no footer bar
                    "overflow": "auto",
                    "overscroll-behavior": "contain",
                    "box-sizing": "border-box",
                    "min-height": 0,
                }}, body],

            /*----------------------------*
             *          Footer
             *----------------------------*/
            ['div', {class: 'yui-window-footer is-flex-shrink-0'}, [
                ['div', {
                    class: 'is-justify-content-space-between is-align-items-center p-1',
                    style: {
                        "display": gobj_read_bool_attr(gobj, "showFooter")?'flex':'none',
                        "border-top": "1px solid var(--bulma-border)",
                        "min-height": "30px",
                        "box-sizing": "border-box",
                    }
                }, [

                    ['div', { class: 'is-flex-grow-1', style: 'height:100%;min-height:0;'}, [
                        ['div', {class: '', style: 'width: 100%;'}, footer]
                    ]],

                    /*----------------------------*
                     *  Resize button in footer
                     *----------------------------*/
                    ['div', {class: 'is-flex-shrink-0 is-flex'}, [
                        ['div', {
                            class: 'without-border',
                            style: {
                                cursor: "nwse-resize",
                                display: gobj_read_bool_attr(gobj, "resizable")?'flex':'none',
                                "box-sizing": "border-box",
                           }
                        }, [
                            ['span', {style: 'display:inline-block;height:1.4em; width:1.4em;'}, '<svg viewBox="0 0 500 500"><path d="m427.87 493.69a33.778 33.78 0 0 1-23.882-57.661l33.778-33.78a33.799 33.8 0 0 1 47.83 47.763l-33.778 33.78a33.778 33.78 0 0 1-23.882 9.8976zm-190.44 0a33.778 33.78 0 0 1-23.882-57.661l224.22-224.23a33.799 33.8 0 0 1 47.83 47.763l-224.22 224.23a33.778 33.78 0 0 1-23.882 9.8976zm-197.26 0a33.778 33.78 0 0 1-23.882-57.661l421.46-421.47a33.786 33.786 0 1 1 47.797 47.763l-421.49 421.47a33.778 33.78 0 0 1-23.882 9.8976z" fill="var(--bulma-text)" stroke-width="33.78"/></svg>']

                        ], {
                            pointerdown: (evt) => {
                                rsStart(gobj, evt);
                            }
                        }]
                    ]]
                ]]
            ]]
        ]]
    );
    gobj_write_attr(gobj, "$container", $container);

    /*----------------------------*
     *  Resize button in body
     *----------------------------*/
    if(gobj_read_bool_attr(gobj, "resizable") && !gobj_read_bool_attr(gobj, "showFooter")) {
        let $resizable_btn = createElement2(['div', {
            class: 'without-border',
            style: {
                cursor: "nwse-resize",
                display: "flex",
                position: "absolute",
                right: "0px",
                bottom: "0px",
                padding: "4px",
                "background-color": "transparent",
            }
        }, [
            ['span',
                {
                    style: 'display:inline-block;height:1.4em; width:1.4em;'
                },
                '<svg viewBox="0 0 500 500"><path d="m427.87 493.69a33.778 33.78 0 0 1-23.882-57.661l33.778-33.78a33.799 33.8 0 0 1 47.83 47.763l-33.778 33.78a33.778 33.78 0 0 1-23.882 9.8976zm-190.44 0a33.778 33.78 0 0 1-23.882-57.661l224.22-224.23a33.799 33.8 0 0 1 47.83 47.763l-224.22 224.23a33.778 33.78 0 0 1-23.882 9.8976zm-197.26 0a33.778 33.78 0 0 1-23.882-57.661l421.46-421.47a33.786 33.786 0 1 1 47.797 47.763l-421.49 421.47a33.778 33.78 0 0 1-23.882 9.8976z" fill="var(--bulma-text)" stroke-width="33.78"/></svg>'
            ]

        ], {
            pointerdown: (evt) => {
                rsStart(gobj, evt);
            }
        }]);

        $container.appendChild($resizable_btn);
    }

    refresh_language($container, t);

    let $parent = gobj_read_attr(gobj, "$parent");
    if($parent) {
        $parent.appendChild($container);
    } else {
        document.body.appendChild($container);
    }

    if (gobj_read_bool_attr(gobj, "openMaximized")) {
        max(gobj);
    } else {
        if(gobj_read_bool_attr(gobj, "content_size")) {
            $container.style.height = "auto";
        }
        if (gobj_read_bool_attr(gobj, "center")) {
            rect = $container.getBoundingClientRect();
            rect = do_center(gobj, rect.x, rect.y, rect.width, rect.height);
            $container.style.left = parseInt(rect.x) + 'px';
            $container.style.top = parseInt(rect.y) + 'px';
        }
    }
}

/************************************************************
 *   Destroy UI
 ************************************************************/
function destroy_ui(gobj)
{
    let $container = gobj_read_attr(gobj, "$container");
    if($container) {
        if($container.parentNode) {
            $container.parentNode.removeChild($container);
        }
        gobj_write_attr(gobj, "$container", null);
    }
}

/************************************************************
 *
 ************************************************************/
function close_window(gobj)
{
    let kw_close = {
        abort_close: false
    };
    gobj_publish_event(gobj, "EV_WINDOW_TO_CLOSE", kw_close);
    let on_close = gobj_read_attr(gobj, "on_close");
    if(on_close) {
        on_close();
    }

    if(!kw_close.abort_close) {
        if(gobj_is_running(gobj)) {
            gobj_stop(gobj);
        }
        gobj_stop_children(gobj);
        gobj_destroy(gobj);
    } else if(kw_close.warning) {
        get_yesnocancel(kw_close.warning, function(resp) {
            if(resp === "yes") {
                gobj_stop(gobj);
                gobj_stop_children(gobj);
                gobj_destroy(gobj);
            }
        });
    }
}

/************************************************************
 *
 ************************************************************/
function toggle(gobj)
{
    if(gobj_read_bool_attr(gobj, "maximized") === true) {
        min(gobj);
    } else {
        max(gobj);
    }
}

/************************************************************
 *
 ************************************************************/
function max(gobj)
{
    let priv = gobj.priv;

    let $container = gobj_read_attr(gobj, "$container");
    priv.prevSize = $container.getBoundingClientRect();

    let rect = do_fix_dimension_to_screen(gobj, 0, 0, 10000, 10000);
    if (gobj_read_bool_attr(gobj, "center")) {
        rect = do_center(gobj, rect.x, rect.y, rect.width, rect.height);
    }

    $container.style.left = parseInt(rect.x) + 'px';
    $container.style.top = parseInt(rect.y) + 'px';
    $container.style.width = parseInt(rect.width) +'px';
    $container.style.height = parseInt(rect.height) +'px';

    gobj_write_bool_attr(gobj, "maximized", true);

    return rect;
}

/************************************************************
 *
 ************************************************************/
function min(gobj)
{
    let priv = gobj.priv;

    let rect = priv.prevSize;
    rect = do_fix_dimension_to_screen(gobj, rect.x, rect.y, rect.width, rect.height);
    if (gobj_read_bool_attr(gobj, "center")) {
        rect = do_center(gobj, rect.x, rect.y, rect.width, rect.height);
    }

    let $container = gobj_read_attr(gobj, "$container");
    $container.style.left = parseInt(rect.x) + 'px';
    $container.style.top = parseInt(rect.y) + 'px';
    $container.style.width = parseInt(rect.width) +'px';
    $container.style.height = parseInt(rect.height) +'px';

    gobj_write_bool_attr(gobj, "maximized", false);

    return rect;
}

/************************************************************
 *  handlers moving
 ************************************************************/
function mvStart(gobj, evt)
{
    let $container = gobj_read_attr(gobj, "$container");

    let window_rect = $container.getBoundingClientRect();
    let x = evt.screenX;
    let y = evt.screenY;
    let pos_x = window_rect.x;
    let pos_y = window_rect.y;
    let div_x, div_y;

    document.addEventListener('pointermove', mvMove);
    document.addEventListener('pointerup', mvStop);

    evt.stopPropagation();
    evt.preventDefault();

    function mvMove(evt)
    {
        div_x = evt.screenX - x;
        div_y = evt.screenY - y;

        // default behavior
        $container.style.transition = 'none';
        $container.style.transform = 'translate3d('+ div_x +'px, '+ div_y +'px, 0px)';
    }

    function mvStop(evt)
    {
        div_x      = (evt.screenX - x);
        div_y      = (evt.screenY - y);
        let xx = pos_x + div_x;
        let yy = pos_y + div_y;

        $container.style.left = xx + 'px';
        $container.style.top = yy + 'px';
        $container.style.transition = 'none';
        $container.style.transform = 'translate3d(0px, 0px, 0px)';

        document.removeEventListener('pointermove', mvMove);
        document.removeEventListener('pointerup', mvStop);

        // trigger event
        let rect = $container.getBoundingClientRect();

        gobj_write_integer_attr(gobj, "x", rect.x);
        gobj_write_integer_attr(gobj, "y", rect.y);
        gobj_write_integer_attr(gobj, "width", rect.width);
        gobj_write_integer_attr(gobj, "height", rect.height);

        if(gobj_read_bool_attr(gobj, "auto_save_size_and_position")) {
            if(gobj_is_service(gobj)) {
                kw_set_local_storage_value(`${gobj_name(gobj)}-rect`, {
                    x:rect.x, y:rect.y, width:rect.width, height: rect.height
                });
            }
        }
        gobj_publish_event(gobj, "EV_WINDOW_MOVED", {rect: rect});
    }
}

/************************************************************
 *  handlers resizing
 ************************************************************/
function rsStart(gobj, evt)
{
    let $container = gobj_read_attr(gobj, "$container");

    let window_rect = $container.getBoundingClientRect();

    let width = window_rect.width;
    let height = window_rect.height;
    let pageX = evt.pageX;
    let pageY = evt.pageY;
    let rel_w = 0;
    let rel_h = 0;

    document.addEventListener('pointermove', rsMove);
    document.addEventListener('pointerup', rsStop);

    evt.stopPropagation();
    evt.preventDefault();

    function rsMove(evt)
    {
        rel_w = evt.pageX - pageX;
        rel_h = evt.pageY - pageY;

        $container.style.width = (width + rel_w) +'px';
        $container.style.height = (height + rel_h) +'px';
    }

    function rsStop(evt)
    {
        rel_w = evt.pageX - pageX;
        rel_h = evt.pageY - pageY;

        $container.style.width = (width + rel_w) +'px';
        $container.style.height = (height + rel_h) +'px';

        document.removeEventListener('pointermove', rsMove);
        document.removeEventListener('pointerup', rsStop);

        // trigger event
        let rect = $container.getBoundingClientRect();
        if(gobj_read_bool_attr(gobj, "force_center")) {
            rect = do_center(gobj,
                rect.x, rect.y, rect.width, rect.height
            );
            $container.style.left = rect.x + 'px';
            $container.style.top = rect.y + 'px';
            $container.style.width = rect.width +'px';
            $container.style.height = rect.height +'px';
        }

        gobj_write_integer_attr(gobj, "x", rect.x);
        gobj_write_integer_attr(gobj, "y", rect.y);
        gobj_write_integer_attr(gobj, "width", rect.width);
        gobj_write_integer_attr(gobj, "height", rect.height);

        if(gobj_read_bool_attr(gobj, "auto_save_size_and_position")) {
            if(gobj_is_service(gobj)) {
                kw_set_local_storage_value(`${gobj_name(gobj)}-rect`, {
                    x:rect.x, y:rect.y, width:rect.width, height: rect.height
                });
            }
        }
        gobj_publish_event(gobj, "EV_WINDOW_RESIZED", {rect: rect});
    }
}

/************************************************************
 *
 ************************************************************/
function do_fix_dimension_to_screen(gobj, x, y, width, height)
{
    let maxW, maxH;
    if (window.innerHeight === undefined) {
        maxW = document.documentElement.offsetWidth;
        maxH = document.documentElement.offsetHeight;
    } else {
        maxW = window.innerWidth;
        maxH = window.innerHeight;
    }

    if (maxW > width) {
        if (x + width > maxW) {
            x = maxW - width;
        }
    } else if (maxW <= width) {
        x = 0;
        width = maxW;
    }

    if (maxH > height) {
        if (y + height > maxH) {
            y = maxH - height;
        }
    } else if (maxH <= height) {
        y = 0;
        height = maxH;
    }

    return {x, y, width, height};
}

/************************************************************
 *
 ************************************************************/
function do_center(gobj, x, y, width, height)
{
    let maxW, maxH;
    if (window.innerHeight === undefined) {
        maxW = document.documentElement.offsetWidth;
        maxH = document.documentElement.offsetHeight;
    } else {
        maxW = window.innerWidth;
        maxH = window.innerHeight;
    }

    if (maxW > width) {
        x = (maxW - width)/2;
    } else if (maxW <= width) {
        x = 0;
    }

    if (maxH > height) {
        y = (maxH - height)/4;
    } else if (maxH <= height) {
        y = 0;
    }

    return {x, y, width, height};
}

/************************************************************
 *
 ************************************************************/
function handleResize(gobj)
{
    let $container = gobj_read_attr(gobj, "$container");

    // Browser window resize
    if(!$container) {
        return;
    }
    let rect = $container.getBoundingClientRect();
    rect = do_fix_dimension_to_screen(gobj, rect.x, rect.y, rect.width, rect.height);
    if(gobj_read_bool_attr(gobj, "center")) {
        rect = do_center(gobj, rect.x, rect.y, rect.width, rect.height);
    }
    if($container) {
        $container.style.left = rect.x + 'px';
        $container.style.top = rect.y + 'px';
        $container.style.width = rect.width +'px';
        $container.style.height = gobj_read_bool_attr(gobj, "content_size")?"auto":parseInt(rect.height) +'px';
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *
 ************************************************************/
function ac_resize(gobj, event, kw, src)
{
    handleResize(gobj);
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_refresh(gobj, event, kw, src)
{
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_show(gobj, event, kw, src)
{

    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_hide(gobj, event, kw, src)
{
    return 0;
}




                    /***************************
                     *          FSM
                     ***************************/




/*---------------------------------------------*
 *          Global methods table
 *---------------------------------------------*/
const gmt = {
    mt_create:  mt_create,
    mt_start:   mt_start,
    mt_stop:    mt_stop,
    mt_destroy: mt_destroy
};

/***************************************************************
 *          Create the GClass
 ***************************************************************/
function create_gclass(gclass_name)
{
    if (__gclass__) {
        log_error(`GClass ALREADY created: ${gclass_name}`);
        return -1;
    }

    /*---------------------------------------------*
     *          States
     *---------------------------------------------*/
    const states = [
        ["ST_IDLE", [
            ["EV_RESIZE",       ac_resize,      null],
            ["EV_REFRESH",      ac_refresh,     null],
            ["EV_SHOW",         ac_show,        null],
            ["EV_HIDE",         ac_hide,        null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_WINDOW_TO_CLOSE",  event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
        ["EV_WINDOW_MOVED",     event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
        ["EV_WINDOW_RESIZED",   event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
        ["EV_RESIZE",           0],
        ["EV_REFRESH",          0],
        ["EV_SHOW",             0],
        ["EV_HIDE",             0]
    ];

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,  // lmt,
        attrs_table,
        PRIVATE_DATA,
        0,  // authz_table,
        0,  // command_table,
        0,  // s_user_trace_level
        0   // gclass_flag
    );
    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************
 *          Register GClass
 ***************************************************************/
function register_c_yui_window()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_window };
