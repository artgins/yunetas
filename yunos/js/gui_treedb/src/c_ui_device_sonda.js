/***********************************************************************
 *          c_ui_device_sonda.js
 *
 *          Monitoriza device sonda
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
    kw_flag_t,
    log_error,
    gobj_subscribe_event,
    gobj_read_pointer_attr,
    gobj_parent,
    createElement2,
    sprintf,
    kw_get_str,
    is_object,
    clean_name,
    is_string,
    kw_has_key,
    gobj_write_attr,
    gobj_read_attr,
    gobj_create,
    gobj_unsubscribe_event,
    gobj_send_event,
    gobj_publish_event,
    set_timeout,
    gobj_create_pure_child,
    gobj_name,
    gobj_start,
    clear_timeout,
    gobj_stop,
    gobj_match_children_tree,
    json_array_size,
    gobj_find_service,
    gobj_destroy,
    gobj_create_service,
    gobj_change_parent,
    gobj_start_tree,
    gobj_stop_children,
    refresh_language,
    is_number,
    gobj_command,
    gobj_short_name,
    gobj_read_str_attr,
    msg_iev_get_stack,
    kw_get_dict,
} from "yunetas";

import {display_error_message} from "./ui/c_yui_main.js";
import {device_measures} from "./ui_lib_devices.js";

import {t} from "i18next";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_UI_DEVICE_SONDA";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of publishing messages"),
SDATA(data_type_t.DTP_POINTER,  "gobj_remote_yuno", 0,  null,   "Remote yuno to ask data"),
SDATA(data_type_t.DTP_STRING,   "device_type",      0,  "",     "Must be 'C_GATE_SONDA-gate_sonda', from parent"),
SDATA(data_type_t.DTP_STRING,   "device_id",        0,  "",     "Device ID, from device"),
SDATA(data_type_t.DTP_STRING,   "device_name",      0,  "",     "Device name, from device"),
SDATA(data_type_t.DTP_STRING,   "device_image",     0,  "",     "Device image, from device"),
SDATA(data_type_t.DTP_BOOLEAN,  "connected",        0,  false,  "Use to show or not the device"),
SDATA(data_type_t.DTP_JSON,     "device",           0,  null,   "Device row, from parent or remote"),
SDATA(data_type_t.DTP_JSON,     "template",         0,  null,   "Template settings, internally or remote"),
SDATA(data_type_t.DTP_JSON,     "alarms_list",      0,  null,   "List of alarms, remote"),
SDATA(data_type_t.DTP_JSON,     "alarms_desc",      0,  null,   "Description of alarms, remote"),
SDATA(data_type_t.DTP_INTEGER,  "max_chart_tracks", 0,  "20",   "Sync with max_key_instances in c_db_history.c"),
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Container element"),
SDATA(data_type_t.DTP_STRING,   "label",            0,  "monitoring_group", "Label"),
SDATA(data_type_t.DTP_STRING,   "icon",             0,  "fab fa-cloudversify", "Icon class"),
SDATA(data_type_t.DTP_JSON,     "tracks",           0,  "[]",   "List of active tracks"),
SDATA_END()
];

let PRIVATE_DATA = {
    gobj_timer: null,
    yui_plot: null,
    last_tm:    0,
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

    build_ui(gobj);

    /*
     *  Create children
     */
    priv.gobj_timer = gobj_create_pure_child(gobj_name(gobj), "C_TIMER", {}, gobj);

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
    let priv = gobj.priv;

    /*
     *  HACK add measures to template
     */
    let device = gobj_read_attr(gobj, "device");
    let template = device.template_settings;
    let device_type = gobj_read_attr(gobj, "device_type");
    set_enums(template, "$device_measures", device_measures(device_type));
    gobj_write_attr(gobj, "template", template);

    start_flow(gobj);

    gobj_start(priv.gobj_timer);
    set_timeout(priv.gobj_timer, 10*1000);
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    let priv = gobj.priv;

    clear_timeout(priv.gobj_timer);
    gobj_stop(priv.gobj_timer);
    gobj_stop_children(gobj);
    unsubscribe_all(gobj);
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
                    ], {
                        'click': function(evt) {
                            evt.stopPropagation();
                            gobj_send_event(gobj, "EV_REFRESH_REALTIME_TRACKS", {}, gobj);
                        }
                    }]
                ]],

                /*------------------------------------------*
                 *      Card content
                 *------------------------------------------*/
                ['div', {class: 'card-content p-1'}, [
                    /*---------------------------*
                     *  Top medidas
                     *---------------------------*/
                    ['div', {class: 'is-flex is-justify-content-space-between', style: 'overflow-x: auto;'}, [
                        ['div', {class: 'buttons mb-1 BUTTONS-TOP-MEDIDAS'}, []],
                        ['div', {class: 'buttons mb-1 BUTTONS-TOP-ACTIONS'}, []]
                    ]],

                    /*---------------------------*
                     *  Table and Graph
                     *---------------------------*/
                    ['div', {class: 'columns m-0 is-flex-wrap-nowrap COLUMNS-DEVICE'}, [
                        /*---------------------------*
                         *      Table
                         *---------------------------*/
                        ['div', {class: 'column is-narrow'}, [
                            ['div', {class: 'with-border', style: 'max-height: 200px; max-width: 250px; overflow:auto;'}, [
                                ['table', {class: 'table is-size-7 is-bordered', style: 'height:100%; width:100%;'}, [
                                    ['thead', {}, [
                                        ['th', {i18n: 'measure'}, 'measure'],
                                        ['th', {i18n: 'value'}, 'value'],
                                        ['th', {i18n: 'unit'}, 'unit'],
                                    ]],
                                    ['tbody', {class: 'TBODY-DEVICE-DATA'}]
                                ]]
                            ]]
                        ]],

                        ['div', {class: 'column is-narrow'}, [
                            ['div', {class: 'chart-container with-border', style: 'min-width:250px; overflow-x: auto;'}, [
                                ['div', {class: `DEVICE-GRAPH`}, []]
                            ]]
                        ]]

                    ]],

                    /*---------------------------*
                     *  Bottom toolbar
                     *---------------------------*/
                    ['div', {class: 'buttons mb-0'}, [
                        ['button', {class: 'button button-device-alarms'}, [
                            ['span', {class: 'icon'}, [
                                ['i', {class: 'fa-solid fa-triangle-exclamation'}]
                            ]],
                            ['span', {class: '', i18n: 'alarms'}, 'alarms']
                        ], {
                            'click': function(evt) {
                                evt.stopPropagation();
                                gobj_send_event(gobj, "EV_SHOW_ALARMS", {}, gobj);
                            }
                        }],
                        ['button', {class: 'button button-device-settings'}, [
                            ['span', {class: 'icon'}, [
                                ['i', {class: 'fa fa-cog'}]
                            ]],
                            ['span', {class: '', i18n: 'settings'}, 'settings']
                        ], {
                            'click': function(evt) {
                                evt.stopPropagation();
                                gobj_send_event(gobj, "EV_SHOW_SETTINGS", {}, gobj);
                            }
                        }],

                        ['div', {class: 'buttons'}, [
                            ['button', {class: 'button device-historical-measures'}, [
                                ['span', {class: 'icon'}, [
                                    ['i', {class: 'fa-solid fa-chart-line'}]
                                ]],
                                ['span', {class: '', i18n: 'historical_tracks'}, 'historical_tracks']
                            ], {
                                'click': function(evt) {
                                    evt.stopPropagation();
                                    gobj_send_event(gobj, "EV_HISTORICAL_MEASURES", {}, gobj);
                                }
                            }],
                            ['button', {class: 'button device-historical-alarms'}, [
                                ['span', {class: 'icon'}, [
                                    ['i', {class: 'fa-solid fa-chart-column'}]
                                ]],
                                ['span', {class: '', i18n: 'historical_alarms'}, 'historical_alarms']
                            ], {
                                'click': function(evt) {
                                    evt.stopPropagation();
                                    gobj_send_event(gobj, "EV_HISTORICAL_ALARMS", {}, gobj);
                                }
                            }],

                            // TODO future, a button to open a window with actions
                            // ['button', {class: 'button device-actions'}, [
                            //     ['span', {class: 'icon'}, [
                            //         ['i', {class: 'fa-solid fa-rocket-launch'}]
                            //     ]],
                            //     ['span', {class: '', i18n: 'device_actions'}, 'device_actions']
                            // ], {
                            //     'click': function(evt) {
                            //         evt.stopPropagation();
                            //         gobj_send_event(gobj, "EV_DEVICE_ACTIONS", {}, gobj);
                            //     }
                            // }]

                        ]],
                    ]]
                ]]
            ]]
        ]]
    );
    gobj_write_attr(gobj, "$container", $cell);
    refresh_language($cell, t);

    recreate_top_measures(gobj);
    recreate_top_actions(gobj);
    recreate_chart(gobj);
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
function recreate_top_measures(gobj, tracks)
{
    let size_icons= 30;
    let track = null;
    if(tracks && tracks.length) {
        track = tracks[tracks.length-1];
    }
    let device = gobj_read_attr(gobj, "device");
    let top_medidas;
    try {
        top_medidas = device.settings.top_measures;
    } catch (e) {
        top_medidas = ["temperature"];
    }

    let $container = gobj_read_attr(gobj, "$container");
    const $top_medidas = $container.querySelector('.BUTTONS-TOP-MEDIDAS');
    $top_medidas.innerHTML = '';

    for (let i = 0; i < top_medidas.length; i++) {
        let field = top_medidas[i];
        switch(field) {
            case "temperature":
                $top_medidas.appendChild(
                    createElement2(
                        ['button', {class: `button is-outlined is-link`, style: 'display: flex;'}, [
                            ['img', {
                                src: '/images/thermometer.png',
                                alt: '',
                                width: size_icons,
                                height: size_icons
                            }],
                            ['strong', {class: `TOP-MEDIDA-${field}`}, '']
                        ], {
                            click: (evt) => {
                                evt.stopPropagation();
                                gobj_send_event(gobj, "EV_SHOW_CHART", {field: field}, gobj);
                            }
                        }]
                    )
                );
                break;

            default:
                $top_medidas.appendChild(
                    createElement2(
                        ['button', {class: `button is-outlined is-link`, style: 'display: flex;'}, [
                            ['span', {class: 'has-text-success mr-1'}, field + ': '],
                            ['strong', {class: `TOP-MEDIDA-${field}`}, '']
                        ], {
                            click: (evt) => {
                                evt.stopPropagation();
                                gobj_send_event(gobj, "EV_SHOW_CHART", {field: field}, gobj);
                            }
                        }]
                    )
                );
                break;
        }
    }

    if(track) {
        for (let i = 0; i < top_medidas.length; i++) {
            let field = top_medidas[i];
            if(field in track) {
                add_top_medida(gobj, field, track[field]);
            }
        }
    }
}

