/***********************************************************************
 *          c_ui_device_termod.js
 *
 *          Monitoriza device termodesinfectadora
 *
 *          Hijo de Monitoring_group
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
    gobj_subscribe_event,
    gobj_read_pointer_attr,
    gobj_parent,
    createElement2,
    json_deep_copy,
    kw_get_str,
    is_object,
    clean_name,
    is_string,
    kw_has_key,
    gobj_write_attr,
    gobj_read_attr,
    gobj_unsubscribe_event,
    gobj_send_event,
    gobj_publish_event,
    gobj_name,
    gobj_create_service,
    gobj_find_service,
    gobj_create,
    gobj_start_tree,
    gobj_change_parent,
    gobj_match_children_tree,
    json_array_size,
    gobj_stop_children,
    refresh_language,
} from "yunetas";

import {display_error_message} from "./ui/c_yui_main.js";
import {device_measures} from "./ui_lib_devices.js";

import "tabulator-tables/dist/css/tabulator.min.css"; // Import Tabulator CSS
import "tabulator-tables/dist/css/tabulator_bulma.css";
import { TabulatorFull as Tabulator } from "tabulator-tables"; // Import Full Tabulator JS
// import { Tabulator } from "tabulator-tables";  // Import light Tabulator JS

import {t} from "i18next";
import { DateTime } from "luxon";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_UI_DEVICE_TERMOD";

/***************************************************************
 *              Data
 ***************************************************************/
