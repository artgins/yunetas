/***********************************************************************
 *          c_ui_historical_chart.js
 *
 *          Historical and Charts of tracks
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA,
    SDATA_END,
    data_type_t,
    event_flag_t,
    gclass_create,
    log_error,
    gobj_subscribe_event,
    gobj_read_pointer_attr,
    gobj_parent,
    createElement2,
    is_array,
    is_object,
    is_string,
    is_pure_number,
    timeTracker,
    get_now,
    gobj_write_attr,
    gobj_read_attr,
    gobj_send_event,
    gobj_read_str_attr,
    gobj_read_integer_attr,
    gobj_current_state,
    gobj_yuno,
    gobj_write_integer_attr,
    gobj_save_persistent_attrs,
    refresh_language,
} from "yunetas";

import {display_error_message} from "./ui/c_yui_main.js";

import {
    get_months_range,
    get_weeks_range,
    get_days_range,
} from "./ui_lib_time.js";

import "tabulator-tables/dist/css/tabulator.min.css"; // Import Tabulator CSS
import "tabulator-tables/dist/css/tabulator_bulma.css";
import { TabulatorFull as Tabulator } from "tabulator-tables"; // Import Full Tabulator JS
// import { Tabulator } from "tabulator-tables";  // Import light Tabulator JS

import {t} from "i18next";
import { DateTime } from "luxon";

// Import uPlot
import uPlot from "uplot";
import "uplot/dist/uPlot.min.css";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_UI_HISTORICAL_CHART";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of publishing messages"),
SDATA(data_type_t.DTP_POINTER,  "gobj_remote_yuno", 0,  null,   "Remote yuno to ask data"),
SDATA(data_type_t.DTP_STRING,   "device_id",        0,  "",     "Device ID from device"),
SDATA(data_type_t.DTP_STRING,   "device_name",      0,  "",     "Device name from device"),
SDATA(data_type_t.DTP_JSON,     "schema",           0,  null,   "Schema for the table"),
SDATA(data_type_t.DTP_STRING,   "theme",            0,  "",     "Theme"),
SDATA(data_type_t.DTP_STRING,   "range_base",       0,  "day",  "Base range for data"),
SDATA(data_type_t.DTP_INTEGER,  "from_t",           0,  "0",    "Start timestamp"),
SDATA(data_type_t.DTP_INTEGER,  "to_t",             0,  "0",    "End timestamp"),
SDATA(data_type_t.DTP_JSON,     "charts",           0,  "{}",   "List of charts with their handlers"),
SDATA(data_type_t.DTP_JSON,     "chart_colors",     0,  [
    {stroke:"blue",     fill: "rgba(0,0,255,0.05)"},
    {stroke:"green",    fill: "rgba(0,255,0,0.05)"},
    {stroke:"orange",   fill: "rgba(255,165,0,0.1)"},
    {stroke:"#00FF00",  fill: "rgba(0,255,0,0.05)"},
    {stroke:"#0000FF",  fill: "rgba(0,0,255,0.05)"},
    {stroke:"#FFA500",  fill: "rgba(255,165,0,0.05)"},
    {stroke:"#800080",  fill: "rgba(128,0,128,0.05)"},
    {stroke:"#FF0000",  fill: "rgba(255,0,0,0.05)"},
    {stroke:"#008000",  fill: "rgba(0,128,0,0.05)"},
    {stroke:"#FF1493",  fill: "rgba(255,20,147,0.05)"},
    {stroke:"#00CED1",  fill: "rgba(0,206,209,0.05)"},
    {stroke:"#FF4500",  fill: "rgba(255,69,0,0.05)"},
    {stroke:"#8A2BE2",  fill: "rgba(138,43,226,0.05)"},
    {stroke:"#A52A2A",  fill: "rgba(165,42,42,0.05)"},
    {stroke:"#5F9EA0",  fill: "rgba(95,158,160,0.05)"},
    {stroke:"#D2691E",  fill: "rgba(210,105,30,0.05)"},
    {stroke:"#32CD32",  fill: "rgba(50,205,50,0.05)"},
    {stroke:"#FF6347",  fill: "rgba(255,99,71,0.05)"},
    {stroke:"#4682B4",  fill: "rgba(70,130,180,0.05)"},
    {stroke:"#DAA520",  fill: "rgba(218,165,32,0.05)"},
    {stroke:"#9932CC",  fill: "rgba(153,50,204,0.05)"},
    {stroke:"#3CB371",  fill: "rgba(60,179,113,0.05)"}
], "Color settings for charts"),
SDATA(data_type_t.DTP_INTEGER,  "subcell_width",    0,  "500",  "Width of subcell"),
SDATA(data_type_t.DTP_INTEGER,  "subcell_height",   0,  "400",  "Height of subcell"),
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Container element"),
SDATA(data_type_t.DTP_STRING,   "label",            0,  "monitoring_group", "Label"),
SDATA(data_type_t.DTP_STRING,   "icon",             0,  "fa-solid fa-chart-line", "Icon class"),
SDATA(data_type_t.DTP_POINTER,  "tabulator",        0,  null,   "Tabulator instance"),
SDATA(data_type_t.DTP_POINTER,  "uplot_chart",      0,  null,   "Uplot chart instance"),

SDATA(data_type_t.DTP_INTEGER,  "uplot_width",      0,  0,      ""),
SDATA(data_type_t.DTP_INTEGER,  "uplot_height",     0,  0,      ""),
SDATA(data_type_t.DTP_INTEGER,  "ytable_width",     0,  0,      ""),
SDATA(data_type_t.DTP_INTEGER,  "ytable_height",    0,  0,      ""),

SDATA_END()
];

let PRIVATE_DATA = {
    selectedColumns: null,
    time_tracker: null,
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
    let priv = gobj.priv;

    priv.selectedColumns = new Set();


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
    let $container = gobj_read_attr(gobj, "$container");
    let $elm = $container.querySelector('.CHART-UPLOT');
    gobj_write_attr(gobj, "uplot_width", $elm.offsetWidth);
    gobj_write_attr(gobj, "uplot_height", $elm.offsetHeight);

    $elm = $container.querySelector('.TABLE-YTABLE');
    gobj_write_attr(gobj, "ytable_width", $elm.offsetWidth);
    gobj_write_attr(gobj, "ytable_height", $elm.offsetHeight);

    gobj_send_event(gobj, "EV_TIME_RANGE", {range:"range-day"}, gobj);
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
    let tabulator = gobj_read_attr(gobj, "tabulator");
    if(tabulator) {
        tabulator.destroy();
        gobj_write_attr(gobj, "tabulator", null);
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
    let $cell = createElement2(
        ['div', {class: 'is-flex is-flex-direction-column', style:'height:100%;'}, [
            /*---------------------------*
             *  Search toolbar
             *---------------------------*/
            ['section', {class: 'section with-border p-3'}, [
                ['div', {class: 'columns m-0'}, [
                    ['div', {class: 'column is-narrow'}, [
                        ['div', {class: 'control'}, [
                            /*---------------------------*
                             *  Date From
                             *---------------------------*/
                            ['div', {class: 'field has-addons'}, [
                                ['div', {class: 'field-label is-normal' }, [
                                    ['label', {class:'label ', i18n:'from'}, 'from']
                                ]],
                                ['div', {class: 'field-body'}, [
                                    ['div', {class: 'field'}, [
                                        ['div', {class: 'control' }, [
                                            ['input', {class:'input search-from-time', type:'text', readonly:true}, '']
                                        ]]
                                    ]]
                                ]]
                            ]],
                        ]],
                    ]],

                    ['div', {class: 'column is-narrow'}, [
                        ['div', {class: 'control'}, [
                            /*---------------------------*
                             *  Date To
                             *---------------------------*/
                            ['div', {class: 'field has-addons'}, [
                                ['div', {class: 'field-label is-normal' }, [
                                    ['label', {class:'label ', i18n:'to'}, 'to']
                                ]],
                                ['div', {class: 'field-body'}, [
                                    ['div', {class: 'field'}, [
                                        ['div', {class: 'control' }, [
                                            ['input', {class:'input search-to-time', type:'text', readonly:true}, '']
                                        ]]
                                    ]]
                                ]]
                            ]],
                        ]],
                    ]],
                ]],

                ['div', {class: 'columns m-0'}, [
                    ['div', {class: 'column is-narrow'}, [
                        ['div', {class: 'buttons'}, [
                            ['div', {class: 'level-item'}, [
                                /*---------------------------*
                                 *  Button Decrement
                                 *---------------------------*/
                                ['button', {class: 'button'}, [
                                    ['span', {class: 'icon'}, [
                                        ['i', {class: 'fas fa-minus-square'}]
                                    ]],
                                    // ['span', {class: '', i18n: 'today'}, 'today']
                                ], {
                                    'click': function(evt) {
                                        evt.stopPropagation();
                                        gobj_send_event(gobj, "EV_TIME_DECREMENT", {}, gobj);
                                    }
                                }],
                            ]],
                            ['div', {class: 'level-item'}, [
                                /*---------------------------*
                                 *  Button Day/Week/Month
                                 *---------------------------*/
                                ['div', {class: 'buttons has-addons buttons-select-range'}, [
                                    ['button', {class: 'button range-day', i18n: 'day'}, 'day', {
                                        click: function(evt) {
                                            evt.stopPropagation();
                                            gobj_send_event(gobj, "EV_TIME_RANGE", {range:"range-day"}, gobj);
                                        }
                                    }],
                                    ['button', {class: 'button range-week', i18n: 'week'}, 'week', {
                                        click: function(evt) {
                                            evt.stopPropagation();
                                            gobj_send_event(gobj, "EV_TIME_RANGE", {range:"range-week"}, gobj);
                                        }
                                    }],
                                    ['button', {class: 'button range-month', i18n: 'month'}, 'month', {
                                        click: function(evt) {
                                            evt.stopPropagation();
                                            gobj_send_event(gobj, "EV_TIME_RANGE", {range:"range-month"}, gobj);
                                        }
                                    }],
                                ]],
                            ]],
                            ['div', {class: 'level-item'}, [
                                /*---------------------------*
                                 *  Button Increment
                                 *---------------------------*/
                                ['button', {class: 'button'}, [
                                    ['span', {class: 'icon'}, [
                                        ['i', {class: 'fas fa-plus-square'}]
                                    ]],
                                    // ['span', {class: '', i18n: 'today'}, 'today']
                                ], {
                                    'click': function(evt) {
                                        evt.stopPropagation();
                                        gobj_send_event(gobj, "EV_TIME_INCREMENT", {}, gobj);
                                    }
                                }],
                            ]],
                        ]],
                    ]],

                    ['div', {class: 'column is-narrow'}, [
                        ['div', {class: 'control'}, [
                            /*---------------------------*
                             *  Button Search
                             *---------------------------*/
                            ['p', {class: 'control'}, [
                                ['button', {class: 'button px-5 is-warning button-search'}, [
                                    ['span', {class: 'icon'}, [
                                        ['i', {class: 'fas fa-search'}]
                                    ]],
                                    ['span', {
                                            class: '',
                                            i18n: `search ${gobj_read_str_attr(gobj, "theme")}`
                                    },
                                    `search ${gobj_read_str_attr(gobj, "theme")}`,
                                    {
                                        'click': function(evt) {
                                            evt.stopPropagation();
                                            gobj_send_event(gobj, "EV_SEARCH", {}, gobj);
                                        }
                                    }]
                                ]],
                                ['span',
                                    {
                                        class: 'yui-badge tag ml-4 is-medium is-hidden',
                                    },
                                    ''
                                ]
                            ]],
                        ]],
                    ]],
                ]],
            ]],

            /*---------------------------*
             *      TABLE and CHART
             *---------------------------*/
            // ['hr', {class:"is-divider m-0"}], // SEPARATOR LINE

            ['section', {class: 'section with-border p-0 is-flex-grow-1'}, [
                ['div', {class: 'columns m-0', style:'height:100%;'}, [
                    /*---------------------------*
                     *      Table
                     *---------------------------*/
                    ['div', {class: 'column is-half', style:'height:100%;'}, [
                        ['div',
                            {
                                class: 'with-border TABLE-YTABLE',
                                style: `width: 100%;height:100%;`
                            }
                        ]
                    ]],

                    /*---------------------------*
                     *      GRAPH Chart
                     *---------------------------*/
                    ['div', {class: 'column is-half', style:'height:100%;'}, [
                        ['div', {class: 'uplot-wrapper', style: `width: 100%;height:100%;`}, [
                            ['div',
                                {
                                    class: 'CHART-UPLOT with-border',
                                    style: `width: 100%;height:100%;`
                                }
                            ]
                        ]]
                    ]]
                ]],
            ]]
        ]]
    );

    gobj_write_attr(gobj, "$container", $cell);
    refresh_language($cell, t);
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
function tabulator_formatter_time(cell, formatterParams)
{
    let value = cell.getValue();
    value = parseInt(value) || 0;
    value = DateTime.fromSeconds(value).toFormat('yyyy-LL-dd HH:mm:ss');

    return '<span class="is-size-7">' + value + '</span>';
}

