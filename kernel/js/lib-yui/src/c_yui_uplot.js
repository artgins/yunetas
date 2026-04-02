/***********************************************************************
 *          c_yui_uplot.js
 *
 *          Graphics, Charts with uPlot
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

import {
    SDATA,
    SDATA_END,
    data_type_t,
    event_flag_t,
    kw_flag_t,
    gclass_create,
    createElement2,
    log_error,
    gobj_read_pointer_attr,
    gobj_parent,
    gobj_subscribe_event,
    kw_get_str,
    gobj_read_attr,
    gobj_read_str_attr,
    gobj_write_attr,
    gobj_write_bool_attr,
    gobj_read_bool_attr,
    is_string, gobj_short_name,
} from "yunetas";

// Import uPlot
import uPlot from "uplot";
import "uplot/dist/uPlot.min.css";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_UPLOT";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_STRING,   "title",            0,  "",     "Chart title"),
SDATA(data_type_t.DTP_INTEGER,  "width",            0,  800,    "Chart width"),
SDATA(data_type_t.DTP_INTEGER,  "height",           0,  600,    "Chart height"),
SDATA(data_type_t.DTP_STRING,   "tm",               0,  "tm",   "Field time of record"),
SDATA(data_type_t.DTP_JSON,     "series",           0,  [{}],   "Data series"),
SDATA(data_type_t.DTP_JSON,     "axes",             0,  [{}],   "Chart axes"),
SDATA(data_type_t.DTP_JSON,     "scales",           0,  {},     "Chart scales"),
SDATA(data_type_t.DTP_JSON,     "data",             0,  [[]],   "Chart data"),
SDATA(data_type_t.DTP_JSON,     "colors",           0,  [
    {stroke:"red", fill: "rgba(255,0,0,0.05)"},
    {stroke:"green", fill: "rgba(0,255,0,0.07)"},
    {stroke:"blue", fill: "rgba(0,0,255,0.05)"},
    {stroke:"orange", fill: "rgba(255,165,0,0.1)"},
], "Color settings"),
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "HTML container for the chart"),
SDATA(data_type_t.DTP_BOOLEAN,  "own_container",    0,  false,  "If true, container is gobj-managed"),
SDATA(data_type_t.DTP_POINTER,  "uplot",            0,  null,   "uPlot instance"),
SDATA(data_type_t.DTP_INTEGER,  "timeout_retry",    0,  5,      "Retry timeout (seconds)"),
SDATA(data_type_t.DTP_INTEGER,  "timeout_idle",     0,  5,      "Idle timeout (seconds)"),
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
    build_ui(gobj);

    /*
     *  CHILD subscription model
     */
    let subscriber = gobj_read_pointer_attr(gobj, "subscriber");
    if(!subscriber) {
        subscriber = gobj_parent(gobj);
    }
    gobj_subscribe_event(gobj, null, {}, subscriber);
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
    let uplot = gobj_read_attr(gobj, "uplot");
    if(uplot) {
        uplot.destroy();
        gobj_write_attr(gobj, "uplot", null);
    }
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
    /*----------------------------------------------*
     *  Layout Schema
     *----------------------------------------------*/
    let $container = gobj_read_attr(gobj, "$container");
    if(!$container) {
        $container = createElement2(
            ['div',
                {
                    class: '',
                    style: 'overflow:auto;'
                }
            ]
        );
        gobj_write_bool_attr(gobj, "$container", $container);
        gobj_write_bool_attr(gobj, "own_container", true);
    }

    let uPlot_options = {
        title: gobj_read_attr(gobj, "title"),
        width: gobj_read_attr(gobj, "width"),
        height: gobj_read_attr(gobj, "height"),
        series: gobj_read_attr(gobj, "series"),
        axes: gobj_read_attr(gobj, "axes"),
        scales: gobj_read_attr(gobj, "scales"),
    };
    let uplot = new uPlot(
        uPlot_options,
        gobj_read_attr(gobj, "data"),
        $container
    );
    gobj_write_attr(gobj, "uplot", uplot);
}