/************************************************************
 *
 ************************************************************/
function recreate_top_actions(gobj)
{
    let device_type = gobj_read_attr(gobj, "device_type");

    let $container = gobj_read_attr(gobj, "$container");
    const $top_medidas = $container.querySelector('.BUTTONS-TOP-ACTIONS');
    $top_medidas.innerHTML = '';

    switch(device_type) {
        case 'C_GATE_ENCHUFE-gate_enchufe':
            $top_medidas.appendChild(
                createElement2(
                    ['div', {class: `field `, style: ''}, [
                        ['label', {class: 'switch is-rounded'}, [
                            ['input', {class: 'btn-power', type: 'checkbox', value: false}],
                            ['span', {class: 'check is-info'}],
                            ['span', {class: 'control-label'}, 'On'],
                        ]],
                    ], {
                        change: (evt) => {
                            evt.stopPropagation();
                            let value = evt.currentTarget.querySelector('input').checked;
                            gobj_send_event(gobj, "EV_SET_POWER", {set: value}, gobj);
                        }
                    }]
                )
            );
            break;
    }
}

/************************************************************
 *
 ************************************************************/
function recreate_chart(gobj)
{
    let priv = gobj.priv;

    let device = gobj_read_attr(gobj, "device");
    let top_medidas;
    try {
        top_medidas = device.settings.top_measures;
    } catch (e) {
        top_medidas = ["temperature"];
    }

    let $container = gobj_read_attr(gobj, "$container");
    const $charts = $container.querySelector('.COLUMNS-DEVICE');
    let $chart = $charts.querySelector(`.DEVICE-GRAPH`);
    if(!priv.yui_plot) {
        priv.yui_plot = gobj_create(
            "chart",
            "C_YUI_UPLOT",
            {
                $container: $chart,
                width: 450,
                height: 180,
                tm: "tm",
            },
            gobj
        );
        gobj_start(priv.yui_plot);
    } else {
        gobj_send_event(priv.yui_plot, "EV_CLEAR", {}, gobj);
    }

    for (let i = 0; i < top_medidas.length; i++) {
        let field = top_medidas[i];
        gobj_send_event(
            priv.yui_plot,
            "EV_ADD_SERIE",
            {
                id: field,
                label: field,
                width: 2,
            },
            gobj
        );
    }
}