/************************************************************
 *
 ************************************************************/
// Toggle column selection
function toggleColumnSelection(gobj, column)
{
    let priv = gobj.priv;

    const field = column.getField();
    const headerElement = column.getElement().querySelector(".tabulator-col-content");

    if (priv.selectedColumns.has(field)) {
        // Unselect the column
        priv.selectedColumns.delete(field);
        headerElement.classList.remove("is-success");
    } else {
        // Select the column
        priv.selectedColumns.add(field);
        headerElement.classList.add("is-success");
    }
}

/************************************************************
 *
 ************************************************************/
function create_tabulator(gobj)
{
    let priv = gobj.priv;

    /*-------------------------------*
     *      Schema
     *-------------------------------*/
    let columns = [];

    let schema = gobj_read_attr(gobj, "schema");
    if(is_array(schema)) {
        for(let i=0; i<schema.length; i++) {
            const col = schema[i];
            let key = "";
            if(is_string(col)) {
                key = col;
            } else if(is_object(col)) {
                key = col.id;
            } else {
                log_error("What col type?");
                continue;
            }
            if(key.charAt(0) === '_') {
                continue;
            }
            if(key === 'tm') {
                continue;
            }
            columns.push(
                {
                    // fillspace: value.fillspace,
                    field: key,
                    title: t(col.header? col.header:key),
                    headerSort: false,
                    headerClick: (e, column) => {
                        toggleColumnSelection(gobj, column);
                        const selectedColumnArray = Array.from(priv.selectedColumns);
                        update_uplot_series(
                            gobj,
                            selectedColumnArray,  // columns
                            null   // filters
                        );
                    },
                }
            );
        }
    } else if(is_object(schema)) {
        for (const key of Object.keys(schema)) {
            const col = schema[key];
            if (key.charAt(0) === '_') {
                continue;
            }
            if(key === 'tm') {
                continue;
            }
            columns.push(
                {
                    // fillspace: value.fillspace,
                    field: key,
                    title: t(col.header),
                    headerSort: false,
                    headerClick: (e, column) => {
                        toggleColumnSelection(gobj, column);
                        const selectedColumnArray = Array.from(priv.selectedColumns);
                        update_uplot_series(
                            gobj,
                            selectedColumnArray,  // columns
                            null   // filters
                        );
                    },
                }
            );
        }
    } else {
        log_error("bad schema variable");
    }

    columns.unshift({
        field: "tm",
        title: t("time"),
        headerSort: false,
        // fillspace: 2.5,
        formatter: tabulator_formatter_time,
        headerClick: (e, column) => {
            toggleColumnSelection(gobj, column);
            const selectedColumnArray = Array.from(priv.selectedColumns);
            update_uplot_series(
                gobj,
                selectedColumnArray,  // columns
                null   // filters
            );
        }
    });

    let tabulator_settings = {
        layout: "fitData",
            validationMode: "highlight",
            columnDefaults: {
            resizable: true,
        },
        height: gobj_read_integer_attr(gobj, "ytable_height") + "px",
        width: gobj_read_integer_attr(gobj, "ytable_width") + "px",
    };

    tabulator_settings.columns = columns;

    let $container = gobj_read_attr(gobj, "$container");
    let tabulator = new Tabulator(
        $container.querySelector('.TABLE-YTABLE'),
        tabulator_settings
    );
    gobj_write_attr(gobj, "tabulator", tabulator);
    return tabulator;
}

