/***********************************************************************
 *          c_test_view.js
 *
 *      C_TEST_VIEW - Minimal view gobj used by the test app.
 *      Exposes a $container and renders a card with:
 *          - The configured title + color
 *          - Its own route
 *          - A lifecycle instance counter (so you can tell whether the
 *            gobj was destroyed between visits or just hidden)
 *
 *      Copyright (c) 2026, ArtGins.
 *      All Rights Reserved.
 ***********************************************************************/
import {
    SDATA, SDATA_END, data_type_t,
    gclass_create, log_error,
    gobj_read_attr, gobj_write_attr, gobj_name,
    createElement2,
} from "@yuneta/gobj-js";


const GCLASS_NAME = "C_TEST_VIEW";

let __instance_counter__ = 0;

const attrs_table = [
SDATA(data_type_t.DTP_STRING,   "title",        0,  "View",       "Title shown on the card"),
SDATA(data_type_t.DTP_STRING,   "color",        0,  "#3273dc",    "Accent color"),
SDATA(data_type_t.DTP_POINTER,  "$container",   0,  null,         "Root HTMLElement"),
SDATA(data_type_t.DTP_INTEGER,  "instance_id",  0,  0,            "Monotonic id of this instance"),
SDATA_END()
];

let PRIVATE_DATA = {};
let __gclass__ = null;


function mt_create(gobj)
{
    let id = ++__instance_counter__;
    gobj_write_attr(gobj, "instance_id", id);
    build_ui(gobj);
}

function mt_start(gobj) { }
function mt_stop(gobj)  { }

function mt_destroy(gobj)
{
    let $c = gobj_read_attr(gobj, "$container");
    if($c && $c.parentNode) $c.parentNode.removeChild($c);
    gobj_write_attr(gobj, "$container", null);
}


function build_ui(gobj)
{
    let title = gobj_read_attr(gobj, "title") || "View";
    let color = gobj_read_attr(gobj, "color") || "#3273dc";
    let id = gobj_read_attr(gobj, "instance_id");

    let $c = createElement2(
        ["div", {class: "view-card", style: `background:${hex_alpha(color, 0.08)};`},
            ["h1", {class: "title is-4", style: `color:${color};`}, title],
            ["p", {class: "is-size-7"},
                `gobj: ${gobj_name(gobj)}  ·  instance #${id}`
            ],
            ["div", {class: "bg", style: `border-color:${color};`}, title]
        ]
    );
    gobj_write_attr(gobj, "$container", $c);
}

function hex_alpha(hex, a)
{
    /*  Accept "#rrggbb" only; fallback to original string. */
    let m = /^#([0-9a-f]{6})$/i.exec(hex);
    if(!m) return hex;
    let n = parseInt(m[1], 16);
    let r = (n >> 16) & 0xff, g = (n >> 8) & 0xff, b = n & 0xff;
    return `rgba(${r},${g},${b},${a})`;
}


const gmt = {
    mt_create:  mt_create,
    mt_start:   mt_start,
    mt_stop:    mt_stop,
    mt_destroy: mt_destroy
};

function create_gclass(name)
{
    if(__gclass__) {
        log_error(`GClass ALREADY created: ${name}`);
        return -1;
    }
    __gclass__ = gclass_create(
        name,
        [],                 /* event_types */
        [["ST_IDLE", []]],  /* states */
        gmt,
        0,
        attrs_table,
        PRIVATE_DATA,
        0, 0, 0, 0
    );
    return __gclass__ ? 0 : -1;
}

function register_c_test_view()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_test_view };