/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",         0,  null,   "Subscriber of publishing messages"),
SDATA(data_type_t.DTP_POINTER,  "gobj_remote_yuno",   0,  null,   "Remote yuno to ask data"),
SDATA(data_type_t.DTP_STRING,   "device_type",        0,  "",     "Must be 'C_GATE_SONDA-gate_sonda', from parent"),
SDATA(data_type_t.DTP_STRING,   "device_id",          0,  "",     "Device ID, from device"),
SDATA(data_type_t.DTP_STRING,   "device_name",        0,  "",     "Device name, from device"),
SDATA(data_type_t.DTP_STRING,   "device_image",     0,  "",     "Device image, from device"),
SDATA(data_type_t.DTP_BOOLEAN,  "connected",          0,  false,  "Use to show or not the device"),
SDATA(data_type_t.DTP_JSON,     "device",             0,  null,   "Device row, from parent or remote"),
SDATA(data_type_t.DTP_JSON,     "template",           0,  null,   "Template settings, internally or remote"),
SDATA(data_type_t.DTP_JSON,     "alarms_list",        0,  null,   "List of alarms, remote"),
SDATA(data_type_t.DTP_JSON,     "alarms_desc",        0,  null,   "Description of alarms, remote"),
SDATA(data_type_t.DTP_JSON,     "tabulator_settings", 0,  {
    layout: "fitData",
    validationMode: "highlight",
    columnDefaults: { resizable: true },
    maxHeight: "500"
}, "Tabulator settings"),
SDATA(data_type_t.DTP_POINTER,  "tabulator",          0,  null,   "Tabulator instance"),
SDATA(data_type_t.DTP_POINTER,  "$container",         0,  null,   "Container element"),
SDATA(data_type_t.DTP_STRING,   "label",              0,  "monitoring_group", "Label"),
SDATA(data_type_t.DTP_STRING,   "icon",               0,  "fab fa-cloudversify", "Icon class"),
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
    /*
     *  HACK add measures to template
     */
    let device = gobj_read_attr(gobj, "device");
    let template = device.template_settings;
    let device_type = gobj_read_attr(gobj, "device_type");
    set_enums(template, "$device_measures", device_measures(device_type));
    gobj_write_attr(gobj, "template", template);

    start_flow(gobj);
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    gobj_stop_children(gobj);
    unsubscribe_all(gobj);
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
        let device_type = gobj_read_attr(gobj, "device_type");
        let device_name = gobj_read_attr(gobj, "device_name");
        let device_id = gobj_read_attr(gobj, "device_id");
        let device_image = gobj_read_attr(gobj, "device_image");
        let size_icons= 62;

        let $cell = createElement2(
            // WARNING column is display: block
            ['div', {class: `column ${gobj_name(gobj)}`}, [
                ['div', {class: 'card strong-shadow min-width-mobile'}, [
                    /*------------------------------------------*
                     *      Card header
                     *  DEVICE-IMAGE DEVICE-NAME DEVICE-WIFI
                     *------------------------------------------*/
                    ['div', {class: 'card-header'}, [
                        ['div', {class: 'is-flex'}, [
                            // TODO coge la imagen de la tabla device_types
                            ['img', {class: 'DEVICE-IMAGE', src: `${device_image}`, alt: `${device_image}`, width: size_icons, height: size_icons}]
                        ]],
                        ['div', {class: 'card-header-title p-2 is-flex is-flex-direction-column'}, [
                            ['div', {class: ' DEVICE-NAME'}, `${device_name}`],
                            ['div', {class: 'p-1', style: 'font-weight:normal; font-size: 0.60rem !important;'}, `(${device_id})`],
                        ]],

                        ['div', {class: 'is-flex'}, [
                            ['img', {class: 'DEVICE-WIFI', src: '/images/wifi-no.png', alt: '', width: size_icons, height: size_icons}]
                        ]]
                    ]],

                    /*------------------------------------------*
                     *      Card content
                     *------------------------------------------*/
                    ['div', {class: 'card-content p-3'}, [
                        /*---------------------------*
                         *  Top medidas
                         *---------------------------*/
                        ['div', {class: 'buttons BUTTONS-TOP-MEDIDAS'}, []],

                        /*---------------------------*
                         *  Table and Graph
                         *---------------------------*/
                        ['div', {class: 'columns m-0 is-flex-wrap-nowrap COLUMNS-DEVICE'}, [
                            /*---------------------------*
                             *      Table
                             *---------------------------*/
                            ['div', {class: 'column is-narrow'}, [
                                ['div', {class: 'with-border', style: ' overflow:auto;'}, [
                                    ['div', {class: 'TABLE-TERMOD', style: 'height:100%; width:100%;'}]
                                ]]
                            ]]
                        ]],

                        /*---------------------------*
                         *  Bottom toolbar
                         *---------------------------*/
                        ['div', {class: 'buttons mb-0'}, [
                        ]]
                    ]]
                ]]
            ]]
        );
        gobj_write_attr(gobj, "$container", $cell);

        let tabulator_settings = json_deep_copy(gobj_read_attr(gobj, "tabulator_settings"));

        let fields = device_measures(gobj_read_attr(gobj, "device_type"));
        tabulator_settings.columns = fields.map(
            item => ({
                field: item,
                title: t(item),
                width: 350,
            })
        );

        tabulator_settings.columns.unshift({
            field: "tm",
            title: t("time"),
            width: 250,
            formatter: tabulator_formatter_time,
        });
        let tabulator = new Tabulator(
            $cell.querySelector('.TABLE-TERMOD'),
            tabulator_settings
        );
        gobj_write_attr(gobj, "tabulator", tabulator);
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
    // TODO a configuración
    const maxDataPoints = 10;
    const upperLimit = 30.0;
    const lowerLimit = 20.0;
    const margin = 10; // Margin for the y-axis
    const yStepSize = 1; // Fixed step size for the y-axis

    function create_chart(gobj, ctx, field)
    {
        // Borrado
        return null;
    }

    /************************************************************
     *
     ************************************************************/
    function append_data_to_chart(gobj, myChart, track, field)
    {
        let value = track[field];
        switch(field) {
            case "temperature":
                if(value % 1 !== 0) {
                    value = value.toFixed(1);
                }
                break;
        }
        let t = track.tm || track.t;

        const newData = {
            x: (t instanceof Date)? t: new Date(t*1000),
            y: parseFloat(value)
        };

        const data = myChart.data.datasets[0].data;

        if (data.length >= maxDataPoints) {
            data.shift();
        }

        data.push(newData);

        // adjustAxes(myChart); // Adjust y-axis based on the new data
        try {
            myChart.update();
        } catch (e) {
            log_error(e);
        }
    }

    /************************************************************
     *
     ************************************************************/
    function adjustAxes(myChart) {
        const data = myChart.data.datasets[0].data;
        if (data.length === 0) {
            return;
        }

        /*-------------------------*
         *      Adjust Y-Axis
         *-------------------------*/
        const temperatures = data.map(d => d.y);
        const minTemp = Math.min(...temperatures);
        const maxTemp = Math.max(...temperatures);
        const yRange = maxTemp - minTemp;

        // Calculate Y step size based on the range, with a minimum step size
        const yStepSize = Math.max(1, Math.floor(yRange / 15));

        const yMin = Math.floor((minTemp - margin) / yStepSize) * yStepSize;
        const yMax = Math.ceil((maxTemp + margin) / yStepSize) * yStepSize;

        myChart.options.scales.y.min = yMin;
        myChart.options.scales.y.max = yMax;
        myChart.options.scales.y.ticks.stepSize = yStepSize;

        /*-------------------------*
         *      Adjust X-Axis
         *-------------------------*/
        const chartWidth = myChart.width;
        const desiredTickSpacing = 50; // Minimum spacing in pixels between ticks
        const numberOfTicks = Math.floor(chartWidth / desiredTickSpacing);
        const times = data.map(d => d.x.getTime());
        const minTime = Math.min(...times);
        const maxTime = Math.max(...times);
        const xRange = maxTime - minTime;

        // Calculate X step size to have logical divisions and avoid overcrowding
        const xStepSize = Math.ceil(xRange / numberOfTicks / 1000) * 1000; // in milliseconds

        myChart.options.scales.x.time.stepSize = xStepSize / 1000; // Convert milliseconds to seconds
    }

    /************************************************************
     *
     ************************************************************/
    function add_top_medida(gobj, $top_medidas, field, value)
    {
        let size_icons= 30;

        let $measure = $top_medidas.querySelector(`.TOP-MEDIDA-${field}`);
        if(!$measure) {
            $measure = $top_medidas.appendChild(
                createElement2(
                    ['button', {class: `button is-outlined is-link`, style: 'display: flex;'}, [
                        ['img', {
                            src: '/images/thermometer.png',
                            alt: '',
                            width: size_icons,
                            height: size_icons
                        }],
                        ['strong', {class: `TOP-MEDIDA-${field}`}, value]
                    ]]
                )
            );
            $measure = $top_medidas.querySelector(`.TOP-MEDIDA-${field}`);
        }
        if(!$measure) {
            // Something wrong
            return;
        }

        switch(field) {
            case "temperature":
                if(value % 1 !== 0) {
                    value = value.toFixed(1);
                }
                break;
        }

        $measure.textContent = value;
    }

    /************************************************************
     *  From internal click
     ************************************************************/
    function show_alarms(gobj)
    {
        let name = clean_name(gobj_name(gobj));

        let gobj_form = gobj_find_service(`alarms-${name}`);
        if(gobj_form) {
            return;
        }
        gobj_form = gobj_create_service(
            `alarms-${name}`,
            "C_UI_ALARMS",
            {
                subscriber: gobj,
                gobj_remote_yuno: gobj_read_attr(gobj, "gobj_remote_yuno"),
                device_id: gobj_read_attr(gobj, "device_id"),
                settings: gobj_read_attr(gobj, "device").settings,
                alarms_list: gobj_read_attr(gobj, "alarms_list"),
                alarms_desc: gobj_read_attr(gobj, "alarms_desc"),
                with_rownum: false,
            },
            gobj
        );

        let gobj_window = gobj_create(
            "window-alarms",
            "C_YUI_WINDOW",
            {
                $parent: document.getElementById('top-layer'),
                subscriber: gobj_form,
                header: `<div class="has-text-centered">${t("device alarms")}</div>`,
                width: 1000,
                height: 650,
                body: gobj_form
            },
            gobj
        );

        // HACK change parent to destroy gobj_form when gobj_window is closed
        gobj_change_parent(gobj_form, gobj_window);

        gobj_start_tree(gobj_window);
    }

    /************************************************************
     *  From internal click
     ************************************************************/
    function show_settings(gobj)
    {
        let children = gobj_match_children_tree(
            gobj,
            {
                "__gclass_name__": "Yui_form",
                "__gobj_name__": "settings"
            }
        );

        if(json_array_size(children)>0) {
            return;
        }

        let gobj_settings = gobj_create(
            "settings",
            "C_YUI_FORM",
            {
                subscriber: gobj,
                template: gobj_read_attr(gobj, "template"),
                record: gobj_read_attr(gobj, "device").settings,
                editable: true,
                selectable: false,
                with_rownum: true,
                with_delete_row_btn: true,
            },
            gobj
        );

        let gobj_window = gobj_create(
            "window-settings",
            "C_YUI_WINDOW",
            {
                $parent: document.getElementById('top-layer'),
                subscriber: gobj_settings,
                header: `<div class="has-text-centered">${t("device settings")}</div>`,
                width: 1050,
                height: 650,
                body: gobj_settings
            },
            gobj
        );

        // HACK change parent to destroy gobj_form when gobj_window is closed
        gobj_change_parent(gobj_settings, gobj_window);

        gobj_start_tree(gobj_window);
    }

    /************************************************************
     *  From internal click
     ************************************************************/
    function show_historical(gobj)
    {
        // let name = clean_name(gobj_name(gobj));
        //
        // let gobj_form = gobj_find_service(`historical-${name}`);
        // if(gobj_form) {
        //     return;
        // }
        // gobj_form = gobj_create_service(
        //     `historical-${name}`,
        //     Ui_historical,
        //     {
        //         subscriber: gobj,
        //         gobj_remote_yuno: __yuno__.__remote_service__,
        //         device_id: gobj.conf.device_id,
        //         with_rownum: false,
        //     },
        //     gobj
        // );
        //
        // let gobj_window = gobj_create(
        //     "window-historical",
        //     Yui_window,
        //     {
        //         $parent: document.getElementById('top-layer'),
        //         subscriber: gobj_form,
        //         header: `<div class="has-text-centered">${t("historical")}</div>`,
        //         width: 1000,
        //         height: 650,
        //         body: gobj_form
        //     },
        //     gobj
        // );
        //
        // // HACK change parent to destroy gobj_form when gobj_window is closed
        // gobj_change_parent(gobj_form, gobj_window);
        //
        // gobj_start_tree(gobj_window);
    }

    /************************************************************
     *
     ************************************************************/
    function start_flow(gobj)
    {
        let id = gobj_read_attr(gobj, "device_id");
        let gobj_remote_yuno = gobj_read_attr(gobj, "gobj_remote_yuno");

        gobj_send_event(
            gobj_remote_yuno,
            "EV_LIST_ALARMS",
            {
                device_id: id,
            },
            gobj
        );

        gobj_subscribe_event(
            gobj_remote_yuno,
            "EV_REALTIME_TRACK",
            {
                __filter__: [
                    {
                        "id": id
                    }
                ]
            },
            gobj
        );

        gobj_subscribe_event(
            gobj_remote_yuno,
            "EV_REALTIME_ALARM",
            {
                __filter__: [
                    {
                        "id": id
                    }
                ]
            },
            gobj
        );

        gobj_subscribe_event(
            gobj_remote_yuno,
            "EV_UPDATE_DEVICE_SETTINGS",
            {
                __filter__: [
                    {
                        "id": id
                    }
                ]
            },
            gobj
        );

        gobj_subscribe_event(
            gobj_remote_yuno,
            "EV_UPDATE_DEVICE_ALARMS",
            {
                __filter__: [
                    {
                        "id": id
                    }
                ]
            },
            gobj
        );
    }

    /************************************************************
     *
     ************************************************************/
    function unsubscribe_all(gobj)
    {
        let id = gobj_read_attr(gobj, "device_id");
        let gobj_remote_yuno = gobj_read_attr(gobj, "gobj_remote_yuno");

        gobj_unsubscribe_event(
            gobj_remote_yuno,
            "EV_REALTIME_TRACK",
            {
                __filter__: [
                    {
                        "id": id
                    }
                ]
            },
            gobj
        );

        gobj_unsubscribe_event(
            gobj_remote_yuno,
            "EV_REALTIME_ALARM",
            {
                __filter__: [
                    {
                        "id": id
                    }
                ]
            },
            gobj
        );

        gobj_unsubscribe_event(
            gobj_remote_yuno,
            "EV_UPDATE_DEVICE_SETTINGS",
            {
                __filter__: [
                    {
                        "id": id
                    }
                ]
            },
            gobj
        );

        gobj_unsubscribe_event(
            gobj_remote_yuno,
            "EV_UPDATE_DEVICE_ALARMS",
            {
                __filter__: [
                    {
                        "id": id
                    }
                ]
            },
            gobj
        );
    }

    /************************************************************
     *
     ************************************************************/
    function process_device_settings_updated(gobj, template, device)
    {
        gobj_write_attr(gobj, "device", device);

        /*
         *  Update settings if it's open
         */
        let children = gobj_match_children_tree(
            gobj,
            {
                "__gclass_name__": "Yui_form",
                "__gobj_name__": "settings"
            }
        );

        if(json_array_size(children)>0) {
            let gobj_settings = children[0];
            if(gobj_settings) {
                gobj_send_event(
                    gobj_settings,
                    "EV_LOAD_RECORD",
                    device.settings,
                    gobj
                );
            }
        }
    }

    /************************************************************
     *
     ************************************************************/
    function set_enums(template, enum_key, new_value)
    {
        Object.keys(template).forEach(name => {
            let value = template[name];
            if(name === 'enum') {
                if(is_string(value) && value === enum_key) {
                    template[name] = new_value;
                }
            }
            if(is_object(value)) {
                set_enums(value, enum_key, new_value);
            }
        });
    }

    /************************************************************
     *  WARNING code duplicated
     ************************************************************/
    function get_alarm_color(gobj, caso)
    {
        let device = gobj_read_attr(gobj, "device");
        let color; // WARNING code duplicated
        switch (caso) {
            case "active_not_validated":
            case "active_validated":
            case "not_active_not_validated":
                color = kw_get_str(gobj, device.settings, "alarm_colors`" + caso);
                break;
            default:
                color = "hsl(var(--bulma-button-h), var(--bulma-button-s), var(--bulma-button-color-l))";
                break;
        }
        return color;
    }

    /************************************************************
     *
     ************************************************************/
    function update_button_alarm_color(gobj)
    {
    }

    /************************************************************
     *
     ************************************************************/
    function process_realtime_track(gobj, track)
    {
        let $cell = gobj_read_attr(gobj, "$container");

        /*-----------------------------*
         *      Card header
         *-----------------------------*/
        let device = gobj_read_attr(gobj, "device");
        let $wifi_state = $cell.querySelector(`.DEVICE-WIFI`);
        if($wifi_state) {
            // TODO a timeout loop to change the wifi state
            let elapsed = Date.now()/1000 - track.__md_tranger__.tm;
            if(elapsed < 60*10) {
                $wifi_state.src = 'images/wifi-yes.png';
                device.connected = true;
                gobj_write_attr(gobj, "connected", true);
            } else {
                $wifi_state.src = 'images/wifi-no.png';
                device.connected = false;
                gobj_write_attr(gobj, "connected", false);
            }
        }

        /*-----------------------------*
         *      Card content
         *-----------------------------*/
        let device_type = gobj_read_attr(gobj, "device_type");
        let fields = device_measures(device_type);
        if(!fields) {
            return;
        }

        /*-----------------------------*
         *      BUTTONS-TOP-MEDIDAS
         *-----------------------------*/

        /*-----------------------------*
         *      TABLE-TERMOD
         *-----------------------------*/
        let tabulator = gobj_read_attr(gobj, "tabulator");
        if(tabulator.initialized) {
           tabulator.addRow(track);
        } else {
            tabulator.on("tableBuilt", function() {
                tabulator.addRow(track);
            });
        }

        gobj_publish_event(gobj, "EV_REALTIME_TRACK", track);
        return 0;
    }

    /************************************************************
     *
     ************************************************************/
    function process_realtime_alarm(gobj, track)
    {
        /*
         *  Search if alarm exists
         */
        let new_alarm = true;
        let alarms_list = gobj_read_attr(gobj, "alarms_list");
        for(let i=0; i<alarms_list.length; i++) {
            let alarm = alarms_list[i];
            if(alarm.id === track.id && alarm.alarm === track.alarm) {
                alarms_list[i] = track;
                new_alarm = false;
                break;
            }
        }

        if(new_alarm) {
            alarms_list.push(track);
        }

        update_button_alarm_color(gobj);

        /*
         *  Update alarms if it's open
         */
        let name = clean_name(gobj_name(gobj));
        let children = gobj_match_children_tree(
            gobj,
            {
                "__gclass_name__": "Ui_alarms",
                "__gobj_name__": `alarms-${name}`
            }
        );
        if(json_array_size(children)>0) {
            let gobj_alarms = children[0];
            if(gobj_alarms) {
                gobj_send_event(gobj_alarms, "EV_REALTIME_ALARM", track, gobj);
            }
        }

        gobj_publish_event(gobj, "EV_REALTIME_ALARM", track);
        return 0;
    }




                    /***************************
                     *      Actions
                     ***************************/




    /************************************************************
     *  Remote response
     ************************************************************/
    function ac_realtime_track(gobj, event, kw, src)
    {
        let webix_msg = kw;

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

        if(is_object(data)) {
            data = [data];
        }

        for(let i=0; i<data.length; i++) {
            let track = data[i];
            process_realtime_track(gobj, track);
        }

        return 0;
    }

    /************************************************************
     *  Remote response
     ************************************************************/
    function ac_realtime_alarm(gobj, event, kw, src)
    {
        let webix_msg = kw;

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

        process_realtime_alarm(gobj, data);

        return 0;
    }

    /************************************************************
     *  Remote response
     ************************************************************/
    function ac_list_alarms(gobj, event, kw, src)
    {
        let webix_msg = kw;

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

        if(kw_has_key(schema, "cols")) {
            gobj_write_attr(gobj, "alarms_desc", schema.cols);
        } else {
            gobj_write_attr(gobj, "alarms_desc", schema);
        }
        gobj_write_attr(gobj, "alarms_list", data);

        update_button_alarm_color(gobj);

        return 0;
    }

    /************************************************************
     *  Remote response
     ************************************************************/
    function ac_update_device_settings(gobj, event, kw, src)
    {
        let result;
        let comment;
        let schema;
        let data;

        try {
            result = kw.result;
            comment = kw.comment;
            schema = kw.schema;
            data = kw.data;
        } catch (e) {
            log_error(e);
            return -1;
        }
        if(result < 0) {
            display_error_message(
                "Error",
                t(comment)
            );
            return -1;
        }

        process_device_settings_updated(gobj, schema, data);

        return 0;
    }

    /************************************************************
     *  Remote response
     ************************************************************/
    function ac_update_device_alarms(gobj, event, kw, src)
    {
        let result;
        let comment;
        let schema;
        let data;

        try {
            result = kw.result;
            comment = kw.comment;
            schema = kw.schema;
            data = kw.data;
        } catch (e) {
            log_error(e);
            return -1;
        }
        if(result < 0) {
            display_error_message(
                "Error",
                t(comment)
            );
            return -1;
        }

        if(is_object(data)) {
            data = [data];
        }

        for(let i=0; i<data.length; i++) {
            let alarm = data[i];
            process_realtime_alarm(gobj, alarm); // solo cambia el validated
        }

        return 0;
    }

    /************************************************************
     *  From some internal map,
     *  The coordinates of the device have changed, update
     ************************************************************/
    function ac_set_coordinates(gobj, event, kw, src)
    {
        // TODO
        // let coordinates = encod_coordinates(gobj, kw);
        // let record = gobj.config.device.settings;
        // record.coordinates = coordinates;
        // let kw_set_device_settings = {
        //     device_id: gobj.config.device_id,
        //     record: record
        // };
        //
        // let ret = gobj_send_event(gobj_remote_yuno,
        //     "EV_UPDATE_DEVICE_SETTINGS",
        //     kw_set_device_settings,
        //     gobj
        // );
        // if(ret) {
        //     log_error(ret);
        // }

        return 0;
    }

    /************************************************************
     *  From internal click
     ************************************************************/
    function ac_show_alarms(gobj, event, kw, src)
    {
        show_alarms(gobj);
        return 0;
    }

    /************************************************************
     *  From internal click
     ************************************************************/
    function ac_show_settings(gobj, event, kw, src)
    {
        show_settings(gobj);
        return 0;
    }

    /************************************************************
     *  From internal click
     ************************************************************/
    function ac_show_historical(gobj, event, kw, src)
    {
        show_historical(gobj);
        return 0;
    }

    /************************************************************
     *  From external settings form
     ************************************************************/
    function ac_save_record(gobj, event, kw, src)
    {
        let kw_set_device_settings = {
            device_id: gobj_read_attr(gobj, "device_id"),
            record: kw
        };

        let ret = gobj_send_event(
            gobj_read_attr(gobj, "gobj_remote_yuno"),
            "EV_UPDATE_DEVICE_SETTINGS",
            kw_set_device_settings,
            gobj
        );
        if(ret) {
            log_error(ret);
        }

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
            ["EV_REALTIME_TRACK",           ac_realtime_track,          null],
            ["EV_REALTIME_ALARM",           ac_realtime_alarm,          null],
            ["EV_LIST_ALARMS",              ac_list_alarms,             null],
            ["EV_UPDATE_DEVICE_SETTINGS",   ac_update_device_settings,  null],
            ["EV_UPDATE_DEVICE_ALARMS",     ac_update_device_alarms,    null],
            ["EV_SET_COORDINATES",          ac_set_coordinates,         null],
            ["EV_SHOW_ALARMS",              ac_show_alarms,             null],
            ["EV_SHOW_SETTINGS",            ac_show_settings,           null],
            ["EV_SHOW_HISTORICAL",          ac_show_historical,         null],
            ["EV_SAVE_RECORD",              ac_save_record,             null],
            ["EV_REFRESH",                  ac_refresh,                 null],
            ["EV_WINDOW_MOVED",             null,                       null],
            ["EV_WINDOW_RESIZED",           null,                       null],
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
        ["EV_REALTIME_TRACK",           event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
        ["EV_REALTIME_ALARM",           event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
        ["EV_LIST_ALARMS",              event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_UPDATE_DEVICE_SETTINGS",   event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_UPDATE_DEVICE_ALARMS",     event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_SET_COORDINATES",          event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_SHOW_ALARMS",              0],
        ["EV_SHOW_SETTINGS",            0],
        ["EV_SHOW_HISTORICAL",          0],
        ["EV_SAVE_RECORD",              0],
        ["EV_WINDOW_MOVED",             0],
        ["EV_WINDOW_RESIZED",           0],
        ["EV_WINDOW_TO_CLOSE",          0],
        ["EV_REFRESH",                  0],
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
function register_c_ui_device_termod()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_ui_device_termod };