/************************************************************
 *  Uplot interface
 ************************************************************/
function uplot_create_chart(gobj, name, series=[], data=[[]], axes=[{}], scales={})
{
    let charts = gobj_read_attr(gobj, "charts");
    if(charts[name]) {
        charts[name].destroy();
        charts[name] = null;
    }

    if(!charts[name]) {
        let opts = {
            height: gobj_read_integer_attr(gobj, "uplot_height") - 30,
            width: gobj_read_integer_attr(gobj, "uplot_width"),
            series: [{}, ...series],
            axes: axes,
            scales: scales,
        };

        let $container = gobj_read_attr(gobj, "$container");
        charts[name] = new uPlot(
            opts,
            data,
            $container.querySelector('.CHART-UPLOT')
        );
    }
    return charts[name];
}

/************************************************************
 *
 ************************************************************/
function get_index_color(gobj, records, measure)
{
    if(!records.length) {
        return 0;
    }
    let record = records[0];
    let i = 0;
    let chart_colors = gobj_read_attr(gobj, "chart_colors");
    for (const key of Object.keys(record)) {
        if(key === measure) {
            if(i>=chart_colors.length-1) {
                return 0;
            }
            return i+1;
        }
        i++;
    }
    return 0;
}

/************************************************************
 *
 ************************************************************/