/************************************************************
 *
 ************************************************************/
function add_top_medida(gobj, field, value)
{
    let $container = gobj_read_attr(gobj, "$container");
    const $top_medidas = $container.querySelector('.BUTTONS-TOP-MEDIDAS');
    let $measure = $top_medidas.querySelector(`.TOP-MEDIDA-${field}`);
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

        default:
            if(is_number(value)) {
                if(value % 1 !== 0) {
                    value = value.toFixed(3);
                }
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
    let window_service_name = `window-alarms-${name}`;
    let gobj_service_name = `alarms-${name}`;

    let gobj_window = gobj_find_service(window_service_name);
    if(gobj_window) {
        gobj_destroy(gobj_window);
        return;
    }

    let gobj_form = gobj_create_service(
        gobj_service_name,
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

    gobj_window = gobj_create_service(
        window_service_name,
        "C_YUI_WINDOW",
        {
            $parent: document.getElementById('top-layer'),
            subscriber: gobj_form,
            header: `<div class="has-text-centered">${t("device alarms")}</div>`,
            width: 1000,
            height: 650,
            auto_save_size_and_position: true,
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
    let name = clean_name(gobj_name(gobj));
    let window_service_name = `window-settings-${name}`;
    let gobj_service_name = `settings-${name}`;

    let gobj_window = gobj_find_service(window_service_name);
    if(gobj_window) {
        gobj_destroy(gobj_window);
        return;
    }

    let gobj_form = gobj_create_service(
        gobj_service_name,
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

    gobj_window = gobj_create_service(
        window_service_name,
        "C_YUI_WINDOW",
        {
            $parent: document.getElementById('top-layer'),
            subscriber: gobj_form,
            header: `<div class="has-text-centered">${t("device settings")}</div>`,
            width: 1050,
            height: 650,
            auto_save_size_and_position: true,
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
function show_historical_measures(gobj)
{
    let name = clean_name(gobj_name(gobj));
    let window_service_name = `window-historical-tracks-${name}`;
    let historical_service_name = `historical-tracks-${name}`;

    let gobj_window = gobj_find_service(window_service_name);
    if(gobj_window) {
        gobj_destroy(gobj_window);
        return;
    }

    let gobj_form = gobj_create_service(
        historical_service_name,
        "C_UI_HISTORICAL_CHART",
        {
            subscriber: gobj,
            gobj_remote_yuno: gobj_read_attr(gobj, "gobj_remote_yuno"),
            device_id: gobj_read_attr(gobj, "device_id"),
            device_name: gobj_read_attr(gobj, "device_name"),
            schema: device_measures(gobj_read_attr(gobj, "device_type")),
            theme: "measures",
        },
        gobj
    );

    gobj_window = gobj_create_service(
        window_service_name,
        "C_YUI_WINDOW",
        {
            $parent: document.getElementById('top-layer'),
            subscriber: gobj_form,
            header: `<div class="has-text-centered">${t("historical_tracks")} : ${gobj_read_attr(gobj, "device_name")} (${gobj_read_attr(gobj, "device_id")})</div>`,
            width: 1000,
            height: 650,
            auto_save_size_and_position: true,
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
function show_historical_alarms(gobj)
{
    let name = clean_name(gobj_name(gobj));
    let window_service_name = `window-historical-alarms-${name}`;
    let historical_service_name = `historical-alarms-${name}`;

    let gobj_window = gobj_find_service(window_service_name);
    if(gobj_window) {
        gobj_destroy(gobj_window);
        return;
    }

    let gobj_form = gobj_create_service(
        historical_service_name,
        "C_UI_HISTORICAL_CHART",
        {
            subscriber: gobj,
            gobj_remote_yuno: gobj_read_attr(gobj, "gobj_remote_yuno"),
            device_id: gobj_read_attr(gobj, "device_id"),
            device_name: gobj_read_attr(gobj, "device_name"),
            schema: gobj_read_attr(gobj, "alarms_desc"),
            theme: "alarms",
        },
        gobj
    );

    gobj_window = gobj_create_service(
        window_service_name,
        "C_YUI_WINDOW",
        {
            $parent: document.getElementById('top-layer'),
            subscriber: gobj_form,
            header: `<div class="has-text-centered">${t("historical_alarms")} : ${gobj_read_attr(gobj, "device_name")} (${gobj_read_attr(gobj, "device_id")})</div>`,
            width: 1000,
            height: 650,
            auto_save_size_and_position: true,
            body: gobj_form
        },
        gobj
    );

    // HACK change parent to destroy gobj_form when gobj_window is closed
    gobj_change_parent(gobj_form, gobj_window);

    gobj_start_tree(gobj_window);
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
 *  update with Remote response
 ************************************************************/
function process_device_settings_updated(gobj, device)
{
    let priv = gobj.priv;

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

    let tracks = gobj_read_attr(gobj, "tracks");
    recreate_top_measures(gobj, tracks);
    recreate_chart(gobj);

    let yui_plot = priv.yui_plot;
    gobj_send_event(yui_plot, "EV_LOAD_DATA", tracks, gobj);
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
    let $container = gobj_read_attr(gobj, "$container");
    let btn = $container.querySelector('.button-device-alarms');

    let active_not_validated = 0;
    let active_validated = 0;
    let not_active_not_validated = 0;

    let alarms_list = gobj_read_attr(gobj, "alarms_list");
    for(let alarm of alarms_list) {
        /*
         *  Get by preference order
         */
        if(alarm.active && !alarm.validated) {
            active_not_validated++;
        } else if(alarm.active && alarm.validated) {
            active_validated++;
        } else if(!alarm.active && !alarm.validated) {
            not_active_not_validated++;
        }
    }

    let caso = "";
    if(active_not_validated) {
        caso = "active_not_validated";
    } else if(active_validated) {
        caso = "active_validated";
    } else if(not_active_not_validated) {
        caso = "not_active_not_validated";
    }

    btn.style.color = get_alarm_color(gobj, caso);
}

/************************************************************
 *
 ************************************************************/
function process_realtime_track(gobj, track)
{
    let priv = gobj.priv;

    let $cell = gobj_read_attr(gobj, "$container");
    priv.last_tm = track.__md_tranger__.tm;

    /*-----------------------------*
     *      Card header
     *-----------------------------*/
    let device = gobj_read_attr(gobj, "device");
    let $wifi_state = $cell.querySelector(`.DEVICE-WIFI`);
    if($wifi_state) {
        // TODO a timeout loop to change the wifi state
        // WARNING code repeated
        let elapsed = Date.now()/1000 - priv.last_tm;
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
    let fields_names = device_measures(device_type, true, false);
    let fields_units = device_measures(device_type, false, true);

    /*-----------------------------*
     *      BUTTONS-TOP-MEDIDAS
     *-----------------------------*/
    let top_medidas;
    try {
        top_medidas = device.settings.top_measures;
    } catch (e) {
        top_medidas = ["temperature"];
    }

    for (let i = 0; i < top_medidas.length; i++) {
        let field = top_medidas[i];
        if(field in track) {
            add_top_medida(gobj, field, track[field]);
        }
    }

    /*-----------------------------*
     *      BUTTONS-TOP-ACTIONS
     *-----------------------------*/
    if(track.yuno === "C_GATE_ENCHUFE^gate_enchufe") {
        let power = track["power_on"];
        const $power = $cell.querySelector('.btn-power');
        $power.checked = power.toLowerCase() === 'on';
    }

    /*-----------------------------*
     *      TBODY-DEVICE-DATA
     *-----------------------------*/
    const $tbody = $cell.querySelector('.TBODY-DEVICE-DATA');
    if($tbody) {
        for(let i=0; i<fields.length; i++) {
            let field = fields[i];
            if(!(field in track)) {
                continue;
            }
            let field_name = fields_names && fields_names[i];
            let field_unit = fields_units && fields_units[i];

            let value = track[field];
            let $measure = $tbody.querySelector(`.TD-${field}`);
            if(!$measure) {
                let items = [
                    ['th', {}, `${t(field_name)}`],
                    ['td', {class: `TD-${field}`}, '']
                ];
                if(fields_units) {
                    items.push(
                        ['td', {class: ''}, `${field_unit}`]
                    );
                }
                $tbody.appendChild(
                    createElement2(
                        ['tr', {}, items]
                    )
                );
                $measure = $tbody.querySelector(`.TD-${field}`);
            }
            if($measure) {
                $measure.textContent = value;
            }
        }
    }

    gobj_publish_event(gobj, "EV_REFRESH", {});
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
    let gobj_alarms = gobj_match_children_tree(
        gobj,
        {
            "__gclass_name__": "C_UI_ALARMS",
            "__gobj_name__": `alarms-${name}`
        }
    );
    if(json_array_size(gobj_alarms)>0) {
        gobj_send_event(gobj_alarms[0], "EV_REALTIME_ALARM", track, gobj);
    }

    gobj_publish_event(gobj, "EV_REFRESH", {});
    return 0;
}

/************************************************************
 *
 ************************************************************/
function encod_coordinates(gobj, coordinates)
{
    let latitude = coordinates.latitude;
    let longitude = coordinates.longitude;
    return {
        geometry: {
            "type": "Point",
            "coordinates": [
                longitude.toPrecision(8), latitude.toPrecision(8)
            ]
        }
    };
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *  Remote response
 ************************************************************/
function ac_realtime_track(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let data = kw;

    let stack = kw.__md_iev__.ievent_gate_stack[0];
    if(stack.dst_service !== gobj_name(gobj)) {
        display_error_message(sprintf("Error, it's not my service '%s', mine is'%s'", stack.dst_service, gobj_name(gobj)));
    }

    if(is_object(data)) {
        if(kw_has_key(kw, "data")) {
            /*
             *  Comes from __first_shot__
             */
            data = data.data;
        } else {
            data = [data];
        }
    }

    let tracks = gobj_read_attr(gobj, "tracks");
    for(let i=0; i<data.length; i++) {
        let track = data[i];
        process_realtime_track(gobj, track);

        tracks.push(track);
        if(tracks.length > gobj_read_attr(gobj, "max_chart_tracks")) {
            tracks.shift();
        }
    }

    let yui_plot = priv.yui_plot;
    gobj_send_event(yui_plot, "EV_LOAD_DATA", tracks, gobj);

    return 0;
}

/************************************************************
 *  Remote response
 ************************************************************/
function ac_realtime_alarm(gobj, event, kw, src)
{
    let data = kw;
    if(is_object(data)) {
        data = [data];
    }

    for(let i=0; i<data.length; i++) {
        let alarm = data[i];
        process_realtime_alarm(gobj, alarm);
    }

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
    let data = kw;
    process_device_settings_updated(gobj, data);

    gobj_publish_event(gobj, "EV_REFRESH", {});
    return 0;
}

/************************************************************
 *  Remote response
 ************************************************************/
function ac_update_device_alarms(gobj, event, kw, src)
{
    let data = kw;

    if(is_object(data)) {
        data = [data];
    }

    for(let i=0; i<data.length; i++) {
        let alarm = data[i];
        process_realtime_alarm(gobj, alarm); // solo cambia el validated

        /*
         *  Update alarms if it's open
         */
        let name = clean_name(gobj_name(gobj));
        let gobj_alarms = gobj_match_children_tree(
            gobj,
            {
                "__gclass_name__": "C_UI_ALARMS",
                "__gobj_name__": `alarms-${name}`
            }
        );
        if(json_array_size(gobj_alarms)>0) {
            gobj_send_event(gobj_alarms[0], "EV_REALTIME_ALARM", alarm, gobj);
        }

    }
    gobj_publish_event(gobj, "EV_REFRESH", {});

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
 *  From internal click
 ************************************************************/
function ac_set_power(gobj, event, kw, src)
{
    let gobj_remote_yuno = gobj_read_pointer_attr(gobj, "gobj_remote_yuno");
    if(!gobj_remote_yuno) {
        log_error(`${gobj_short_name(gobj)}: No gobj_remote_yuno defined`);
        return;
    }

    let command = "set-power";
    let kw_set_power = {
        yuno: "gate_enchufe", // TODO gobj_read_attr(gobj, "device").yuno,
        service: "",
        device_id: gobj_read_attr(gobj, "device_id"),
        set: kw.set
    };

    kw_set_power.__md_command__ = { // Data to be returned
        device_id: gobj_read_attr(gobj, "device_id"),
        set: kw.set
    };

    let ret = gobj_command(
        gobj_remote_yuno,
        command,
        kw_set_power,
        gobj
    );
    if(ret) {
        log_error(ret);
    }

    if(ret) {
        log_error(ret);
    }

    return 0;
}

/************************************************************
 *  Remote response
 ************************************************************/
function ac_mt_command_answer(gobj, event, kw, src)
{
    let webix_msg = kw;

    let result;
    let comment;
    let schema;
    let data;

    try {
        result = webix_msg.result;
        comment = webix_msg.comment;
        schema = webix_msg.schema;  // What about controlling if version has changed?
        data = webix_msg.data;
    } catch (e) {
        log_error(e);
        return;
    }
    if(result < 0) {
        display_error_message(
            "Error",
            t(comment)
        );
        // HACK don't return, pass errors when need it.
    }

    let command_stack  = msg_iev_get_stack(gobj, kw, "command_stack", true);
    let command = kw_get_str(gobj, command_stack, "command", "", kw_flag_t.KW_REQUIRED);
    let kw_command = kw_get_dict(gobj, command_stack, "kw", {}, kw_flag_t.KW_REQUIRED);

    switch(command) {
        case "set-power":
            if(result >= 0) {
                /*
                 *  Here it could update cols of `descs`
                 *  seeing if they have changed in schema argument (controlling the version?)
                 *  Now the schema pass to creation of nodes is get from `descs`.
                 */
                // process_command(gobj, __md_command__, data);
            }
            break;

        default:
            log_error(`${gobj_short_name(gobj)} Command unknown: ${command}`);
    }

    return 0;
}

/************************************************************
 *  From some internal map,
 *  The coordinates of the device have changed, update
 ************************************************************/
function ac_set_coordinates(gobj, event, kw, src)
{
    let coordinates = encod_coordinates(gobj, kw);
    let device = gobj_read_attr(gobj, "device");
    let record = device.settings;
    record.coordinates = coordinates;
    let kw_set_device_settings = {
        device_id: gobj_read_attr(gobj, "device_id"),
        record: record
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
 *  From internal click
 ************************************************************/
function ac_show_chart(gobj, event, kw, src)
{
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
function ac_refresh_realtime_tracks(gobj, event, kw, src)
{
    let id = gobj_read_attr(gobj, "device_id");
    gobj_write_attr(gobj, "tracks", []);

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
    return 0;
}

/************************************************************
 *  From internal click
 ************************************************************/
function ac_historical_measures(gobj, event, kw, src)
{
    show_historical_measures(gobj);
    return 0;
}

/************************************************************
 *  From internal click
 ************************************************************/
function ac_historical_alarms(gobj, event, kw, src)
{
    show_historical_alarms(gobj);
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
    let priv = gobj.priv;

    let broadcast = false;
    let device = gobj_read_attr(gobj, "device");

    let $cell = gobj_read_attr(gobj, "$container");
    let $wifi_state = $cell.querySelector(`.DEVICE-WIFI`);
    if($wifi_state) {
        // TODO a timeout loop to change the wifi state
        // WARNING code repeated
        let elapsed = Date.now()/1000 - priv.last_tm;
        if(elapsed < 60*10) {
            $wifi_state.src = '/images/wifi-yes.png';
            if(!device.connected) {
                broadcast = true;
            }
            device.connected = true;
            gobj_write_attr(gobj, "connected", true);
        } else {
            $wifi_state.src = '/images/wifi-no.png';
            if(device.connected) {
                broadcast = true;
            }
            device.connected = false;
            gobj_write_attr(gobj, "connected", false);
        }
    }

    if(broadcast) {
        gobj_publish_event(gobj, "EV_REFRESH", {});
    }
    set_timeout(priv.gobj_timer, 10*1000);

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
            ["EV_REFRESH_REALTIME_TRACKS",  ac_refresh_realtime_tracks, null],
            ["EV_LIST_ALARMS",              ac_list_alarms,             null],
            ["EV_UPDATE_DEVICE_SETTINGS",   ac_update_device_settings,  null],
            ["EV_UPDATE_DEVICE_ALARMS",     ac_update_device_alarms,    null],
            ["EV_SET_COORDINATES",          ac_set_coordinates,         null],
            ["EV_SHOW_CHART",               ac_show_chart,              null],
            ["EV_SHOW_ALARMS",              ac_show_alarms,             null],
            ["EV_SHOW_SETTINGS",            ac_show_settings,           null],
            ["EV_HISTORICAL_MEASURES",      ac_historical_measures,     null],
            ["EV_HISTORICAL_ALARMS",        ac_historical_alarms,       null],
            ["EV_SAVE_RECORD",              ac_save_record,             null],
            ["EV_SET_POWER",                ac_set_power,               null],
            ["EV_MT_COMMAND_ANSWER",        ac_mt_command_answer,       null],
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
        ["EV_REALTIME_TRACK",           event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_REALTIME_ALARM",           event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_LIST_ALARMS",              event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_REFRESH_REALTIME_TRACKS",  0],
        ["EV_UPDATE_DEVICE_SETTINGS",   event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_UPDATE_DEVICE_ALARMS",     event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_SET_COORDINATES",          event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_MT_COMMAND_ANSWER",        event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_SHOW_CHART",               0],
        ["EV_SHOW_ALARMS",              0],
        ["EV_SHOW_SETTINGS",            0],
        ["EV_HISTORICAL_MEASURES",      0],
        ["EV_HISTORICAL_ALARMS",        0],
        ["EV_SAVE_RECORD",              0],
        ["EV_SET_POWER",                0],
        ["EV_WINDOW_MOVED",             0],
        ["EV_WINDOW_RESIZED",           0],
        ["EV_WINDOW_TO_CLOSE",          0],
        ["EV_REFRESH",                  event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
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
function register_c_ui_device_sonda()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_ui_device_sonda };
