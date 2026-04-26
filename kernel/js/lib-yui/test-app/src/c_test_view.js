/***********************************************************************
 *          c_test_view.js
 *
 *      C_TEST_VIEW — Minimal view gobj used by the test app.
 *      Exposes a $container and renders a card with:
 *          - The configured title + color
 *          - Its own route
 *          - A lifecycle instance counter (so you can tell whether the
 *            gobj was destroyed between visits or just hidden)
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA, SDATA_END, data_type_t,
    gclass_create, log_error,
    gobj_parent,
    gobj_read_attr, gobj_read_pointer_attr, gobj_write_attr,
    gobj_subscribe_event,
    gobj_name,
    createElement2,
} from "@yuneta/gobj-js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_TEST_VIEW";

let __instance_counter__ = 0;


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",   0,  null,         "Subscriber of output events"),

SDATA(data_type_t.DTP_STRING,   "title",        0,  "View",       "Title shown on the card"),
SDATA(data_type_t.DTP_STRING,   "color",        0,  "#3273dc",    "Accent color"),
SDATA(data_type_t.DTP_POINTER,  "$container",   0,  null,         "Root HTMLElement"),
SDATA(data_type_t.DTP_INTEGER,  "instance_id",  0,  0,            "Monotonic id of this instance"),
SDATA_END()
];

let PRIVATE_DATA = {};
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
     *  CHILD subscription model
     */
    let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, null, {}, subscriber);

    let id = ++__instance_counter__;
    gobj_write_attr(gobj, "instance_id", id);
    build_ui(gobj);
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
    let $c = gobj_read_attr(gobj, "$container");
    if($c && $c.parentNode) {
        $c.parentNode.removeChild($c);
    }
    gobj_write_attr(gobj, "$container", null);
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************
 *  Build the card DOM and store it in $container.
 ***************************************************************/
function build_ui(gobj)
{
    let title = gobj_read_attr(gobj, "title") || "View";
    let color = gobj_read_attr(gobj, "color") || "#3273dc";
    let id = gobj_read_attr(gobj, "instance_id");

    let $c = createElement2(
        ["div", {class: "view-card", style: `background:${hex_alpha(color, 0.08)};`},
            [
                ["h1", {class: "title is-4", style: `color:${color};`}, title],
                ["p", {class: "is-size-7"},
                    `gobj: ${gobj_name(gobj)}  ·  instance #${id}`
                ],
                ["div", {class: "bg", style: `border-color:${color};`}, title]
            ]
        ]
    );
    gobj_write_attr(gobj, "$container", $c);
}

/***************************************************************
 *  Convert "#rrggbb" + alpha to an rgba() string;
 *  return the input untouched if it is not a 6-digit hex.
 ***************************************************************/
function hex_alpha(hex, a)
{
    let m = /^#([0-9a-f]{6})$/i.exec(hex);
    if(!m) {
        return hex;
    }
    let n = parseInt(m[1], 16);
    let r = (n >> 16) & 0xff, g = (n >> 8) & 0xff, b = n & 0xff;
    return `rgba(${r},${g},${b},${a})`;
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *              FSM
 ***************************************************************/
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
    if(__gclass__) {
        log_error(`GClass ALREADY created: ${gclass_name}`);
        return -1;
    }

    /*---------------------------------------------*
     *          States
     *---------------------------------------------*/
    const states = [
        ["ST_IDLE", []]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [];

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
function register_c_test_view()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_test_view };