function update_uplot_series(gobj, columns, filters)
{
    let tabulator = gobj_read_attr(gobj, "tabulator");
    let records = tabulator.getData();
    let chart_colors = gobj_read_attr(gobj, "chart_colors");
    let theme = gobj_read_str_attr(gobj, "theme");

    /*
     *  Build series
     */
    let series = [];
    for(let m=0; m<columns.length; m++) {
        let measure = columns[m];
        let color = chart_colors[get_index_color(gobj, records, measure)];
        let serie = {
            label: t(measure),
            show: true,
        };
        Object.assign(serie, color);
        series.push(serie);
    }

    /*
     *  Build data
     */
    let data = [[]];    // include first array of times
    for(let m=0; m<columns.length; m++) {
        data.push([]);
    }
    for(let r=0; r<records.length; r++) {
        let record = records[r];
        for(let m=0; m<columns.length; m++) {
            let measure = columns[m];
            if(m===0) {
                data[0].push(record["tm"]);
            }
            let value = record[measure];
            if(!is_pure_number(value)) {
                if(is_string(value)) {
                    value = value.toLowerCase();
                }
                switch(value) {
                    case true:
                    case 'on':
                        value = 1;
                        break;

                    case false:
                    case 'off':
                    default:
                        value = 0;
                        break;
                }
            }
            data[m+1].push(value);
        }
    }

    // console.dir("COLUMNS", columns);
    // console.dir("FILTERS", filters);

    uplot_create_chart(gobj, theme, series, data);
}

