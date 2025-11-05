/***********************************************************************
 *          c_ui_monitoring_group.js
 *
 *          Monitorización de los devices del grupo
 *
 *          Crea un nodo (C_UI_DEVICE_SONDA,...) por device
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA,
    SDATA_END,
    data_type_t,
    sdata_flag_t,
    gclass_create,
    log_error,
    gobj_subscribe_event,
    gobj_read_pointer_attr,
    gobj_parent,
    createElement2,
    clean_name,
    gclass_find_by_name,
    empty_string,
    gobj_write_attr,
    gobj_read_attr,
    gobj_send_event,
    gobj_read_str_attr,
    gobj_save_persistent_attrs,
    gobj_post_event,
    gobj_create_service,
    gobj_start,
    gobj_find_service,
    gobj_name,
    gobj_stop_children,
    refresh_language,
} from "yunetas";

import {t} from "i18next";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_UI_MONITORING_GROUP";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_POINTER,  "gobj_remote_yuno", 0,  null,   "Remote yuno to ask data"),
SDATA(data_type_t.DTP_STRING,   "group_id",         0,  "",     "Group ID set by parent"),
SDATA(data_type_t.DTP_JSON,     "devices",          0,  "[]",   "List of devices set by parent"),
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Container element"),
SDATA(data_type_t.DTP_STRING,   "label",            0,  "monitoring_group", "Label"),
SDATA(data_type_t.DTP_STRING,   "icon",             0,  "fab fa-cloudversify", "Icon class"),
SDATA(data_type_t.DTP_STRING,   "select_by_state",  0,  "",     "State filter"),
SDATA(data_type_t.DTP_STRING,   "select_by_name",   0,  "",     "Name filter"),
SDATA(data_type_t.DTP_STRING,   "select_view",      sdata_flag_t.SDF_PERSIST, "show-grid", "View type: grid or map"),
SDATA(data_type_t.DTP_POINTER,  "gobj_map",         0,  null,   "Map object"),
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
    crea_devices_grid_and_map(gobj);
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    gobj_stop_children(gobj);
    destroy_ui(gobj);
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
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
        let $container = createElement2(
            // Don't use is-flex, don't work well with is-hidden
            ['div', {class: `monitoring-group is-hidden ${clean_name(gobj_name(gobj))}`, style: 'height:100%; display:flex; flex-direction:column;'}, [
                /*----------------------------------------*
                 *          TOOLBAR
                 *----------------------------------------*/
                ['div', {class: 'is-flex-grow-0 mb-2'}, [
                    ['div', {class: 'is-flex is-justify-content-space-between', style: 'overflow-x: auto;'}, [
                        /*----------------------------------------*
                         *  Ver: todos, connected, disconnected
                         *----------------------------------------*/
                        ['div', {class: 'select'}, [
                            ['select', {class: ''}, [
                                ['option', {value: 'see-all', i18n: 'see all'}, 'see all'],
                                ['option', {value: 'connected', i18n: 'connected'}, 'connected'],
                                ['option', {value: 'disconnected', i18n: 'disconnected'}, 'disconnected'],
                            ], {
                                'change': function (evt) {
                                    gobj_write_attr(gobj, "select_by_state", evt.target.value);
                                    gobj_send_event(gobj, "EV_SELECT", {}, gobj);
                                }
                            }]
                        ]],

                        /*----------------------------------------*
                         *  Ver: map, boxes
                         *----------------------------------------*/
                        ['div', {class: 'select'}, [
                            ['select', {class: 'select-view'}, [
                                ['option', {value: 'show-grid', i18n: 'show-grid'}, 'show-grid'],
                                ['option', {value: 'show-map', i18n: 'show-map'}, 'show-map'],
                            ], {
                                'change': function (evt) {
                                    gobj_write_attr(gobj, "select_view", evt.target.value);
                                    gobj_send_event(gobj, "EV_SELECT_VIEW", {}, gobj);
                                }
                            }]
                        ]],

                        /*----------------------------------------*
                         *      Buscar device
                         *----------------------------------------*/
                        ['div', {class: 'field is-flex is-align-items-center'}, [
                            ['label', {class: 'is-hidden-mobile pr-2', i18n:'search device'}, 'search device'],
                            ['div', {class: 'control has-icons-left has-icons-right'}, [
                                [
                                    'input',
                                    {class: 'input', type: 'text', placeholder: t('device') + '...', style: 'min-width: 300px;'},
                                    '',
                                    {
                                        'input': function(evt) {
                                            let $btn = this.closest('.control').querySelector('.clear-button');
                                            if (this.value) {
                                                $btn.style.display = 'inline-flex';
                                                gobj_write_attr(gobj, "select_by_name", this.value);
                                                gobj_send_event(gobj, "EV_SELECT", {}, gobj);
                                            } else {
                                                $btn.style.display = 'none';
                                                gobj_write_attr(gobj, "select_by_name", this.value);
                                                gobj_send_event(gobj, "EV_SELECT", {}, gobj);
                                            }
                                        }
                                    }
                                ],
                                [
                                    'span',
                                    {
                                        class: 'icon is-left is-clickable'
                                    },
                                    '<i class="fas fa-search"></i>',
                                    {
                                        'click': function(evt) {
                                            evt.stopPropagation();
                                            let $input = this.closest('.control').querySelector('input');
                                            gobj_write_attr(gobj, "select_by_name", $input.value);
                                            gobj_send_event(gobj, "EV_SELECT", {}, gobj);
                                        }
                                    }
                                ],
                                [
                                    'span',
                                    {
                                        class: 'icon clear-button is-right is-clickable',
                                        style: 'display:none;'
                                    },
                                    '<i class="fas fa-times"></i>',
                                    {
                                        'click': function(evt) {
                                            evt.stopPropagation();
                                            let $input = this.closest('.control').querySelector('input');
                                            $input.value = '';
                                            this.style.display = 'none';
                                            gobj_write_attr(gobj, "select_by_name", "");
                                            gobj_send_event(gobj, "EV_SELECT", {}, gobj);
                                        }
                                    }
                                ]
                            ]],

                            /*----------------------------*
                             *      Refresh
                             *----------------------------*/
                            ['button', {class: 'button ml-2'}, [
                                ['span', {class: 'icon'}, '<i class="fa fa-sync"></i>'],
                                ['span', {class: 'is-hidden-mobile', i18n: 'refresh'}, 'refresh']
                            ], {
                                click: function(evt) {
                                    evt.stopPropagation();
                                    gobj_send_event(gobj, "EV_REFRESH", {}, gobj);
                                }
                            }],

                        ]]
                    ]],
                ]],

                /*----------------------------------------*
                 *          WORK AREA
                 *----------------------------------------*/
                ['div', {class: 'is-flex-grow-1', style: 'height: 100%;'}, [
                    ['div',
                        {
                            class: 'xgrid columns m-0 is-flex-wrap-wrap', // m-0 avoid little h-scrollbar
                        },
                    ],
                    ['div',
                        {
                            class: 'xmap is-hidden',
                        }
                    ]
                ]]
            ]]
        );
        gobj_write_attr(gobj, "$container", $container);
        refresh_language($container, t);
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
    function get_group_map_name(gobj)
    {
        return `group_map_${clean_name(gobj_name(gobj))}`;
    }

    /************************************************************
     *  gobj name of a device id
     ************************************************************/
    function get_device_cell_name(gobj, device_id)
    {
        return `device_cell_${clean_name(gobj_name(gobj))}_${clean_name(device_id)}`;
    }

    /************************************************************
     *
     ************************************************************/
    function get_device_type(gobj, device) // WARNING repeated
    {
        return clean_name(device.device_type[0]);
    }

    /************************************************************
     *
     ************************************************************/
    function crea_devices_grid_and_map(gobj)
    {
        let $container = gobj_read_attr(gobj, "$container");
        let $grid = $container.querySelector('.xgrid');
        let $map = $container.querySelector('.xmap');

        const dimensions = $map.parentNode.getBoundingClientRect($map);
        $map.style.width = dimensions.width + 'px';
        $map.style.height = dimensions.height + 'px';

        let gobj_map = gobj_create_service(
            get_group_map_name(gobj),
            "C_YUI_MAP",
            {
                $map: $map,
                dimensions_with_parent_observer: true,
                devices: gobj_read_attr(gobj, "devices"),
            },
            gobj
        );
        gobj_write_attr(gobj, "gobj_map", gobj_map);
        gobj_start(gobj_map);

        let devices = gobj_read_attr(gobj, "devices");
        for(let device of devices) {
            let gclass_name = null;
            let device_cell_name = get_device_cell_name(gobj, device.id);
            let first_device_type = get_device_type(gobj, device); // get the first type
            switch(first_device_type) {
                case "C_GATE_SONDA-gate_sonda":
                    gclass_name = "C_UI_DEVICE_SONDA";
                    break;
                case "C_GATE_ESTERILIZ-gate_esteriliz":
                    gclass_name = "C_UI_DEVICE_TERMOD";
                    break;
                case "C_GATE_AURAAIR-gate_auraair":
                case "C_GATE_MQTT-gate_mqtt":
                case "C_GATE_MQTT-FS00802_WIFI":
                case "C_GATE_ENCHUFE-gate_enchufe":
                case "C_GATE_FRIGO-gate_frigo":
                default:
                    gclass_name = "C_UI_DEVICE_SONDA"; // TODO implement others device types
                    break;
            }

            /*
             *  Create the box for grid
             */
            let gobj_cell = gobj_create_service(
                device_cell_name,
                gclass_name,
                {
                    gobj_remote_yuno: gobj_read_str_attr(gobj, "gobj_remote_yuno"),
                    device_type: first_device_type,
                    device: device,
                    device_id: device.id,
                    device_name: device.name,
                    device_image: device.image,
                    subscriber: gobj_map // Subscribe map to EV_REALTIME_TRACK/EV_REALTIME_ALARM
                },
                gobj
            );
            if(gobj_cell) {
                let $cell = gobj_read_attr(gobj_cell, "$container");
                $grid.appendChild($cell);
                gobj_start(gobj_cell);
            }

            /*
             *  info to map
             */
            device.gobj_service_name = device_cell_name; // info for map
            device.first_device_type = first_device_type;
        }
    }

    /************************************************************
     *
     ************************************************************/
    function show_map(gobj)
    {
        let $container = gobj_read_attr(gobj, "$container");
        let $grid = $container.querySelector('.xgrid');
        let $map = $container.querySelector('.xmap');

        $grid.classList.add("is-hidden");
        $map.classList.remove("is-hidden");

        let gobj_map = gobj_read_attr(gobj, "gobj_map");
        gobj_send_event(gobj_map, "EV_SHOW", {}, gobj);
        return 0;
    }

    /************************************************************
     *
     ************************************************************/
    function show_grid(gobj)
    {
        let gobj_map = gobj_read_attr(gobj, "gobj_map");
        gobj_send_event(gobj_map, "EV_HIDE", {}, gobj);

        let $container = gobj_read_attr(gobj, "$container");
        let $grid = $container.querySelector('.xgrid');
        let $map = $container.querySelector('.xmap');

        $grid.classList.remove("is-hidden");
        $map.classList.add("is-hidden");

        return 0;
    }




                    /***************************
                     *      Actions
                     ***************************/




    /************************************************************
     *
     ************************************************************/
    function ac_select(gobj, event, kw, src)
    {
        let select_by_state = gobj_read_attr(gobj, "select_by_state");
        let select_by_name = gobj_read_attr(gobj, "select_by_name");

        let devices = gobj_read_attr(gobj, "devices");
        for(let device of devices) {
            let device_cell_name = get_device_cell_name(gobj, device.id);
            let gobj_cell = gobj_find_service(device_cell_name);
            if(gobj_cell) {
                let show_by_state = false;
                switch(select_by_state) {
                    case 'connected':
                        if(gobj_read_attr(gobj_cell, "connected")) {
                            show_by_state = true;
                        }
                        break;
                    case 'disconnected':
                        if(!gobj_read_attr(gobj_cell, "connected")) {
                            show_by_state = true;
                        }
                        break;
                    case 'see-all':
                    default:
                        show_by_state = true;
                        break;
                }

                let show_by_name = false;
                if(empty_string(select_by_name)) {
                    show_by_name = true;
                } else {
                    let device_name = gobj_read_attr(gobj_cell, "device_name");
                    let device_id = gobj_read_attr(gobj_cell, "device_id");
                    if(device_name.toLowerCase().includes(select_by_name.toLowerCase()) ||
                            device_id.toLowerCase().includes(select_by_name.toLowerCase())) {
                        show_by_name = true;
                    } else {
                        show_by_name = false;
                    }
                }

                if(show_by_name && show_by_state) {
                    device.hide = false;
                    gobj_send_event(gobj_cell, "EV_SHOW", {}, gobj);
                } else {
                    device.hide = true;
                    gobj_send_event(gobj_cell, "EV_HIDE", {}, gobj);
                }
            }
        }

        let gobj_map = gobj_read_attr(gobj, "gobj_map");
        gobj_send_event(gobj_map, "EV_REFRESH", {}, gobj);

        return 0;
    }

    /************************************************************
     *
     ************************************************************/
    function ac_select_view(gobj, event, kw, src)
    {
        if(gobj_read_str_attr(gobj, "select_view") === 'show-map') {
            show_map(gobj);
        } else {
            show_grid(gobj);
        }

        gobj_save_persistent_attrs(gobj, "select_view");

        return 0;
    }

    /************************************************************
     *
     ************************************************************/
    function ac_refresh(gobj, event, kw, src)
    {
        // TODO TEST remove next line or comment
        // gobj_write_attr(
        //     gobj_parent(gobj),
        //     "autorefresh",
        //     !gobj_read_bool_attr(gobj_parent(gobj), "autorefresh")
        // );
        gobj_post_event(gobj_parent(gobj), "EV_REFRESH", {}, gobj);
        return 0;
    }

    /************************************************************
     *  {
     *      href: href
     *  }
     ************************************************************/
    function ac_show(gobj, event, kw, src)
    {
        let href = kw.href;

        let $container = gobj_read_attr(gobj, "$container");
        let $select = $container.querySelector('.select-view');
        let select_view = gobj_read_str_attr(gobj, "select_view");
        $select.value = select_view?select_view:'show-grid';

        if(select_view === 'show-map') {
            show_map(gobj);
        } else {
            show_grid(gobj);
        }

        return 0;
    }

    /************************************************************
     *
     ************************************************************/
    function ac_hide(gobj, event, kw, src)
    {
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
            ["EV_SELECT",               ac_select,              null],
            ["EV_SELECT_VIEW",          ac_select_view,         null],
            ["EV_REFRESH",              ac_refresh,             null],
            ["EV_SHOW",                 ac_show,                null],
            ["EV_HIDE",                 ac_hide,                null],
            ["EV_TIMEOUT",              ac_timeout,             null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_SELECT",               0],
        ["EV_SELECT_VIEW",          0],
        ["EV_REFRESH",              0],
        ["EV_SHOW",                 0],
        ["EV_HIDE",                 0],
        ["EV_TIMEOUT",              0]
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
function register_c_ui_monitoring_group()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_ui_monitoring_group };
