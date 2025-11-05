/***********************************************************************
 *          c_ui_alarms.js
 *
 *          Alarms
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    __yuno__,
    SDATA,
    SDATA_END,
    event_flag_t,
    data_type_t,
    gclass_create,
    log_error,
    gobj_subscribe_event,
    gobj_read_pointer_attr,
    gobj_parent,
    createElement2,
    sprintf,
    kw_get_str,
    template_get_field_desc,
    is_array,
    is_object,
    clean_name,
    str_in_list,
    kw_has_key,
    gobj_read_attr,
    gobj_write_attr,
    gobj_write_bool_attr,
    gobj_read_bool_attr,
    gobj_short_name,
    gobj_send_event,
    gobj_unsubscribe_event,
    gobj_find_service,
    json_deep_copy,
    refresh_language,
} from "yunetas";

import {find_device_cell} from "./c_yuneta_gui.js";
import {display_error_message} from "./ui/c_yui_main.js";

import "tabulator-tables/dist/css/tabulator.min.css"; // Import Tabulator CSS
import "tabulator-tables/dist/css/tabulator_bulma.css";
import { TabulatorFull as Tabulator } from "tabulator-tables"; // Import Full Tabulator JS
// import { Tabulator } from "tabulator-tables";  // Import light Tabulator JS

import {t} from "i18next";
import { DateTime } from "luxon";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_UI_ALARMS";

/***************************************************************
 *              Data
 ***************************************************************/
/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of publishing messages"),
SDATA(data_type_t.DTP_POINTER,  "gobj_remote_yuno", 0,  null,   "Remote yuno to ask data"),
SDATA(data_type_t.DTP_STRING,   "device_id",        0,  "",     "If set, then get only this device ID"),
SDATA(data_type_t.DTP_JSON,     "settings",         0,  null,   "Device settings, local or remote, used for colors"),
SDATA(data_type_t.DTP_JSON,     "alarms_list",      0,  null,   "List of alarms, local or remote"),
SDATA(data_type_t.DTP_JSON,     "alarms_desc",      0,  null,   "Description of alarms, local or remote"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_delete_row_btn",0,false,  "Enable delete row button"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_rownum",      0,  false,  "Enable rownum"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_checkbox",    0,  false,  "Enable checkbox"),
SDATA(data_type_t.DTP_JSON,     "tabulator_settings", 0,  {
    layout: "fitDataFill",
    validationMode: "highlight",
    columnDefaults: {
        resizable: true
    },
    maxHeight: "100%"  // very good hack
}, "Default settings for Tabulator"),
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Container element"),
SDATA(data_type_t.DTP_POINTER,  "tabulator",        0,  null,   "Tabulator instance"),
SDATA(data_type_t.DTP_STRING,   "label",            0,  "alarms", "Label"),
SDATA(data_type_t.DTP_STRING,   "icon",             0,  "fa-solid fa-bell", "Icon class"),
SDATA(data_type_t.DTP_BOOLEAN,  "initialized",      0,  false,  "Indicates if it is initialized"),
SDATA(data_type_t.DTP_BOOLEAN,  "changes",          0,  false,  "Indicates if changes have been made"),
SDATA(data_type_t.DTP_STRING,   "select",           0,  "no-validated", "First or current filter of alarms"),
SDATA_END()
];