/************************************************************
 *
 ************************************************************/
function save_time_from_to(gobj)
{
    let from_t = gobj_read_integer_attr(gobj, "from_t");
    let to_t = gobj_read_integer_attr(gobj, "to_t");

    let $container = gobj_read_attr(gobj, "$container");
    let $input_from = $container.querySelector('.search-from-time');
    let $input_to = $container.querySelector('.search-to-time');

    $input_from.value = DateTime.fromSeconds(from_t).toFormat('yyyy/MM/dd HH:mm:ss ');
    $input_to.value = DateTime.fromSeconds(to_t).toFormat('yyyy/MM/dd HH:mm:ss ');

    gobj_save_persistent_attrs(gobj);
}

/************************************************************
 *
 ************************************************************/
function get_records(gobj, id, from, to)
{
    let priv = gobj.priv;

    if(!id) {
        return;
    }
    let gobj_remote_yuno = gobj_read_attr(gobj, "gobj_remote_yuno");
    if(gobj_current_state(gobj_remote_yuno)!=="ST_SESSION") {
        return;
    }

    let kw_tracks = {
        "key": id,
        "from_t": from,
        "to_t": to
    };

    let theme = gobj_read_str_attr(gobj, "theme");
    let remote_event = "";
    switch(theme) {
        case "measures":
            remote_event = "EV_LIST_TRACKS";
            break;
        case "alarms":
            remote_event = "EV_LIST_ALARM_HISTORIC";
            break;
        default:
            log_error("Theme unknown: " + theme);
            return;
    }

    if(gobj_read_attr(gobj_yuno(), "tracing")) {
        priv.time_tracker = timeTracker(remote_event, true);
    } else {
        priv.time_tracker = timeTracker();
    }

    gobj_send_event(
        gobj_read_attr(gobj, "gobj_remote_yuno"),
        remote_event,
        kw_tracks,
        gobj
    );

    let $container = gobj_read_attr(gobj, "$container");
    let $btn = $container.querySelector('.yui-badge');
    if($btn) {
        $btn.classList.add("is-hidden");
    }

    /*
     *  Create the table if not exist
     */
    let tabulator = gobj_read_attr(gobj, "tabulator");
    if(!tabulator) {
        tabulator = create_tabulator(gobj);
    } else {
        tabulator.clearData();
    }
}