/************************************************************
 *   Destroy UI
 ************************************************************/
function destroy_ui(gobj)
{
    if(gobj_read_bool_attr(gobj, "own_container")) {
        let $container = gobj_read_attr(gobj, "$container");
        if($container) {
            if($container.parentNode) {
                $container.parentNode.removeChild($container);
            }
        }
    }
}

/************************************************************
 *  Function to draw rectangle mode
 ************************************************************/
function rectangle_path(u, sidx, i0, i1)
{
    let s      = u.series[sidx];
    let xdata  = u.data[0];
    let ydata  = u.data[sidx];
    let scaleX = 'x';
    let scaleY = s.scale;

    let stroke = new Path2D();
    stroke.moveTo(Math.round(xdata[0]), Math.round(ydata[0]));

    for (let i = i0; i <= i1 - 1; i++) {
        let x0 = Math.round(u.valToPos(xdata[i], scaleX, true));
        let y0 = Math.round(u.valToPos(ydata[i], scaleY, true));
        let x1 = Math.round(u.valToPos(xdata[i+1], scaleX, true));
        let y1 = Math.round(u.valToPos(ydata[i+1], scaleY, true));

        stroke.lineTo(x0, y0);
        stroke.lineTo(x1, y0);
    }

    let fill = new Path2D(stroke);

    let minY = Math.round(u.valToPos(u.scales[scaleY].min, scaleY, true));
    let minX = Math.round(u.valToPos(u.scales[scaleX].min, scaleX, true));
    let maxX = Math.round(u.valToPos(u.scales[scaleX].max, scaleX, true));

    fill.lineTo(maxX, minY);
    fill.lineTo(minX, minY);

    return {
        stroke,
        fill
    };
}

/************************************************************
 *  Function to draw bezierCurve mode
 ************************************************************/
function beziercurve_path(u, sidx, i0, i1)
{
    let s      = u.series[sidx];
    let xdata  = u.data[0];
    let ydata  = u.data[sidx];
    let scaleX = 'x';
    let scaleY = s.scale;

    let stroke = new Path2D();
    stroke.moveTo(xdata[0], ydata[0]);

    for (let i = i0; i <= i1 - 1; i++) {
        let x0 = u.valToPos(xdata[i], scaleX, true);
        let y0 = u.valToPos(ydata[i], scaleY, true);
        let x1 = u.valToPos(xdata[i+1], scaleX, true);
        let y1 = u.valToPos(ydata[i+1], scaleY, true);

        let midX = (x0 + x1) / 2;
        let midY = (y0 + y1) / 2;

        stroke.bezierCurveTo(midX, y0, midX, y1, x1, y1);
    }

    let fill = new Path2D(stroke);

    let minY = u.valToPos(u.scales[scaleY].min, scaleY, true);
    let minX = u.valToPos(u.scales[scaleX].min, scaleX, true);
    let maxX = u.valToPos(u.scales[scaleX].max, scaleX, true);

    fill.lineTo(maxX, minY);
    fill.lineTo(minX, minY);

    return {
        stroke,
        fill,
    };
}




                    /***************************
                     *      Actions
                     ***************************/




/******************************************************
 *
 ******************************************************/