let PRIVATE_DATA = {
    subscribed_devices: []
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
    let alarms_list = gobj_read_attr(gobj, "alarms_list");
    if(alarms_list) {
        procesa_list_alarms(gobj, alarms_list);
    } else {
        get_list_alarms(gobj);
    }
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
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
 *
 ************************************************************/
function isEventSet(table, event)
{
    // Access the event listeners directly from the Tabulator instance
    let eventListeners = table.externalEvents.events;
    return eventListeners && eventListeners[event] && eventListeners[event].length > 0;
}

/************************************************************
 *
 ************************************************************/
function set_changed_stated(gobj, changed)
{
    let $container = gobj_read_attr(gobj, "$container");
    let $button_save = $container.querySelector('.button-save');
    if(!$button_save) {
        return;
    }

    if(changed) {
        /*
         *  Data changed, enable buttons to save/undo
         */
        gobj_write_bool_attr(gobj, "changes", true);

        $button_save.removeAttribute("disabled");

        $button_save.querySelector('span i').style.color = "MediumSeaGreen";

    } else {
        /*
         *  Data not changed, disable buttons to save/undo
         */
        gobj_write_bool_attr(gobj, "changes", false);

        $button_save.setAttribute("disabled", true);

        $button_save.querySelector('span i').style.color = "hsl(var(--bulma-button-h), var(--bulma-button-s), var(--bulma-button-color-l))";

        // setTimeout(function() {
        //     $container.querySelectorAll('.yui-tabulator-table').forEach($table => {
        //         if(isEventSet($table.tabulator, "dataChanged")) {
        //             $table.tabulator.off("dataChanged");
        //         }
        //         $table.tabulator.on("dataChanged", function(data) {
        //             //data - the updated table data
        //             gobj_send_event(gobj, "EV_RECORD_CHANGED", {}, gobj);
        //         });
        //     });
        // }, 200);
    }
}

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
        ['div', {class: 'alarms', style: 'height:100%; display:flex; flex-direction:column;'}, [
            ['div', {class: 'is-flex-grow-0 mt-4 mb-4'}, [
                ['div', {class: 'buttons'}, [
                    /*----------------------------*
                     *      Refresh
                     *----------------------------*/
                    ['button', {class: 'button'}, [
                        ['span', {class: 'icon'}, '<i class="fa fa-sync"></i>'],
                        ['span', {class: 'is-hidden-mobile', i18n: 'refresh'}, 'refresh']
                    ], {
                        click: function(evt) {
                            evt.stopPropagation();
                            gobj_send_event(gobj, "EV_REFRESH", {}, gobj);
                        }
                    }],

                    /*----------------------------*
                     *      Select
                     *----------------------------*/
                    ['div', {class: 'buttons has-addons buttons-select-alarms'}, [
                        ['button', {class: 'button no-validated', i18n: 'no_validated'}, 'no_validated', {
                            click: function(evt) {
                                evt.stopPropagation();
                                gobj_send_event(gobj, "EV_SELECT_ALARMS", {select: "no-validated"}, gobj);
                            }
                        }],
                        ['button', {class: 'button validated', i18n: 'active-validated'}, 'active-validated', {
                            click: function(evt) {
                                evt.stopPropagation();
                                gobj_send_event(gobj, "EV_SELECT_ALARMS", {select: "validated"}, gobj);
                            }
                        }],
                        ['button', {class: 'button all', i18n: 'all'}, 'all', {
                            click: function(evt) {
                                evt.stopPropagation();
                                gobj_send_event(gobj, "EV_SELECT_ALARMS", {select: "all"}, gobj);
                            }
                        }]
                    ]]
                ]]
            ]],
            ['div', {class: 'is-flex-grow-1', style: 'overflow: auto;'}, [
                ['div',
                    {
                        class: 'yui-tabulator-table is-bordered is-striped',
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

/******************************************************************
 *  Create tabulator
 ******************************************************************/
function create_tabulator(gobj, $extend, name, template)
{
    function get_selectable_state(row) {
        return false; // TODO review gobj_read_bool_attr(gobj, "selectable");
    }

    $extend.template = template;

    let tabulator_settings = json_deep_copy(gobj_read_attr(gobj, "tabulator_settings"));

    Object.assign(tabulator_settings, {
        /*
         *  HACK initially must be true, else doesn't work.
         *  Later could be set to false
         */
        // selectableRows: true, // Rows selected with click, if selectableRowsCheck() return true
        //selectableRowsCheck: get_selectable_state,
    });

    let columns = [];
    if(gobj_read_bool_attr(gobj, "with_rownum")) {
        columns.push(
            {
                formatter: "rownum",
                hozAlign: "center",
                headerSort: false,
                resizable: false,
                width: 40
            }
        );
    }

    tabulator_settings.columns = template2columns(gobj, columns, template);
    if(gobj_read_bool_attr(gobj, "with_delete_row_btn")) {
        columns.push(
            {
                formatter: function(cell, formatterParams) {
                    return '<i style="" class="fa fa-trash has-text-danger"></i>';
                },
                width:40,
                hozAlign:"center",
                frozen: true,
                headerSort: false,
                resizable: false,
                cellClick: function(evt, cell) {
                    evt.stopPropagation();
                    evt.preventDefault();
                    let row = cell.getRow();
                    row.delete();
                }
            }
        );
    }

    if(gobj_read_bool_attr(gobj, "with_checkbox")) {
        tabulator_settings.rowHeader = {
            formatter: "rowSelection",
            titleFormatter: "rowSelection",
            headerSort: false,
            resizable: false,
            frozen: true,
            headerHozAlign: "center",
            hozAlign: "center"
        };
    }

    let tabulator = new Tabulator(
        $extend,
        tabulator_settings
    );

    // tabulator.on("rowClick", function(e, row) {
    //     //e - the click event object
    //     //row - row component
    //     e.preventDefault();
    //     if(get_selectable_state(row)) {
    //         row.toggleSelect();
    //     }
    // });

    // tabulator.on("cellClick", function(e, cell) {
    //     //e - the click event object
    //     //cell - cell component
    //     e.preventDefault();
    //     let row = cell.getRow();
    //     // if(get_selectable_state(row)) {
    //     //     // cannot set at same time with rowClick, one select and other de-select
    //     //     row.toggleSelect();
    //     // }
    // });

    tabulator.on("cellEdited", function(cell) {
        //cell - cell component
        gobj_send_event(gobj, "EV_RECORD_CHANGED", {cell: cell}, gobj);
    });


    return tabulator;
}

/************************************************************
 *  WARNING code duplicated
 ************************************************************/
function get_alarm_color(gobj, device_id, active, no_validated)
{
    let caso = "";
    if (active && no_validated) {
        caso = "active_not_validated";
    } else if (active && !no_validated) {
        caso = "active_validated";
    } else if (!active && no_validated) {
        caso = "not_active_not_validated";
    }

    let color; // WARNING code duplicated
    switch (caso) {
        case "active_not_validated":
        case "active_validated":
        case "not_active_not_validated":
            let settings = gobj_read_attr(gobj, "settings");
            if(!settings) {
                let device_cell = find_device_cell(device_id);
                if(device_cell) {
                    let device = gobj_read_attr(device_cell, "device");
                    settings = device.settings;
                }
            }
            color = kw_get_str(gobj, settings, "alarm_colors`" + caso);
            break;
        default:
            color = "hsl(var(--bulma-button-h), var(--bulma-button-s), var(--bulma-button-color-l))";
            break;
    }
    return color;
}

/******************************************************************
 *  Convert a template's items in tabulator columns
 ******************************************************************/
function template2columns(gobj, columns, template)
{
    if(is_array(template)) {
        for(let i=0; i<template.length; i++) {
            let desc = template[i];
            let key = desc.id;
            if(key.charAt(0) === '_') {
                continue;
            }
            desc2col(gobj, columns, key, desc);
        }

    } else if(is_object(template)) {
        for (const key in template) {
            if (Object.prototype.hasOwnProperty.call(template, key)) {
                const desc = template[key];
                if(key.charAt(0) === '_') {
                    continue;
                }
                desc2col(gobj, columns, key, desc);
            }
        }
    }
    return columns;
}

/******************************************************************
 *  Convert a template's items in tabulator columns
 ******************************************************************/
function desc2col(gobj, columns, key, desc)
{
    function get_editable_state(cell) {
        // TODO habilita editable validate field
        if(cell.getField()==="validated") {
            return true;
        } else {
            return false;
        }
    }

    // Skip this entry if some condition is met
    // if (key === '_yuno' || key === '_device_type') {
    //     continue;
    // }
    const field_desc = template_get_field_desc(key, desc);

    let readonly = !field_desc.is_writable;
    let validator = [];
    let column = {
        field: field_desc.name,
        title: t(field_desc.header || field_desc.name),
        editable: get_editable_state,
        editor: false,
        headerWordWrap: true,
        validator: validator,
    };

    if(field_desc.default_value !== undefined) {
        column.mutator = function (value) {
            return value || field_desc.default_value;
        };
    }

    if(field_desc.name === "alarm") {
        column.formatter = function(cell, formatterParams) {
            let value = cell.getValue();
            let record = cell.getData();
            let color = get_alarm_color(gobj, clean_name(record.id), record.active, !record.validated);
            return `<span style="color:${color}; font-weight:bold;">` + value + "</span>";
        };
    }

    if(field_desc.name === "triggers" || field_desc.name === "_old_triggers") {
        column.formatter = function (cell, formatterParams) {
            let triggers = cell.getValue();
            let r = '';
            if(triggers) {
                for (let trigger of triggers) {
                    r += `<p>`;
                    r += `<strong>${t(trigger.id)}</strong>`;
                    r += `<span class="ml-2 mr-3">${trigger.operation}</span>`;
                    r += `<span>${trigger.value}</span>`;
                    r += `</p>`;
                    r += `<p>`;
                    r += `<span>(${t("current value")}&nbsp;${trigger._current_value})</span>`;
                    r += `</p>`;
                }
            }
            return r;
        };
    }

    if(field_desc.is_required) {
        validator.push("required");
    }

    // TODO añade los validator necesarios, como validator.push("integer");
    // TODO los editores, esto debería estar en una función
    switch(field_desc.type) {
        case "object":
        case "dict":
        case "table":
        case "array":
        case "list":
        case "blob":
            // field_conf.tag = 'jsoneditor';
            break;

        case "enum":
            column.editor = "list";
            column.editorParams = {
            };
            switch(field_desc.real_type) {
                case "string": // TODO review types
                case "object":
                case "dict":
                case "array":
                case "list":
                    column.editorParams.values = field_desc.enum_list;
                    break;
            }
            break;

        case "template":
            // field_conf.tag = 'jsoneditor';
            break;

        case "hook":
            // hooks don't appear in form
            break;

        case "fkey":
            // build later
            break;

        case "image":
        case "string":
            // field_conf.tag = 'input';
            // field_conf.inputType = 'text';
            column.editor = "input";
            column.editorParams = {
                search: true,
                selectContents: true,
            };
            break;
        case "integer":
            // field_conf.tag = 'input';
            // field_conf.inputType = 'text';
            // field_conf.extras.inputmode = "decimal";
            // field_conf.extras.pattern = "^-?\\d+$";
            column.editor = "input";
            column.editorParams = {
                search: true,
                selectContents: true,
            };
            // TODO validator.push("integer");
            break;
        case "real":
        case "number":
            // field_conf.tag = 'input';
            // field_conf.inputType = 'text';
            // field_conf.extras.inputmode = "decimal";
            // field_conf.extras.pattern = "^-?\\d+(\\.\\d+)?$";
            column.editor = "input";
            column.editorParams = {
                search: true,
                selectContents: true,
            };
            break;

        case "rowid":
            // field_conf.tag = 'input';
            // field_conf.inputType = 'text';
            column = null;
            break;

        case "boolean":
            // field_conf.tag = 'checkbox';
            column.editor = "tickCross";
            column.formatter = "tickCross";
            column.editorParams = {
            };
            column.hozAlign = "center";
            if(field_desc.name === "active" || field_desc.name === "validated") {
                column.bottomCalc = "count";
            }
            break;

        case "now":
        case "time":
            // TODO con este mutator al backend llega la fecha en string, falta conversor
            // column.editor = "datetime";
            // column.formatter = "datetime";
            column.formatter = function (cell, formatterParams) {
                let value = cell.getValue();
                if(value !== undefined) {
                    value = DateTime.fromSeconds(value);
                }
                return `<span>${value}</span>`;
            };
            //
            // column.editorParams = {
            //     format: "dd/MM/yyyy hh:mm:ss", // the format of the date value stored in the cell
            //     verticalNavigation: "table", //navigate cursor around table without changing the value
            //     elementAttributes: {
            //         title:"slide bar to choose option" // custom tooltip
            //     }
            // };
            break;
        case "color":
            // field_conf.tag = 'input';
            // field_conf.inputType = 'color';
            column.editor = "input";
            column.editorParams = {
            };
            switch(field_desc.real_type) {
                case "string":
                    break;
                case "integer":
                    break;
            }
            break;
        case "email":
            // field_conf.tag = 'input';
            // field_conf.inputType = 'email';
            // field_conf.extras.inputmode = "email";
            column.editor = "input";
            column.editorParams = {
            };
            break;
        case "password":
            // field_conf.tag = 'input';
            // field_conf.inputType = 'password';
            column = null;
            break;
        case "url":
            // field_conf.tag = 'input';
            // field_conf.inputType = 'url';
            // field_conf.extras.inputmode = "url";
            column.editor = "input";
            column.editorParams = {
            };
            break;
        case "tel":
            // field_conf.tag = 'input';
            // field_conf.inputType = 'tel';
            // field_conf.extras.inputmode = "tel";
            column.editor = "input";
            column.editorParams = {
            };
            break;

        case "id":
            // field_conf.tag = 'input';
            column.editor = "input";
            column.editorParams = {
            };
            break;
        case "currency":
            // field_conf.tag = 'input';
            column.editor = "input";
            column.editorParams = {
            };
            break;
        case "hex":
            // field_conf.tag = 'input';
            column.editor = "input";
            column.editorParams = {
            };
            break;
        case "binary":
            // field_conf.tag = 'input';
            column.editor = "input";
            column.editorParams = {
            };
            break;
        case "percent":
            // field_conf.tag = 'input';
            column.editor = "input";
            column.editorParams = {
            };
            break;
        case "base64":
            // field_conf.tag = 'input';
            column.editor = "textarea";
            column.editorParams = {
            };
            break;

        default:
            log_error(sprintf("%s: type unknown '%s'", gobj_short_name(gobj), field_desc.type));
            column = null;
    }

    if(column) {
        if(readonly) {
            delete column.editor;
        }
        columns.push(column);
    }
}

/************************************************************
 *
 ************************************************************/
function get_list_alarms(gobj)
{
    if(!gobj_read_attr(gobj, "gobj_remote_yuno")) {
        log_error(`${gobj_short_name(gobj)}: No gobj_remote_yuno defined`);
        return;
    }

    let kw = {
    };
    let device_id = gobj_read_attr(gobj, "device_id");
    if(device_id) {
        kw.device_id = device_id;
    }
    let ret = gobj_send_event(
        gobj_read_attr(gobj, "gobj_remote_yuno"),
        "EV_LIST_ALARMS",
        kw,
        gobj
    );
    if(ret) {
        log_error(ret);
    }
}

/************************************************************
 *  Response of remote command
 *      [
 *          {
 *              id: "DVES_40C768"
 *              active: true
 *              alarm: "XXXX"
 *              description: "YYYYYY"
 *              device_type: Array [ "C_GATE_ENCHUFE-gate_enchufe" ]
 *              notified: "ginsmar@gmail.com"
 *              sent: true
 *              tm: 1720252167
 *              triggers: Array [ {…} ]
 *              validated: false
 *              yuno: "Gate_enchufe^gate_enchufe"
 *          },
 *          ...
 *      ]
 ************************************************************/
function procesa_list_alarms(gobj, alarms)
{
    gobj_write_bool_attr(gobj, "initialized", false);

    let $container = gobj_read_attr(gobj, "$container");
    let tabulator = gobj_read_attr(gobj, "tabulator");
    if(!tabulator) {
        let $extend = $container.querySelector('.yui-tabulator-table');
        tabulator = $extend.tabulator = create_tabulator(
            gobj,
            $extend,
            "alarms",
            gobj_read_attr(gobj, "alarms_desc")
        );
        gobj_write_attr(gobj, "tabulator", tabulator);
    }

    if(tabulator.initialized) {
        for(let alarm of alarms) {
            process_realtime_alarm(gobj, alarm);
        }
        set_changed_stated(gobj, false);
        gobj_write_bool_attr(gobj, "initialized", false);
        tabulator.redraw(); /* Delete double scrollbar */

        gobj_send_event(
            gobj,
            "EV_SELECT_ALARMS",
            {
                select: gobj_read_attr(gobj, "select")
            },
            gobj
        );
        re_subscribe_all(gobj);
        update_button_alarm_color(gobj);

    } else {
        tabulator.on("tableBuilt", function() {
            for(let alarm of alarms) {
                process_realtime_alarm(gobj, alarm);
            }
            set_changed_stated(gobj, false);
            gobj_write_bool_attr(gobj, "initialized", true);
            tabulator.redraw(); /* Delete double scrollbar */

            gobj_send_event(
                gobj,
                "EV_SELECT_ALARMS",
                {
                    select: gobj_read_attr(gobj, "select")
                },
                gobj
            );
            re_subscribe_all(gobj);
            update_button_alarm_color(gobj);
        });
    }
}

/************************************************************
 *
 ************************************************************/
function process_realtime_alarm(gobj, alarm, event)
{
    /*
     *  Search if alarm is already in table: update or add
     */
    let tabulator = gobj_read_attr(gobj, "tabulator");
    let rows = tabulator.searchRows([
        {field: "id", type: "=", value: alarm.id},
        {field: "alarm", type: "=", value: alarm.alarm},
    ]);

    if(rows.length === 0) {
        tabulator.addRow(alarm, true);
    } else {
        rows.forEach(row => {
            row.delete();
        });
        tabulator.addRow(alarm, true);
    }

    tabulator.redraw();
    tabulator.recalc(); //recalculate all column calculations
    update_button_alarm_color(gobj);
    gobj_send_event(
        gobj,
        "EV_SELECT_ALARMS",
        {
            select: gobj_read_attr(gobj, "select")
        },
        gobj
    );
}

/************************************************************
 *
 ************************************************************/
function update_button_alarm_color(gobj)
{
    let tabulator = gobj_read_attr(gobj, "tabulator");
    let data = tabulator.getData();

    let active_not_validated = 0;
    let active_validated = 0;
    let not_active_not_validated = 0;

    for(let alarm of data) {
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

    let __yui_routing__ = gobj_find_service("__yui_routing__", true);
    let $btn = gobj_read_attr(__yui_routing__, "$container").querySelector('#\\#alarms .yui-badge');
    if(!$btn) {
        return;
    }

    let color = "";
    let count = 0;
    if(active_not_validated) {
        color = "is-danger";
        count = active_not_validated;
    } else if(active_validated) {
        color = "is-warning";
        count = active_validated;
    } else if(not_active_not_validated) {
        color = "is-info";
        count = not_active_not_validated;
    }

    $btn.classList.remove("is-danger", "is-warning", "is-info");

    if(count) {
        $btn.classList.remove("is-hidden");
        $btn.classList.add(color);
        // $btn.innerHTML = `<strong>${count}</strong>`;
        $btn.textContent = count;

    } else {
        $btn.classList.add("is-hidden");
    }
}

/************************************************************
 *
 ************************************************************/
function subscribe_all(gobj, devices)
{
    let priv = gobj.priv;
    let gobj_remote_yuno = gobj_read_attr(gobj, "gobj_remote_yuno");

    /*
     *  Subscribe real time tracks
     */
    for(let j=0; j<devices.length; j++) {
        let id = devices[j];

        if(!str_in_list(priv.subscribed_devices, id)) {
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

            priv.subscribed_devices.push(id);
        }
    }
}

/************************************************************
 *
 ************************************************************/
function unsubscribe_all(gobj)
{
    let priv = gobj.priv;
    let gobj_remote_yuno = gobj_read_attr(gobj, "gobj_remote_yuno");

    for (let i=0; i<priv.subscribed_devices.length; i++) {
        let id = priv.subscribed_devices[i];

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
    }
    priv.subscribed_devices = [];
}

/************************************************************
 *
 ************************************************************/
function re_subscribe_all(gobj)
{
    let priv = gobj.priv;

    if(gobj_read_attr(gobj, "device_id")) {
        return;
    }
    let tabulator = gobj_read_attr(gobj, "tabulator");
    let rows = tabulator.getData();
    let devices = [];
    for (let row of rows) {
        let id = row.id;
        if (!str_in_list(priv.subscribed_devices, id)) {
            devices.push(id);
        }
    }
    subscribe_all(gobj, devices);
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *  From Window parent, is going to close
 ************************************************************/
function ac_window_to_close(gobj, event, kw, src)
{
    if(gobj_read_bool_attr(gobj, "changes")) {
        kw.abort_close = true; // Don't close the window until fields repaired
        kw.warning = "All changes will be lost. Are you sure?";
    }
    return 0;
}

/************************************************************
 *  Remote response
 ************************************************************/
function ac_list_alarms(gobj, event, kw, src)
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
        return 0;
    }

    if(kw_has_key(schema, "cols")) {
        gobj_write_attr(gobj, "alarms_desc", schema.cols);
    } else {
        gobj_write_attr(gobj, "alarms_desc", schema);
    }
    procesa_list_alarms(gobj, data);

    return 0;
}

/************************************************************
 *  Remote response
 ************************************************************/
function ac_realtime_alarm(gobj, event, kw, src)
{
    let result;
    let comment;
    let schema;
    let data;

    if(kw_has_key(kw, "result") && kw_has_key(kw, "schema")) {
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
            return 0;
        }
        process_realtime_alarm(gobj, data, event);
    } else {
        process_realtime_alarm(gobj, kw, event);
    }

    return 0;
}

/************************************************************
 *  Internal event, validate has changed
 ************************************************************/
function ac_record_changed(gobj, event, kw, src)
{
    // if(gobj_read_bool_attr(gobj, "initialized")) {
    //     set_changed_stated(gobj, true);
    // }
    let cell = kw.cell;
    let row = cell.getRow();
    let data = row.getData();

    let kw_set_device_alarms = {
        device_id: data.id,
        record: data
    };

    let gobj_remote_yuno = gobj_read_attr(gobj, "gobj_remote_yuno");
    let ret = gobj_send_event(
        gobj_remote_yuno,
        "EV_UPDATE_DEVICE_ALARMS",
        kw_set_device_alarms,
        gobj
    );
    if(ret) {
        log_error(ret);
    }

    return 0;
}

/************************************************************
 *  From internal form button
 *  Publish record to be saved by external clients
 ************************************************************/
function ac_save_record(gobj, event, kw, src)
{
    set_changed_stated(gobj, false);
    return 0;
}

/************************************************************
 *  From internal form button
 ************************************************************/
function ac_select_alarms(gobj, event, kw, src)
{
    let $container = gobj_read_attr(gobj, "$container");
    let buttons = $container.querySelectorAll(".buttons-select-alarms .button");
    for (let btn of buttons) {
        btn.classList.remove("is-success", "is-selected");
    }

    let cls = kw.select;
    gobj_write_attr(gobj, "select", cls);
    let button = $container.querySelector(`.buttons-select-alarms .${cls}`);
    button.classList.add("is-success", "is-selected");

    let tabulator = gobj_read_attr(gobj, "tabulator");
    tabulator.clearFilter();

    switch (cls) {
        case "no-validated":
            tabulator.setFilter([
                // {field:"active", type:"=", value:true},
                {field:"validated", type:"=", value:false}
            ]);
            break;
        case "validated":
            tabulator.setFilter([
                {field:"active", type:"=", value:true},
                {field:"validated", type:"=", value:true}
            ]);
            break;
        case "all":
            break;
    }
    // return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_refresh(gobj, event, kw, src)
{
    get_list_alarms(gobj);
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

    let tabulator = gobj_read_attr(gobj, "tabulator");
    if(1 || !tabulator || tabulator.getData().length===0) {
        // get_list_alarms(gobj); by subscription TODO las alarmas nuevas no están subscritas
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
            ["EV_LIST_ALARMS",          ac_list_alarms,         null],
            ["EV_REALTIME_ALARM",       ac_realtime_alarm,      null],
            ["EV_UPDATE_DEVICE_ALARMS", ac_realtime_alarm,      null],
            ["EV_RECORD_CHANGED",       ac_record_changed,      null],
            ["EV_SAVE_RECORD",          ac_save_record,         null],
            ["EV_SELECT_ALARMS",        ac_select_alarms,       null],
            ["EV_REFRESH",              ac_refresh,             null],
            ["EV_WINDOW_MOVED",         null,                   null],
            ["EV_WINDOW_RESIZED",       null,                   null],
            ["EV_WINDOW_TO_CLOSE",      ac_window_to_close,     null],
            ["EV_SHOW",                 ac_show,                null],
            ["EV_HIDE",                 ac_hide,                null],
            ["EV_TIMEOUT",              ac_timeout,             null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_LIST_ALARMS",          event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_REALTIME_ALARM",       event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_UPDATE_DEVICE_ALARMS", event_flag_t.EVF_PUBLIC_EVENT],
        ["EV_RECORD_CHANGED",       0],
        ["EV_SAVE_RECORD",          0],
        ["EV_SELECT_ALARMS",        0],
        ["EV_REFRESH",              0],
        ["EV_WINDOW_MOVED",         0],
        ["EV_WINDOW_RESIZED",       0],
        ["EV_WINDOW_TO_CLOSE",      0],
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
function register_c_ui_alarms()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_ui_alarms };