/************************************************************
 *
 ************************************************************/
function process_list_tracks(gobj, data)
{
    let priv = gobj.priv;
    let tracks = data;   // Save current list

    if(gobj_read_attr(gobj_yuno(), "tracing")) {
        priv.time_tracker.mark("response", `${data.length} records`);
    }

    let $container = gobj_read_attr(gobj, "$container");
    let $btn = $container.querySelector('.yui-badge');
    if($btn) {
        $btn.classList.remove("is-hidden");
        const n = priv.time_tracker.mark();
        $btn.textContent = data.length + " " + t("records") + ", " + n.total + " seconds";
    }

    let tabulator = gobj_read_attr(gobj, "tabulator");
    if(tabulator.initialized) {
        /*
         *  Load table
         */
        tabulator.setData(tracks);

        if(gobj_read_attr(gobj_yuno(), "tracing")) {
            priv.time_tracker.mark("loaded table", `${data.length} records`);
        }

        /*
         *  Load chart
         */
        const selectedColumnArray = Array.from(priv.selectedColumns);
        update_uplot_series(
            gobj,
            selectedColumnArray,  // columns
            null   // filters
        );

        if(gobj_read_attr(gobj_yuno(), "tracing")) {
            priv.time_tracker.mark("loaded graph", `${data.length} records`);
        }

    } else {
        tabulator.on("tableBuilt", function() {

            /*
             *  Load table
             */
            tabulator.setData(tracks, false);

            if(gobj_read_attr(gobj_yuno(), "tracing")) {
                priv.time_tracker.mark("loaded table", `${data.length} records`);
            }

            /*
             *  Load chart
             */
            const selectedColumnArray = Array.from(priv.selectedColumns);
            update_uplot_series(
                gobj,
                selectedColumnArray,  // columns
                null   // filters
            );

            if(gobj_read_attr(gobj_yuno(), "tracing")) {
                priv.time_tracker.mark("loaded graph", `${data.length} records`);
            }
        });
    }
}

/************************************************************
 *
 ************************************************************/