function ac_load_data(gobj, event, kw, src)
{
    let uplot = gobj_read_attr(gobj, "uplot");

    if(!kw.length) {
        uplot.setData([]);
        return -1;
    }

    let tm = gobj_read_str_attr(gobj, "tm");

    let data = [];
    let max_series = uplot.series.length;

    for (let idx=0; idx < max_series; idx++) {
        data[idx]=[];
    }
    for (let i=0; i<kw.length; i++) {
        data[0].push(kw[i][tm]);

        for(let idx=1 ; idx<uplot.series.length; idx++) {
            let serie = uplot.series[idx];
            let value = kw[i][serie.id];
            if(is_string(value)) {
                if(value.toLowerCase() === "on") {
                    value = 1;
                } else if(value.toLowerCase() === "off") {
                    value = 0;
                } else {
                    log_error(`${gobj_short_name(gobj)} value not contemplated: '${value}'`);
                    value = 0;
                }
            }
            data[idx].push(value);
        }
    }

    //console.log(data)
    uplot.setData(data);

    // create click handlers for cursor points
    let pts = Array.from(uplot.root.querySelectorAll(".cursor-pt"));

    pts.forEach(function(pt, i) {
        pt.onclick = function(e) {
            let seriesIdx = i+1;
            let dataIdx = uplot.cursor.idx;
            let xVal = uplot.data[        0][dataIdx];
            let yVal = uplot.data[seriesIdx][dataIdx];

            let xPos = uplot.valToPos(xVal, 'x');
            let yPos = uplot.valToPos(yVal, 'y');

            let heart = document.createElement("div");
            heart.classList.add("heart");
            heart.style.left = xPos + "px";
            heart.style.top = yPos + "px";
            uplot.root.querySelector(".over").appendChild(heart);
        };
    });

    return 0;
}

/******************************************************
 *
 ******************************************************/
function ac_add_serie(gobj, event, kw, src)
{
    let uplot = gobj_read_attr(gobj, "uplot");

    let idx;
    for(idx=0 ; idx<uplot.series.length; idx++) {
        let serie = uplot.series[idx];
        if(serie.id && serie.id === kw.id) {
            log_error("Serie '" + kw.id + "' already exists");
            return -1;
        }
    }
    let color = {stroke: "Orange", fill: "rgb(255, 165, 0, 0.05)"};
    let colors = gobj_read_attr(gobj, "colors");
    if(idx<colors.length) {
        color = colors[idx];
    }
    kw_get_str(gobj, kw, "stroke", color.stroke, kw_flag_t.KW_CREATE);
    kw_get_str(gobj, kw, "fill", color.fill, kw_flag_t.KW_CREATE);
    //kw["paths"] = beziercurve_path; //rectangle_path;
    uplot.addSeries(kw);
    return 0;
}

/******************************************************
 *
 ******************************************************/
function ac_del_serie(gobj, event, kw, src)
{
    let uplot = gobj_read_attr(gobj, "uplot");

    for(let idx=1; idx<uplot.series.length; idx++) {
        let serie = uplot.series[idx];
        if(serie.id && serie.id === kw.id) {
            uplot.delSeries(idx);
            let data = uplot.data.splice(idx, 1);
            if(data.length===1) {
                data = [[]];
            }
            uplot.setData(data);
            return 0;
        }
    }
    //log_error("Serie '" + kw.id + "' not found");
    return -1;
}

/******************************************************
 *
 ******************************************************/
function ac_clear(gobj, event, kw, src)
{
    let uplot = gobj_read_attr(gobj, "uplot");
    uplot.setData([[]]);
    for(let idx=uplot.series.length-1; idx>0; idx--) {
        uplot.delSeries(idx);
    }
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
    if(__gclass__) {
        log_error(`GClass ALREADY created: ${gclass_name}`);
        return -1;
    }

    /*---------------------------------------------*
     *          States
     *---------------------------------------------*/
    const states = [
        ["ST_IDLE", [
            ["EV_LOAD_DATA",            ac_load_data,           null],
            ["EV_ADD_SERIE",            ac_add_serie,           null],
            ["EV_DEL_SERIE",            ac_del_serie,           null],
            ["EV_CLEAR",                ac_clear,               null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_LOAD_DATA",            0],
        ["EV_ADD_SERIE",            0],
        ["EV_DEL_SERIE",            0],
        ["EV_CLEAR",                0]
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
function register_c_yui_uplot()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_uplot };