function process_list_alarms(gobj, data, schema)
{
    let priv = gobj.priv;
    let alarms = data;  // Save current list

    if(gobj_read_attr(gobj_yuno(), "tracing")) {
        priv.time_tracker.mark("response", `${data.length} records`);
    }

    if(schema) {
        gobj_write_attr(gobj, "schema", schema);
    }

    let $container = gobj_read_attr(gobj, "$container");
    let $btn = $container.querySelector('.yui-badge');
    if($btn) {
        $btn.classList.remove("is-hidden");
        const n = priv.time_tracker.mark();
        $btn.textContent = data.length + " " + t("records") + ", " + n.total + " seconds";
    }

    /*
     *  Create the table if not exist
     */
    let tabulator = gobj_read_attr(gobj, "tabulator");
    if(!tabulator) {
        tabulator = create_tabulator(gobj);
    }

    if(tabulator.initialized) {
        tabulator.setData(alarms);
        const selectedColumnArray = Array.from(priv.selectedColumns);
        update_uplot_series(
            gobj,
            selectedColumnArray,  // columns
            null   // filters
        );
    } else {
        tabulator.on("tableBuilt", function() {
            tabulator.setData(alarms);
            const selectedColumnArray = Array.from(priv.selectedColumns);
            update_uplot_series(
                gobj,
                selectedColumnArray,  // columns
                null   // filters
            );
        });
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *  Remote response
 ************************************************************/
function ac_list_tracks(gobj, event, kw, src)
{
    let webix_msg = kw;
    let $container = gobj_read_attr(gobj, "$container");
    let $button = $container.querySelector(`.button-search`);
    $button.disabled = false;
    $button.classList.remove("is-loading");

    let result;
    let comment;
    let schema;
    let data;

    try {
        result = webix_msg.result;
        comment = webix_msg.comment;
        schema = webix_msg.schema;
        data = webix_msg.data;
    } catch (e) {
        log_error("" + e);
        return;
    }
    if(result < 0) {
        display_error_message("Error", comment);
        return 0;
    }

    process_list_tracks(gobj, data);

    return 0;
}

/************************************************************
 *  Remote response
 ************************************************************/
function ac_list_alarm_historic(gobj, event, kw, src)
{
    let webix_msg = kw;

    let $container = gobj_read_attr(gobj, "$container");
    let $button = $container.querySelector(`.button-search`);
    $button.disabled = false;
    $button.classList.remove("is-loading");

    let result;
    let comment;
    let schema;
    let data;

    try {
        result = webix_msg.result;
        comment = webix_msg.comment;
        schema = webix_msg.schema;
        data = webix_msg.data;
    } catch (e) {
        log_error("" + e);
        return;
    }
    if(result < 0) {
        display_error_message("Error", comment);
        return 0;
    }

    process_list_alarms(gobj, data, schema);

    return 0;
}

/************************************************************
 *  Clicked the button search
 ************************************************************/
function ac_search(gobj, event, kw, src)
{
    let $container = gobj_read_attr(gobj, "$container");
    let $button = $container.querySelector(`.button-search`);
    $button.disabled = true;
    $button.classList.add("is-loading");

    /*
     *  Reset the graph
     */
    uplot_create_chart(
        gobj,
        gobj_read_str_attr(gobj, "theme")
    );

    /*
     *  Ask for data
     */
    get_records(
        gobj,
        gobj_read_str_attr(gobj, "device_id"),
        gobj_read_integer_attr(gobj, "from_t"),
        gobj_read_integer_attr(gobj, "to_t")
    );

    return 0;
}

/************************************************************
 *  Han seleccionado range en el calendar
 ************************************************************/
function ac_select_time_from_to(gobj, event, kw, src)
{
    if (kw.from_t === kw.to_t){
        let range = get_days_range(kw.from_t, 1);
        gobj_write_integer_attr(gobj, "from_t", range.start);
        gobj_write_integer_attr(gobj, "to_t", range.end);
    }else{
        gobj_write_integer_attr(gobj, "from_t", kw.from_t);
        gobj_write_integer_attr(gobj, "to_t", kw.to_t);
    }
    save_time_from_to(gobj);

    return 0;
}

/************************************************************
 *  Han seleccionado range en los
 *  buttons day/week/month
 ************************************************************/
function ac_time_range(gobj, event, kw, src)
{
    let $container = gobj_read_attr(gobj, "$container");
    let buttons = $container.querySelectorAll(".buttons-select-range .button");
    for (let btn of buttons) {
        btn.classList.remove("is-success", "is-selected");
    }

    let cls = kw.range;
    gobj_write_attr(gobj, "range_base", cls);

    let button = $container.querySelector(`.buttons-select-range .${cls}`);
    button.classList.add("is-success", "is-selected");

    let range;
    switch(kw.range) {
        case "range-week":
            range = get_weeks_range(get_now(), 1);
            gobj_write_integer_attr(gobj, "from_t", range.start);
            gobj_write_integer_attr(gobj, "to_t", range.end);
            break;
        case "range-month":
            range = get_months_range(get_now(), 1);
            gobj_write_integer_attr(gobj, "from_t", range.start);
            gobj_write_integer_attr(gobj, "to_t", range.end);
            break;
        case "range-day":
        default:
            range = get_days_range(get_now(), 1);
            gobj_write_integer_attr(gobj, "from_t", range.start);
            gobj_write_integer_attr(gobj, "to_t", range.end);
            break;
    }

    save_time_from_to(gobj);

    return 0;
}

/************************************************************
 *  Han seleccionado increment en el button +
 ************************************************************/
function ac_time_increment(gobj, event, kw, src)
{
    let range;
    let range_base = gobj_read_attr(gobj, "range_base");
    let from_t = gobj_read_integer_attr(gobj, "from_t");
    switch(range_base) {
        case "range-day":
            from_t = DateTime.fromSeconds(from_t).plus({ days: 1 }).toSeconds();
            range = get_days_range(from_t, 1);
            gobj_write_integer_attr(gobj, "from_t", range.start);
            gobj_write_integer_attr(gobj, "to_t", range.end);
            break;
        case "range-week":
            from_t = DateTime.fromSeconds(from_t).plus({ weeks: 1 }).toSeconds();
            range = get_weeks_range(from_t, 1);
            gobj_write_integer_attr(gobj, "from_t", range.start);
            gobj_write_integer_attr(gobj, "to_t", range.end);
            break;
        case "range-month":
            from_t = DateTime.fromSeconds(from_t).plus({ months: 1 }).toSeconds();
            range = get_months_range(from_t, 1);
            gobj_write_integer_attr(gobj, "from_t", range.start);
            gobj_write_integer_attr(gobj, "to_t", range.end);
            break;
        default:
            log_error("range unknown: " + kw.range);
            return -1;
    }

    save_time_from_to(gobj);

    return 0;
}

/************************************************************
 *  Han seleccionado decrement en el button -
 ************************************************************/
function ac_time_decrement(gobj, event, kw, src)
{
    let range;
    let range_base = gobj_read_attr(gobj, "range_base");
    let from_t = gobj_read_integer_attr(gobj, "from_t");
    switch(range_base) {
        case "range-day":
            from_t = DateTime.fromSeconds(from_t).minus({ days: 1 }).toSeconds();
            range = get_days_range(from_t, 1);
            gobj_write_integer_attr(gobj, "from_t", range.start);
            gobj_write_integer_attr(gobj, "to_t", range.end);
            break;
        case "range-week":
            from_t = DateTime.fromSeconds(from_t).minus({ weeks: 1 }).toSeconds();
            range = get_weeks_range(from_t, 1);
            gobj_write_integer_attr(gobj, "from_t", range.start);
            gobj_write_integer_attr(gobj, "to_t", range.end);
            break;
        case "range-month":
            from_t = DateTime.fromSeconds(from_t).minus({ months: 1 }).toSeconds();
            range = get_months_range(from_t, 1);
            gobj_write_integer_attr(gobj, "from_t", range.start);
            gobj_write_integer_attr(gobj, "to_t", range.end);
            break;
        default:
            log_error("range unknown: " + kw.range);
            return -1;
    }

    save_time_from_to(gobj);

    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_refresh(gobj, event, kw, src)
{
    // get_records(gobj, device_id, from_t, to_t);
    return 0;
}

/************************************************************
 *  Window resize
 ************************************************************/
function ac_resize(gobj, event, kw, src)
{
    return 0;
}

/************************************************************
 *  Order from parent to show
 ************************************************************/
function ac_show(gobj, event, kw, src)
{
    // WARNING container (class column) is display: block
    let $container = gobj_read_attr(gobj, "$container");
    $container.style.display = 'block';
    return 0;
}

/************************************************************
 *  Order from parent to hide
 ************************************************************/
function ac_hide(gobj, event, kw, src)
{
    let $container = gobj_read_attr(gobj, "$container");
    $container.style.display = 'none';
    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_timeout(gobj, event, kw, src)
{
    //gobj.set_timeout(1*1000);
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
            ["EV_LIST_TRACKS",              ac_list_tracks,             null],
            ["EV_LIST_ALARM_HISTORIC",      ac_list_alarm_historic,     null],
            ["EV_SEARCH",                   ac_search,                  null],
            ["EV_SELECT_TIME_FROM_TO",      ac_select_time_from_to,     null],
            ["EV_TIME_RANGE",               ac_time_range,              null],
            ["EV_TIME_INCREMENT",           ac_time_increment,          null],
            ["EV_TIME_DECREMENT",           ac_time_decrement,          null],
            ["EV_REFRESH",                  ac_refresh,                 null],
            ["EV_WINDOW_MOVED",             null,                       null],
            ["EV_WINDOW_RESIZED",           ac_resize,                  null],
            ["EV_WINDOW_TO_CLOSE",          null,                       null],
            ["EV_SHOW",                     ac_show,                    null],
            ["EV_HIDE",                     ac_hide,                    null],
            ["EV_TIMEOUT",                  ac_timeout,                 null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_LIST_TRACKS",              event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_LIST_ALARM_HISTORIC",      event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_SEARCH",                   0],
        ["EV_SELECT_TIME_FROM_TO",      0],
        ["EV_TIME_RANGE",               0],
        ["EV_TIME_INCREMENT",           0],
        ["EV_TIME_DECREMENT",           0],
        ["EV_REFRESH",                  0],
        ["EV_WINDOW_MOVED",             0],
        ["EV_WINDOW_RESIZED",           0],
        ["EV_WINDOW_TO_CLOSE",          0],
        ["EV_SHOW",                     0],
        ["EV_HIDE",                     0],
        ["EV_TIMEOUT",                  0]
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
        0   // gclass_flag (not using gcflag_manual_start)
    );

    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************
 *          Register GClass
 ***************************************************************/
function register_c_ui_historical_chart()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_ui_historical_chart };
