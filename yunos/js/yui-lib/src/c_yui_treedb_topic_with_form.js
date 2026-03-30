/***********************************************************************
 *          c_yui_treedb_topic_with_form.js
 *
 *          Table of TreeDB topic with FORM for editing
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

import {
    __yuno__,
    SDATA,
    SDATA_END,
    data_type_t,
    event_flag_t,
    gclass_create,
    log_error,
    gobj_read_pointer_attr,
    gobj_subscribe_event,
    gobj_parent,
    sprintf,
    gobj_read_attr,
    json_deep_copy,
    createElement2,
    gobj_send_event,
    clean_name,
    is_array,
    is_string,
    is_object,
    str_in_list,
    createOneHtml,
    getPositionRelativeToBody,
    treedb_get_field_desc,
    treedb_decoder_fkey,
    empty_string,
    is_date,
    kwid_find_one_record,
    treedb_hook_data_size,
    parseBoolean,
    json_size,
    list2options,
    trace_msg,
    log_warning,
    kw_get_local_storage_value,
    kwid_get_ids,
    gobj_write_attr,
    gobj_read_bool_attr,
    gobj_read_str_attr,
    gobj_write_bool_attr,
    gobj_publish_event,
    gobj_command,
    gobj_name,
    gobj_short_name,
    refresh_language,
} from "yunetas";

import {
    get_yesnocancel,
    get_ok,
} from "./c_yui_main.js";

import {t} from "i18next";

import { JSONEditor } from 'vanilla-jsoneditor';
import "vanilla-jsoneditor/themes/jse-theme-dark.css";

import "tom-select/dist/css/tom-select.css"; // Import Tom-Select CSS
import TomSelect from "tom-select"; // Import Tom-Select JS

import { TabulatorFull as Tabulator } from "tabulator-tables";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_TREEDB_TOPIC_WITH_FORM";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
/*---------------- Public Attributes ----------------*/
SDATA(data_type_t.DTP_POINTER,  "subscriber",           0,  null,   "Subscriber of output events"),
SDATA(data_type_t.DTP_STRING,   "treedb_name",          0,  null,   "Remote service treedb name"),
SDATA(data_type_t.DTP_STRING,   "topic_name",           0,  null,   "Topic name"),
SDATA(data_type_t.DTP_POINTER,  "desc",                 0,  null,   "Description of topics"),

/*---------------- Edition Mode ----------------*/
SDATA(data_type_t.DTP_BOOLEAN,  "with_edition_mode",    0,  true,   "Enable EDITION mode showing EDIT Button toolbar"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_new_button",      0,  true,   "Button toolbar NEW"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_delete_button",   0,  true,   "Button toolbar DELETE"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_copy_button",     0,  true,   "Button toolbar COPY"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_paste_button",         0,  true,   "Button toolbar PASTE"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_refresh_button",       0,  true,   "Button toolbar REFRESH"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_search_button",        0,  true,   "Button toolbar SEARCH"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_in_row_edit_icons",    0,  true,   "Add a last column with internal EDIT/DELETE icon"),

SDATA(data_type_t.DTP_BOOLEAN,  "editable",             0,  false,  "Edit state"),

/*---------------- Selection Mode ----------------*/
SDATA(data_type_t.DTP_BOOLEAN,  "with_checkbox",        0,  true,   "Auxiliary first column to select rows"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_radio",           0,  false,  "Auxiliary first column to select one row"),
SDATA(data_type_t.DTP_BOOLEAN,  "broadcast_select_rows_event",   0,  false, "Broadcast select rows event"),
SDATA(data_type_t.DTP_BOOLEAN,  "broadcast_unselect_rows_event", 0,  false, "Broadcast unselect rows event"),

/*---------------- Tabulator Defaults ----------------*/
SDATA(data_type_t.DTP_JSON,     "tabulator_settings",   0,  {
    layout: "fitDataFill",
    columnDefaults: {
        resizable: true
    },
    pagination: true,
    paginationSize: 25,
    paginationSizeSelector: [25, 50, 100, true],
    placeholder: "No data available",
    maxHeight: "100%"
}, "Default settings for Tabulator"),

/*---------------- Internal Attributes ----------------*/
SDATA(data_type_t.DTP_POINTER,  "$container",           0,  null,   "HTML container for UI"),
SDATA(data_type_t.DTP_POINTER,  "tabulator",            0,  null,   "Tabulator instance"),
SDATA(data_type_t.DTP_STRING,   "table_id",             0,  null,   "Table div ID"),
SDATA(data_type_t.DTP_STRING,   "toolbar_id",           0,  null,   "Toolbar ID"),
SDATA(data_type_t.DTP_STRING,   "form_id",              0,  null,   "Edit form ID"),
SDATA(data_type_t.DTP_STRING,   "popup_id",             0,  null,   "Edit form popup ID"),

SDATA_END()
];

let PRIVATE_DATA = {
    $container:         null,
    treedb_name:        "",
    topic_name:         "",
    tabulator:          null,
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
    let name = clean_name(gobj_name(gobj));
    gobj_write_attr(gobj, "table_id", "table" + name);
    gobj_write_attr(gobj, "toolbar_id", "toolbar" + name);
    gobj_write_attr(gobj, "form_id", "form" + name);
    gobj_write_attr(gobj, "popup_id", "popup" + name);

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
    create_tabulator(gobj);
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    table__destroy(gobj);
}

/***************************************************************
 *          Framework Method: Destroy
 ***************************************************************/
function mt_destroy(gobj)
{
    destroy_ui(gobj);
}

/************************************************************
 *      Framework Method command
 ************************************************************/
function mt_command_parser(gobj, command, kw, src)
{
    switch(command) {
        case "help":
            return cmd_help(gobj, command, kw, src);
        case "get_topic_data":
            return cmd_get_topic_data(gobj, command, kw, src);
        default:
            log_error("Command not found: %s", command);
            return {
                "result": -1,
                "comment": sprintf("Command not found: %s", command),
                "schema": null,
                "data": null
            };
    }
}




                    /***************************
                     *      Commands
                     ***************************/




/************************************************************
 *
 ************************************************************/
function cmd_help(gobj, cmd, kw, src)
{
    return {
        "result": 0,
        "comment": "",
        "schema": null,
        "data": null
    };
}

/************************************************************
 *
 ************************************************************/
function cmd_get_topic_data(gobj, cmd, kw, src)
{
    let webix = {
        "result": 0,
        "comment": "",
        "schema": null,
        "data": null
    };

    let tabulator = gobj_read_attr(gobj, "tabulator");
    webix.data = tabulator.getData();
    return webix;
}




                    /***************************
                     *      Local Methods
                     ***************************/




/************************************************************
 *
 ************************************************************/
// Function to read text from the clipboard
async function readClipboard(gobj)
{
    try {
        const text = await navigator.clipboard.readText();
        let kw = JSON.parse(text);
        gobj_send_event(gobj, "EV_PASTE_ROWS", kw, gobj);
    } catch (error) {
        log_error(`Failed to read clipboard contents: ${error}`);
    }
}

/************************************************************
 *   Build UI
 ************************************************************/
function build_ui(gobj)
{
    function create_table_toolbar()
    {
        // TODO pon autorización, solo si está autorizado a modificar los datos!!!
        // TODO deja que estos botones se queden en el top cuando se hace scroll (clip ?)
        let $table_toolbar = [];
        let with_edition_mode = gobj_read_bool_attr(gobj, "with_edition_mode");
        let with_new_button = gobj_read_bool_attr(gobj, "with_new_button");
        let with_delete_button = gobj_read_bool_attr(gobj, "with_delete_button");
        let with_copy_button = gobj_read_bool_attr(gobj, "with_copy_button");
        let with_paste_button = gobj_read_bool_attr(gobj, "with_paste_button");

        let toolbar_id = gobj_read_str_attr(gobj, "toolbar_id");

        if(with_edition_mode) {
            $table_toolbar = createElement2(
                ['div', {id: `${toolbar_id}`, class: 'buttons mb-0'}]
            );
            let $edit_button = createElement2(
                ['button', {id: ``, class: 'button button-edit-record mr-1'}, [
                    ['i', {class: 'yi-pen'}],
                    ['span',
                        {
                            class: 'is-hidden-mobile', i18n: 'edit', style: 'padding-left:5px;'
                        },
                        'edit'
                    ]
                ], {
                    'click': (event) => {
                        /////////////////////////////////////////////////////
                        //  WARNING
                        //  if do not stop propagation, the event will arrive until body,
                        //  and a auto-closing function will remove is-active class
                        //  and this popup will not open
                        //  (Bulma components that it opens with toggling "is-active" class)
                        /////////////////////////////////////////////////////
                        event.stopPropagation();
                        gobj_send_event(gobj, "EV_EDITION_MODE", {}, gobj);
                    }
                }
            ]);
            $table_toolbar.appendChild($edit_button);

            if(with_new_button) {
                let $new_button = createElement2(
                    ['button', {id: ``, class: 'button button-new-record mr-1', disabled: true}, [
                        ['i', {class: 'yi-plus'}],
                        ['span',
                            {
                                class: 'is-hidden-mobile', i18n: 'new', style: 'padding-left:5px;'
                            },
                            'new'
                        ]
                    ], {
                        'click': (event) => {
                            event.stopPropagation();
                            gobj_send_event(gobj, "EV_NEW_ROW", {}, gobj);
                        }
                    }
                ]);
                $table_toolbar.appendChild($new_button);
            }

            if(with_delete_button) {
                let $delete_button = createElement2(
                    ['button', {id: ``, class: 'button button-delete-record mr-1', disabled: true}, [
                        ['i', {class: 'yi-trash'}],
                        ['span',
                            {
                                class: 'is-hidden-mobile', i18n: 'delete', style: 'padding-left:5px;'
                            },
                            'delete'
                        ]
                    ], {
                        'click': (event) => {
                            event.stopPropagation();
                            // Delete selected rows ({} empty)
                            gobj_send_event(gobj, "EV_DELETE_ROWS", {}, gobj);
                        }
                    }
                ]);
                $table_toolbar.appendChild($delete_button);
            }

            if(with_copy_button) {
                let $copy_button = createElement2(
                    ['button', {id: ``, class: 'button button-copy-record mr-1', disabled: true}, [
                        ['i', {class: 'yi-copy'}],
                        ['span',
                            {
                                class: 'is-hidden-mobile', i18n: 'copy', style: 'padding-left:5px;'
                            },
                            'copy'
                        ]
                    ], {
                        'click': (event) => {
                            event.stopPropagation();
                            // copy selected rows ({} empty)
                            gobj_send_event(gobj, "EV_COPY_ROWS", {}, gobj);
                        }
                    }
                    ]);
                $table_toolbar.appendChild($copy_button);
            }

            if(with_paste_button) {
                let $paste_button = createElement2(
                    ['button', {id: ``, class: 'button button-paste-record mr-1', disabled: true}, [
                        ['i', {class: 'yi-paste'}],
                        ['span',
                            {
                                class: 'is-hidden-mobile', i18n: 'paste', style: 'padding-left:5px;'
                            },
                            'paste'
                        ]
                    ], {
                        'click': async function(event) {
                            event.stopPropagation();
                            await readClipboard(gobj);
                        }
                    }]
                );
                $table_toolbar.appendChild($paste_button);
            }
        }
        return $table_toolbar;
    }

    let $table_toolbar = create_table_toolbar();

    /*----------------------------------------------*
     *  View toolbar: Search, Refresh
     *  Always visible, independent of edition mode
     *----------------------------------------------*/
    let toolbar_id = gobj_read_str_attr(gobj, "toolbar_id");

    let $view_toolbar = createElement2(['div', {class: 'is-flex is-align-items-center'}]);

    let with_refresh_button     = gobj_read_bool_attr(gobj, "with_refresh_button");
    let with_search_button      = gobj_read_bool_attr(gobj, "with_search_button");

    if(with_search_button) {
        let search_id = `${toolbar_id}_search`;
        let $search_box = createElement2(
            ['div', {class: 'control has-icons-left has-icons-right mr-1'}, [
                ['input', {
                    id: search_id,
                    class: 'input',
                    type: 'text',
                    placeholder: 'search...',
                    style: 'max-width:200px;'
                }],
                ['span', {class: 'icon is-left'}, [
                    ['i', {class: 'yi-magnifying-glass'}]
                ]],
                ['span', {class: 'icon is-right', style: 'cursor:pointer; pointer-events:all;', title: 'clear search'}, [
                    ['i', {class: 'yi-xmark'}]
                ], {
                    'click': (event) => {
                        event.stopPropagation();
                        let $input = document.getElementById(search_id);
                        if($input) { $input.value = ''; $input.dispatchEvent(new Event('input')); }
                    }
                }]
            ]]
        );
        let $search_input = $search_box.querySelector(`#${search_id}`);
        if($search_input) {
            $search_input.addEventListener('input', (event) => {
                let searchText = event.target.value.trim().toLowerCase();
                let tabulator = gobj_read_attr(gobj, "tabulator");
                if(!tabulator) {
                    return;
                }
                if(searchText) {
                    tabulator.setFilter(function(data) {
                        return Object.entries(data).some(([key, val]) => {
                            if(key.startsWith('_')) {
                                return false;
                            }
                            if(val === null || val === undefined) {
                                return false;
                            }
                            return String(val).toLowerCase().includes(searchText);
                        });
                    });
                } else {
                    tabulator.clearFilter();
                }
            });
        }
        $view_toolbar.appendChild($search_box);
    }

    if(with_refresh_button) {
        let $refresh = createElement2(
            ['button', {class: 'button mr-1', title: 'refresh'}, [
                ['i', {class: 'yi-arrows-rotate'}],
                ['span', {class: 'is-hidden-mobile', i18n: 'refresh', style: 'padding-left:5px;'}, 'refresh']
            ], {
                'click': (event) => {
                    event.stopPropagation();
                    gobj_send_event(gobj, "EV_REFRESH", {}, gobj);
                }
            }]
        );
        $view_toolbar.appendChild($refresh);
    }

    /*----------------------------------------------*
     *  Layout Schema
     *----------------------------------------------*/
    let table_id = gobj_read_str_attr(gobj, "table_id");
    let $container = createElement2(
        ['div', {class: 'container-treedb-topic-with-form', style: 'height:100%;'}, [
            ['div', {class: 'toolbar_tabulator_table m-1', style: 'display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap;'}],
            ['div',
                {
                    id: `${table_id}`,
                    style: 'margin-top:0px !important;',
                }
            ]
        ]]
    );
    let $toolbar_slot = $container.querySelector('.toolbar_tabulator_table');
    if($table_toolbar instanceof Element) {
        $toolbar_slot.appendChild($table_toolbar);
    } else {
        $toolbar_slot.appendChild(createElement2(['div', {}]));
    }
    $toolbar_slot.appendChild($view_toolbar);

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

/***************************************************************************
 *  Function to convert dataset (of DOM Element) to plain object
 ***************************************************************************/
function datasetToObject(dataset)
{
    return Object.fromEntries(
        Object.entries(dataset).map(([key, value]) => [key, value])
    );
}

/******************************************************************
 *   Build table with Tabulator
 *   This fn is called on start and when desc attribute is set.
 *   desc contains the description (columns) of table to create
 ******************************************************************/
function create_tabulator(gobj)
{
    let table_id = gobj_read_str_attr(gobj, "table_id");
    let desc = gobj_read_attr(gobj, "desc");

    let columns = [];

    /*
     *  Add the checkbox/radio selection column.
     */
    let with_checkbox = gobj_read_bool_attr(gobj, "with_checkbox");
    let with_radio = gobj_read_bool_attr(gobj, "with_radio");
    if(with_checkbox) {
        columns.push({
            formatter: "rowSelection",
            titleFormatter: "rowSelection",
            field: "_check_box_state_",  // WARNING _check_box_state_ widely used
            hozAlign: "center",
            headerHozAlign: "center",
            width: 40,
            visible: false,
            headerSort: false
        });
    } else if(with_radio) {
        columns.push({
            formatter: "rowSelection",
            field: "_check_box_state_",  // WARNING _check_box_state_ widely used
            hozAlign: "center",
            width: 40,
            visible: false,
            headerSort: false
        });
    }

    /*
     *  Cell formatter — called on every cell display.
     *  Tabulator only accepts: string, number, boolean, DOM Node, null, or undefined.
     *  Returning any other object (e.g. Array, plain {}) triggers a console.warn and
     *  renders an empty cell (see tabulator Cell.js _generateContents).
     *  transform__treedb_value_2_table_value() is responsible for the conversion;
     *  it has a final guard that JSON.stringifies any object that slips through.
     */
    function formatter(cell, formatterParams, onRendered)
    {
        let value = cell.getValue();
        let row = cell.getData();
        let field = cell.getField();
        let col = get_schema_col(gobj, field);
        if(col) {
            return transform__treedb_value_2_table_value(gobj, col, value, row, field);
        }
        return "???";
    }

    for (let i = 0; i < desc.cols.length; i++) {
        let col = desc.cols[i];
        if(!col.id || col.id[0]==='_') {
            continue;
        }

        let hozAlign;
        let sorter = "string";
        let cellClick;
        const field_desc = treedb_get_field_desc(col);
        if(field_desc.is_hidden) {
            continue;
        }
        switch(field_desc.type) {
            case "hook":
                hozAlign = "center";
                cellClick = function(e, cell) {
                    let target = e.target.closest('.hook_cell');
                    if(!target) {
                        return;
                    }
                    e.stopPropagation();
                    let data = Object.fromEntries(
                        Object.entries(target.dataset).map(([k, v]) => [k, v])
                    );
                    let kw_hook = {};
                    if(data && data.row_id) {
                        let pos = getPositionRelativeToBody(target);
                        kw_hook = {
                            treedb_name: gobj_read_attr(gobj, "treedb_name"),
                            topic_name: gobj_read_attr(gobj, "topic_name"),
                            row_id: data.row_id,
                            col_id: data.col_id,
                            click_x: pos.left,
                            click_y: pos.top
                        };
                    }
                    gobj_send_event(gobj, "EV_SHOW_HOOK_DATA", kw_hook, gobj);
                };
                break;
            case "boolean":
                hozAlign = "center";
                sorter = "boolean";
                break;
            case "integer":
            case "real":
                hozAlign = "right";
                sorter = "number";
                break;
        }

        let colDef = {
            title: col.header || col.id,
            field: col.id,
            sorter: sorter,
            hozAlign: hozAlign,
            formatter: formatter,
        };
        if(cellClick) {
            colDef.cellClick = cellClick;
        }
        columns.push(colDef);
    }

    /*
     *  Column with operators: edit, delete
     */
    function operateFormatter(cell, formatterParams, onRendered) {
        return [
            '<button class="button without-border px-2 edit">',
                '<i style="" class="yi-pen has-text-link"></i>',
            '</button>',
            '<button class="button without-border px-2 remove">',
                '<i style="" class="yi-trash has-text-danger"></i>',
            '</button>'
        ].join('');
    }

    let with_in_row_edit_icons = gobj_read_bool_attr(gobj, "with_in_row_edit_icons");
    if(with_in_row_edit_icons) {
        columns.push({
            field: '_operation',
            title: 'Op',
            hozAlign: 'center',
            frozen: "right",
            visible: false,
            headerSort: false,
            formatter: operateFormatter,
            cellClick: function(e, cell) {
                let row = cell.getRow().getData();
                let index = cell.getRow().getPosition();
                if(e.target.closest('.edit')) {
                    e.stopPropagation();
                    show_edit_form(gobj, row, index);
                } else if(e.target.closest('.remove')) {
                    e.stopPropagation();
                    gobj_send_event(gobj, "EV_DELETE_ROWS", {index: index, row: row}, gobj);
                }
            }
        });
    }

    let pkey = desc.pkey || "id";
    let selectable = with_checkbox ? true : (with_radio ? 1 : false);

    let tabulator_settings = json_deep_copy(gobj_read_attr(gobj, "tabulator_settings"));

    Object.assign(tabulator_settings, {
        index: pkey,
        columns: columns,
        selectableRows: selectable,
    });

    let tabulator = new Tabulator(`#${table_id}`, tabulator_settings);
    tabulator._ready = false;
    tabulator.on("tableBuilt", function() {
        tabulator._ready = true;
        if(tabulator._pendingData !== undefined) {
            tabulator.setData(tabulator._pendingData);
            delete tabulator._pendingData;
        }
    });
    tabulator.on("rowSelected", function(row) {
        gobj_send_event(gobj, "EV_SELECT_ROWS", {rows: [row.getData()]}, gobj);
    });
    tabulator.on("rowDeselected", function(row) {
        gobj_send_event(gobj, "EV_UNSELECT_ROWS", {rows: [row.getData()]}, gobj);
    });
    gobj_write_attr(gobj, "tabulator", tabulator);

    refresh_language(gobj_read_attr(gobj, "$container"), t);
}

/************************************************************
 *   Destroy Tabulator instance
 ************************************************************/
function table__destroy(gobj)
{
    let tabulator = gobj_read_attr(gobj, "tabulator");
    if(tabulator) {
        tabulator.destroy();
        gobj_write_attr(gobj, "tabulator", null);
    }
}

/************************************************************
 *  Convert a record column value from backend to frontend
 ************************************************************/
function transform__treedb_value_2_table_value(gobj, col, value, row, field)
{
    let priv = gobj.priv;
    const field_desc = treedb_get_field_desc(col);

    switch(field_desc.type) {
        case "string":
            break;
        case "integer":
            break;
        case "real":
            break;
        case "boolean":
            if(value) {
                value = `<span class=""><i style="color:limegreen; font-size:1.2rem;" class="yi-square-check"></i></i></span>`;
            } else {
                value = `<span class=""><i style="color:orangered; font-size:1.2rem;" class="yi-xmark"></i></span>`;
            }
            break;
        case "object":
        case "dict":
        case "template":
            value = JSON.stringify(value);
            break;
        case "array":
        case "list":
            value = JSON.stringify(value);
            break;
        case "coordinates":
        case "blob":
        case "gbuffer":
            value = JSON.stringify(value);
            break;

        case "enum":
            switch(field_desc.real_type) {
                case "string":
                    break;
                case "object":
                case "dict":
                case "array":
                case "list":
                    value = JSON.stringify(value);
                    break;
            }
            break;

        case "hook":    // Convert data from backend to frontend TABLE CELL
            let items = treedb_hook_data_size(value);

            if(items > 0) {
                value = [
                    '<a class="hook_cell" ',
                    `data-row_id="${row.id}" `,
                    `data-col_id="${col.id}" > `,
                    '<span style="" class="icon yi-eye"></span>',
                    `<span>[&nbsp;<u>${items}</u>&nbsp;]</span>`,
                    '</a>'
                ].join('');
            } else {
                value = "";
            }
            break;

        case "fkey":    // Convert data from backend to frontend TABLE CELL
            let new_value = [];
            if(value) {
                if(is_string(value)) {
                    let fkey = treedb_decoder_fkey(col, value);
                    if(fkey) {
                        new_value.push(fkey.id);
                    }
                } else if(is_array(value)) {
                    for(let i=0; i<value.length; i++) {
                        let fkey = treedb_decoder_fkey(col, value[i]);
                        if(fkey) {
                            new_value.push(fkey.id);
                        }
                    }
                } else {
                    log_error("fkey type unsupported: " + JSON.stringify(value));
                }
            }

            value = new_value.join(", ");
            break;

        case "now":
        case "time":
            switch(field_desc.real_type) {
                case "string":
                case "integer":
                    value = parseInt(value) || 0;
                    value = new Date(value*1000);

                    const userLocale = navigator.language;
                    const opts = {
                        day: 'numeric',
                        month: 'short',   // 'long' for full month name, 'short' for abbreviated, 'narrow' for shortest
                        year: 'numeric',
                        hour: '2-digit',
                        minute: '2-digit',
                        second: '2-digit',
                        hour12: true, // Use true for AM/PM or false for 24-hour format, depending on locale preference
                        // timeZoneName: 'short' // Options are 'short' or 'long'
                    };

                    const formatter = new Intl.DateTimeFormat(userLocale, opts);
                    value = '<span class="is-size-7">' + formatter.format(value) + '</span>';
                    break;
            }
            break;

        case "color":
            switch(field_desc.real_type) {
                case "string":
                    // TODO
                    break;
                case "integer":
                    // TODO
                    break;
            }
            break;

        case "image":
            value = `<img src="${value}" alt="${value}" width="60" height="30" title="">`;
            break;

        default:
            log_error(`transform__treedb_value_2_table_value() unhandled type '${field_desc.type}' (real_type='${field_desc.real_type}') for field '${field}', topic ${priv.topic_name}`);
            break;

    }

    // Tabulator only accepts string, number, boolean, DOM Node, null, or undefined.
    // Any other object (Array, plain {}) produces a console.warn and an empty cell.
    // This guard catches cases where the backend sends an unexpected type for a field
    // (e.g. list_dict=1 converting an unset string/enum field to []).
    if(value !== null && value !== undefined && typeof value === "object" && !(value instanceof Node)) {
        log_error(`transform__treedb_value_2_table_value() unexpected object value for field '${field}' (type='${field_desc.type}', real_type='${field_desc.real_type}'): ${JSON.stringify(value)}, topic ${priv.topic_name}`);
        value = JSON.stringify(value);
    }

    return value;
}

/******************************************************************
 *   Build the topic form with bulma and select2
 ******************************************************************/
function build_topic_form(gobj)
{
    let desc = gobj_read_attr(gobj, "desc");
    let form_id = gobj_read_str_attr(gobj, "form_id");
    let $form = createElement2(['form', {id: form_id, class: 'box p-3', novalidate: ''}]);

    for (let i = 0; i < desc.cols.length; i++) {
        let col = desc.cols[i];
        if(col.id.charAt(0)==='_') {
            continue;
        }
        let [tag, form_input_type, options, extras] =
            transform__treedb_type_2_form_type(gobj, col);

        if(!tag) {
            continue;
        }
        let id = col.id;
        let flag = col.flag;

        let is_writable = str_in_list(flag, "writable");
        let is_notnull = str_in_list(flag, "notnull");
        let is_rowid = str_in_list(flag, "rowid");
        let is_required = str_in_list(flag, "required");
        let is_hidden = str_in_list(flag, "hidden");
        let is_persistent = str_in_list(flag, "persistent");
        let is_hook = str_in_list(flag, "hook");
        let is_fkey = str_in_list(flag, "fkey");

        if(1) {
            /*
             *  update mode by default
             */
            if(id === desc.pkey) {
                is_writable = false;
            }
        }

        // TODO chequea en backend que los writable no son actualizados desde fuera de programa
        // if(is_persistent) {
        //     is_writable = true;
        // }

        if(is_fkey) {
            // TODO no debería estar definido con writable?
            // TODO pueden haber enlaces que solo se puedan hacer por programa
            is_writable = true;
        }

        if(!is_writable) {
            if(id !== desc.pkey) {
                // Show only fields writable (except pkey)
                continue;
            }
        }
        if(is_hidden) {
            continue;
        }

        if(is_rowid) {
            is_writable = false;
            is_required = false;
            is_notnull = false;
        }

        let field_conf = {
            tag: tag,
            inputType: form_input_type,
            name: col.id,
            label: t(col.header || col.id),
            placeholder: "",
            options: options,   // select,select2,radio options
            extras: extras,     // extra html input/textarea attributes
            readonly: !is_writable,
            required: is_required,
            hidden: is_hidden,
            not_null: is_notnull,
            is_pkey: (id === desc.pkey),
            is_rowid: is_rowid
        };

        let $field = create_html_field(field_conf);
        $field.field_conf = field_conf;

        $form.appendChild($field);
    }
    return $form;
}

/************************************************************
 *  Convert a col type from backend to frontend
 *  Return data to build the HTMLElement
 *
 *  Options format for select/select2:
 *      <option value="reading">Reading</option>
 ************************************************************/
function transform__treedb_type_2_form_type(gobj, col)
{
    const field_desc = treedb_get_field_desc(col);

    let tag = null;
    let inputType = '';
    let options = [];
    let extras = {};    // extra html input/textarea attributes

    switch(field_desc.type) {
        case "image":
        case "string":
            tag = 'input';
            inputType = 'text';
            break;
        case "integer":
            tag = 'input';
            extras.inputmode = "numeric";
            break;
        case "real":
            tag = 'input';
            extras.inputmode = "decimal";
            break;

        case "rowid":
            tag = 'input';
            inputType = 'text';
            break;

        case "boolean":
            tag = 'checkbox';
            break;

        case "now":
        case "time":
            tag = 'input';
            inputType = 'datetime-local';
            switch(field_desc.real_type) {
                case "string":
                    break;
                case "integer":
                    break;
            }
            break;
        case "color":
            tag = 'input';
            inputType = 'color';
            switch(field_desc.real_type) {
                case "string":
                    break;
                case "integer":
                    break;
            }
            break;
        case "email":
            tag = 'input';
            inputType = 'email';
            extras.inputmode = "email";
            break;
        case "password":
            tag = 'input';
            inputType = 'password';
            break;
        case "url":
            tag = 'input';
            inputType = 'url';
            extras.inputmode = "url";
            break;
        case "tel":
            tag = 'input';
            inputType = 'tel';
            extras.inputmode = "tel";
            break;

        case "object":
        case "dict":
        case "template":
        case "array":
        case "list":
        case "coordinates":
        case "blob":
            tag = 'jsoneditor';
            break;

        case "enum":
            tag = 'select2';
            switch(field_desc.real_type) {
                case "string": // TODO review types
                case "object":
                case "dict":
                case "array":
                case "list":
                    options =  list2options(field_desc.enum_list, "id", "id");
                    break;
            }
            break;

        case "hook":
            // hooks don't appear in form
            break;

        case "fkey":
            tag = 'select2';
            switch(field_desc.real_type) {
                case "string": // TODO review types
                case "object":
                case "dict":
                case "array":
                case "list":
                    options = get_fkey_options(gobj, col);
                    break;
            }
            break;
    }

    return [tag, inputType, options, extras];
}

/******************************************************************
 *  Helper function to create different form elements
 *  Available types:
 *      - input
 *      - textarea
 *      - select
 *      - select2
 *      - checkbox
 *      - radio
 *      - jsoneditor
 *
 *  {
 *      tag: field tag,
 *      inputType: input type,
 *      name: id
 *      label: ""
 *      placeholder: "",
 *      options: [{id:"", value:""}]]
 *      extras: {}
 *      readonly: bool
 *      required: bool
 *      hidden: bool
 *      not_null: bool
 *      _col: col
 *  }
 ******************************************************************/
function create_html_field(
    {
        tag,
        inputType,
        name,
        label,
        placeholder,
        options,
        extras, // extra html input/textarea attributes
        readonly,
        required,
        hidden,
        not_null
    }
)
{
    let base;
    switch (tag) {
        case 'input':
        case 'textarea':
        case 'select':
        case 'select2':
        case 'checkbox':
        case 'radio':
        case 'jsoneditor':
            base = [
                'div', {class: 'xfield field is-horizontal'}, [
                    ['div', {class: 'field-label is-normal' }, [
                        ['label', {class:'label ', i18n:label, for:name}, label]
                    ]],

                    ['div', {class: 'field-body'}, [
                        ['div', {class: 'field'}, [
                            ['div', {class: 'control' }, []]
                        ]]
                    ]]
                ]
            ];
            break;

        default:
            base = ['div', {class: 'control'}, 'Unsupported element type'];
    }

    let $element = createElement2(base);
    let $control = $element.querySelector('.control');

    switch (tag) {
        case 'input':
        {
            let attrs = {
                class: `input ${name==='id'?'with-focus':''}`,
                name: name,
                type: inputType,
                placeholder: placeholder
            };
            Object.assign(attrs, extras);

            if(readonly) {
                attrs.readonly = '';
            }
            let extend = ['input', attrs, '', {
                'blur': function (evt) {
                    validate_field_on_blur(this);
                }
            }];

            let $extend = createElement2(extend);
            $control.appendChild($extend);
            $control.appendChild(createElement2(['p', {class: 'help is-danger', style: 'display:none'}, '']));
            break;
        }

        case 'jsoneditor':
        {
            let attrs = {
                class: 'jsoneditor jse-theme-dark',
                style: ''
            };
            Object.assign(attrs, extras);

            if(readonly) {
                attrs.readonly = '';
            }
            let extend = ['div', attrs];
            let $extend = createElement2(extend);
            $extend.setAttribute('name', name);
            $control.appendChild($extend);
            $control.appendChild(createElement2(['p', {class: 'help is-danger', style: 'display:none'}, '']));
            break;
        }

        case 'textarea':
        {
            let attrs = {class:'textarea', name:name, placeholder: placeholder};
            Object.assign(attrs, extras);

            if(readonly) {
                attrs.readonly = '';
            }
            let extend = ['textarea', attrs, '', {
                'blur': function (evt) {
                    validate_field_on_blur(this);
                }
            }];
            let $extend = createElement2(extend);
            $control.appendChild($extend);
            $control.appendChild(createElement2(['p', {class: 'help is-danger', style: 'display:none'}, '']));
            break;
        }

        case 'select':
        {
            // {id:"id", value:"name"}
            // <option value="id" data-i18n:"name"> name </option>
            let extend = ['div', {class: 'select' },
                ['select',
                    {name:name},
                    options.map(option =>
                        ['option', {value:option.id, i18n:option.value}, option.value]
                    )
                ]
            ];
            let $extend = createElement2(extend);
            $control.appendChild($extend);
            $control.appendChild(createElement2(['p', {class: 'help is-danger', style: 'display:none'}, '']));
            break;
        }

        case 'select2':
        {
            // {id:"id", value:"name"}
            // <option value="id" data-i18n:"name"> name </option>
            let extend = ['select',
                {
                    name:name,
                    class: 'select2-multiple',
                    multiple:'multiple',
                    style: 'width: 100% !important;'
                },
                options.map(option =>
                    ['option', {value:option.id, i18n:option.value}, option.value]
                )
            ];
            let $extend = createElement2(extend);
            $control.appendChild($extend);
            $control.appendChild(createElement2(['p', {class: 'help is-danger', style: 'display:none'}, '']));
            break;
        }

        case 'checkbox':
        {
            let extend = ['label', {class: 'checkbox'},
                ['input', {type: 'checkbox', name:name}]
            ];
            let $extend = createElement2(extend);
            $control.appendChild($extend);
            $control.appendChild(createElement2(['p', {class: 'help is-danger', style: 'display:none'}, '']));
            break;
        }

        case 'radio':
        {
            for(const option of options) {
                // WARNING here can't use data-i18n, the radio element is destroyed.
                let elm = `
                   <label class="radio">
                      <input type="radio" name="${name}">
                      ${t(option)}
                    </label>
                `;

                let $extend = createOneHtml(elm);
                $control.appendChild($extend);
            }
            $control.appendChild(createElement2(['p', {class: 'help is-danger', style: 'display:none'}, '']));
            break;
        }

        default:
            log_error(`Unsupported element type, tag ${tag}`);
    }

    return $element;
}

/******************************************************************
 *   Build topic form popup with bulma
 ******************************************************************/
function build_topic_modal(gobj)
{
    const $form = build_topic_form(gobj);
    let topic_name = gobj_read_str_attr(gobj, "topic_name");
    let title = t(topic_name);
    let popup_id = gobj_read_str_attr(gobj, "popup_id");

    let $element = createElement2([
        'div', {id: popup_id, class: 'modal' }, [
            ['div', { class: 'modal-background' }, ''],
            ['div', { class: 'modal-card modal-is-responsive' }, [
                ['header', { class: 'modal-card-head p-3' }, [
                    ['p', { class: 'modal-card-title', style:'text-align:center;' }, title],
                    ['button', {class:"delete is-large", "aria-label":"close"}]
                ]],
                ['section', { class: 'modal-card-body p-1' }, [
                    $form
                ]],
                ['footer', { class: 'modal-card-foot p-3' }, [
                    ['div', {class: 'buttons', style: 'justify-content:center; width:100%;'}, [
                        ['button', { class: 'button is-success px-4', type: 'submit'}, [
                            ['span', {class:'icon'}, [
                                ['i', {class:'yi-floppy-disk'}]
                            ]],
                            ['span', {i18n:'save'}, 'save']
                        ], {
                            click: function(evt) {
                                evt.stopPropagation();
                                evt.preventDefault();
                                let $form = $element.querySelector('form');
                                if (validate_form(gobj, $form)) {
                                    let values = get_form_values($form);
                                    let ret;
                                    if($form.__mode__==="create") {
                                        ret = gobj_send_event(gobj, "EV_CREATE_RECORD", values, gobj);
                                    } else {
                                        ret = gobj_send_event(gobj, "EV_UPDATE_RECORD", values, gobj);
                                    }
                                    if(ret === 0) {
                                        $element.classList.remove('is-active');
                                    }
                                }
                            }
                        }],

                        // TODO disable/enable buttons save and undo accord to form state

                        ['button', { class: 'button px-4 form-close'}, [
                            ['span', {class:'icon'}, [
                                ['i', {class:'yi-xmark'}]
                            ]],
                            ['span', {i18n:'cancel'}, 'cancel']
                        ]],

                        ['button', { class: 'button px-4'}, [
                            ['span', {class:'icon'}, [
                                ['i', {class:'yi-broom'}]
                            ]],
                            ['span', {i18n:'clear'}, 'clear']
                        ], {
                            click: function(evt) {
                                evt.stopPropagation();
                                clear_validation(gobj, $form);
                                clear_data(gobj, $form);
                            }
                        }]
                    ]]
                ]]
            ]]
        ], {
            click: function (evt) {
                // Capture bubble events
                evt.stopPropagation();
            }
        }
    ]);

    /*------------------------*
     *      Add events
     *------------------------*/
    /*
     *  Button close
     */
    $element.querySelectorAll('.modal-close, .form-close, .delete').forEach(function (element) {
        element.addEventListener('click', function(event) {
            event.stopPropagation();
            // TODO ver si hay algo modificado y preguntar si se desecha
            $element.classList.remove('is-active');
        });
    });

    refresh_language($element, t);

    /*-----------------------------------------*
     *          Add to popup layer !!!
     *-----------------------------------------*/
    document.getElementById("popup-layer").appendChild($element);

    // jQuery($element).off('click').on('click', function(evt) {
    //     evt.stopPropagation();
    //     evt.preventDefault();
    // });

    /*------------------------*
     *      Jsoneditor
     *------------------------*/
    $element.querySelectorAll('.jsoneditor').forEach(function (element) {
        let font_family = "DejaVu Sans Mono, monospace";
        let sz = 16;
        element.style.setProperty('--jse-font-size-mono', sz + 'px');
        element.style.setProperty('--jse-font-family-mono', font_family);

        element.jsoneditor = new JSONEditor({
            target: element,
            props: {
                timestampTag: function({field, value, path}) {
                    return field === '__t__' || field === '__tm__' || field === 'tm' ||
                        field === 'from_t' || field === 'to_t' || field === 't' ||
                        field === 't_input' || field === 't_output' ||
                        field === 'from_tm' || field === 'to_tm' || field === 'time';
                },
            }
        });
    });

    /*---------------------------------------------------------*
     *  Extensions of jquery must be done after DOM attaching
     *---------------------------------------------------------*/
    // let $$x = jQuery($element).find('.select2-multiple');
    // if($$x && $$x.select2) {
    //     $$x.select2();
    // }

    document.querySelectorAll(".select2-multiple").forEach((el) => {
        new TomSelect(el, {
            plugins: ["remove_button"], // Optional: Adds a remove button for multiple selections
            create: false, // Set to `true` if you want users to add new options
            hideSelected: true,
            closeAfterSelect: false
        });
    });

    return $element;
}

/************************************************************
 *  Show a validation error on a field
 ************************************************************/
function show_field_error($field, message)
{
    $field.querySelectorAll('input, textarea, select').forEach(el => {
        el.classList.add('is-danger');
    });
    let $help = $field.querySelector('.help');
    if($help) {
        $help.textContent = message;
        $help.style.display = 'block';
    }
}

/************************************************************
 *  Clear a validation error from a field
 ************************************************************/
function clear_field_error($field)
{
    $field.querySelectorAll('input, textarea, select').forEach(el => {
        el.classList.remove('is-danger');
    });
    let $help = $field.querySelector('.help');
    if($help) {
        $help.textContent = '';
        $help.style.display = 'none';
    }
}

/************************************************************
 *  Get the current value of a field for validation
 ************************************************************/
function get_field_value_for_validation($field, conf)
{
    switch(conf.tag) {
        case 'input': {
            let $input = $field.querySelector('input');
            if(!$input) {
                return '';
            }
            if($input.type === 'checkbox') {
                return $input.checked;
            }
            return $input.value;
        }
        case 'checkbox': {
            let $input = $field.querySelector('input[type="checkbox"]');
            if(!$input) {
                return false;
            }
            return $input.checked;
        }
        case 'textarea': {
            let $el = $field.querySelector('textarea');
            if(!$el) {
                return '';
            }
            return $el.value;
        }
        case 'select':
        case 'select2': {
            let $el = $field.querySelector('select');
            if(!$el) {
                return '';
            }
            if($el.tomselect) {
                return $el.tomselect.getValue();
            }
            return $el.value;
        }
        case 'jsoneditor': {
            let $el = $field.querySelector('.jsoneditor');
            if(!$el || !$el.jsoneditor) {
                return '';
            }
            let val = $el.jsoneditor.get();
            if(val.text !== undefined) {
                return val.text;
            }
            return JSON.stringify(val.json);
        }
        case 'radio': {
            let $checked = $field.querySelector('input[type="radio"]:checked');
            if(!$checked) {
                return '';
            }
            return $checked.value;
        }
    }
    return '';
}

/************************************************************
 *  Check if a field value is empty
 ************************************************************/
function is_field_value_empty(value)
{
    if(value === null || value === undefined || value === '') {
        return true;
    }
    if(Array.isArray(value) && value.length === 0) {
        return true;
    }
    return false;
}

/************************************************************
 *  Determine if a field requires validation in the current mode
 ************************************************************/
function is_field_required(conf, mode)
{
    if(conf.is_pkey) {
        // pkey is required only in create mode (unless it's a rowid)
        return (mode === "create" && !conf.is_rowid);
    }
    if(conf.readonly) {
        return false;
    }
    return (conf.required || conf.not_null);
}

/************************************************************
 *  Validate a single field on blur
 *  Called from input/textarea blur event handlers
 ************************************************************/
function validate_field_on_blur(element)
{
    let $field = element.closest('.xfield');
    if(!$field || !$field.field_conf) {
        return;
    }
    let conf = $field.field_conf;
    let $form = element.closest('form');
    let mode = $form ? $form.__mode__ : 'update';

    if(!is_field_required(conf, mode)) {
        clear_field_error($field);
        return;
    }

    let value = get_field_value_for_validation($field, conf);
    if(is_field_value_empty(value)) {
        show_field_error($field, t('this field is required'));
    } else {
        clear_field_error($field);
    }
}

/************************************************************
 *  Validate all required fields in the form
 *  Returns true if valid, false otherwise
 ************************************************************/
function validate_form(gobj, $form)
{
    let is_valid = true;
    let first_invalid = null;
    let mode = $form.__mode__ || 'update';

    $form.querySelectorAll('.xfield').forEach($field => {
        let conf = $field.field_conf;
        if(!conf) {
            return;
        }

        if(!is_field_required(conf, mode)) {
            clear_field_error($field);
            return;
        }

        let value = get_field_value_for_validation($field, conf);
        if(is_field_value_empty(value)) {
            show_field_error($field, t('this field is required'));
            is_valid = false;
            if(!first_invalid) {
                first_invalid = $field;
            }
        } else {
            clear_field_error($field);
        }
    });

    if(first_invalid) {
        let $focusable = first_invalid.querySelector('input, select, textarea');
        if($focusable && $focusable.focus) {
            $focusable.focus();
        }
    }

    return is_valid;
}

/************************************************************
 *  Clear all validation errors from the form
 ************************************************************/
function clear_validation(gobj, $form)
{
    $form.querySelectorAll('.xfield').forEach($field => {
        clear_field_error($field);
    });
}

/************************************************************
 *  Before show edit/create from,
 *      set form state "update" or "create"
 *  Only the "id" field is modified,
 *      in "update" is readonly
 *      in "create" is writable
 *  Although by default the pkey (primary key) is "id",
 *      use always desc.pkey value, who knows all cases!
 ************************************************************/
function set_form_mode(gobj, $form, mode, index)
{
    let desc = gobj_read_attr(gobj, "desc");
    let col_pkey = kwid_find_one_record(
        gobj,
        desc.cols,  // kw
        null,               // ids
        {                   // jn_filter
            "id": desc.pkey
        }
    );
    let is_rowid = str_in_list(col_pkey.flag, "rowid");

    let $id = $form.querySelector(`[name="${desc.pkey}"]`);

    if($id) {
        if(mode === "update") {
            /*
             *  update mode
             */
            $id.setAttribute('readonly', 'readonly');
            $id.removeAttribute('required');
        } else {
            /*
             *  create mode
             */
            if(is_rowid) {
                $id.setAttribute('readonly', 'readonly');
                $id.removeAttribute('required');
            } else {
                $id.removeAttribute('readonly');
                $id.setAttribute('required', '');
            }
        }
    } else {
        log_error(sprintf("%s: form without id field ('%s')",
            gobj_short_name(gobj),
            desc.pkey
        ));
    }

    if(is_rowid) {
        mode = "create";
    }

    $form.__mode__ = mode;
    $form.__index__ = index;
}

/************************************************************
 *  Show the edit "update" form of a row (record)
 *  internally called from the icon edit at Op column
 ************************************************************/
function show_edit_form(gobj, row, index)
{
    let popup_id = gobj_read_str_attr(gobj, "popup_id");
    let form_id = gobj_read_str_attr(gobj, "form_id");

    let $element = document.getElementById(popup_id);
    if(!$element) {
        $element = build_topic_modal(gobj);
    }

    let $form = document.getElementById(form_id);
    clear_validation(gobj,$form);

    if(json_size(row) === 0 ||  // WARNING _check_box_state_ widely used
        json_size(row) === 1 && ('_check_box_state_' in row)) {
        /*
         *  WARNING it won't pass by here,
         *  "create" is call directly from Top New button
         */
        set_form_mode(gobj, $form, "create", index);
    } else {
        set_form_mode(gobj, $form, "update", index);
    }

    set_form_values(gobj, $form, row);

    $element.classList.add('is-active');
    let $with_focus = $element.querySelector('.with-focus');
    if($with_focus) {
        $with_focus.focus();
    }
}

/************************************************************
 *  Show the edit "create" form to a new record
 *  internally called from the top toolbar +New button
 ************************************************************/
function show_create_form(gobj, row)
{
    let popup_id = gobj_read_str_attr(gobj, "popup_id");
    let form_id = gobj_read_str_attr(gobj, "form_id");

    let $element = document.getElementById(popup_id);
    if(!$element) {
        $element = build_topic_modal(gobj);
    }

    let $form = document.getElementById(form_id);
    clear_validation(gobj,$form);
    set_form_mode(gobj, $form, "create", -1);
    set_form_values(gobj, $form, row);

    $element.classList.add('is-active');
    let $with_focus = $element.querySelector('.with-focus');
    if($with_focus) {
        $with_focus.focus();
    }
}

/************************************************************
 *  Return column schema of field id
 ************************************************************/
function get_schema_col(gobj, id)
{
    let desc = gobj_read_attr(gobj, "desc");
    for (let i = 0; i < desc.cols.length; i++) {
        let col = desc.cols[i];
        if(col.id === id) {
            return col;
        }
    }
    return null;
}

/************************************************************
 *
 ************************************************************/
function clear_data(gobj, $form)
{
    $form.querySelectorAll('input, select, textarea, .jsoneditor').forEach(input => {
        if(input.name !== 'id') {
            if (input.classList.contains('jsoneditor')) {
                let jsoneditor = input.jsoneditor;
                if(jsoneditor) {
                    jsoneditor.set({text: ''});
                }
            } else if (input.type === 'checkbox') {
                input.checked = false;
            } else if (input.type === 'radio') {
                input.checked = false;

            } else if (input.type === 'datetime-local') { /* time */
                input.value = null;

            } else if (input.multiple) {
                if(input.tomselect) {
                    input.tomselect.clear();
                }
            } else {
                input.value = null;
            }
        }
    });
}

/************************************************************
 *
 ************************************************************/
function get_form_values($form)
{
    const values = {};
    // Process all form elements
    $form.querySelectorAll('input, select, textarea, .jsoneditor').forEach(input => {
        if (input.classList.contains('jsoneditor')) {
            let jsoneditor = input.jsoneditor;
            if(jsoneditor) {
                let name = input.getAttribute('name');
                let value = jsoneditor.get();
                if(value.text===undefined) {
                    values[name] = JSON.stringify(value.json, null, null);
                } else {
                    values[name] = value.text;
                }
            }
        } else if (input.type === 'checkbox') {
            // Handle single checkbox with true/false values
            values[input.name] = input.checked;
        } else if (input.type === 'radio') {
            // Handle radio buttons
            if (input.checked) {
                values[input.name] = input.value;
            }

        } else if (input.type === 'datetime-local') { /* time */
            // Create a new Date object from the datetime-local input value
            const date = new Date(input.value);

            // Convert the Date object to epoch time (in seconds)
            const epochTime = Math.floor(date.getTime() / 1000);
            values[input.name] = epochTime;

        } else if (input.multiple) {
            // Handle multi-select with Select2
            // values[input.name] = $(input).val();

            // Handle multi-select with Tom-Select
            if (input.tomselect) {
                values[input.name] = input.tomselect.getValue(); // Returns an array of selected values
            }
        } else {
            // Handle other input types (text, email, number, select, textarea)
            values[input.name] = input.value;
        }
    });

    return values;
}

/************************************************************
 *  Convert a record column value from backend to frontend
 ************************************************************/
function epochToDateTimeLocal(epochTime)
{
    const date = new Date(epochTime * 1000); // Convert to milliseconds
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    const seconds = String(date.getSeconds()).padStart(2, '0');

    // TODO for 'datetime-local' input, there is no seconds! we must use another library
    return `${year}-${month}-${day}T${hours}:${minutes}`;
}

/************************************************************
 *
 ************************************************************/
function set_form_values(gobj, form, values)
{
    Object.keys(values).forEach(key => {
        let element = form.querySelector(`[name="${key}"]`);
        if (element) {
            let value = values[key];
            let col = get_schema_col(gobj, key);
            if(col) {
                value = transform__treedb_value_2_form_value(gobj, col, value);
            }

            switch (element.type) {
                case 'select':
                    element.value = value;
                    break;

                case 'select-one':
                    element.value = value;
                    break;

                case 'select-multiple':
                    // Handle multi-select with Select2
                    //$(element).val(value).trigger('change');

                    // Handle multi-select with Tom-Select
                    if (element.tomselect) {
                        element.tomselect.setValue(value);
                    }
                    break;

                case 'radio':
                    // TODO review
                    let radio = form.querySelector(`input[name="${key}"][value="${values[key]}"]`);
                    if (radio) {
                        radio.checked = true;
                    }
                    break;

                case 'checkbox':
                    element.checked = value;  // Assume the value is true or false
                    break;

                case 'text':
                case 'textarea':
                case 'email':
                    element.value = value;
                    break;

                case 'datetime-local': /* time */
                    element.value = epochToDateTimeLocal(value);
                    break;

                default:
                    if(element.classList.contains('jsoneditor')) {
                        let jsoneditor = element.jsoneditor;
                        if(jsoneditor) {
                            jsoneditor.set(value);
                        }
                        break;
                    }

                    element.value = value;
                    log_error(
                        sprintf("%s: form type unknown: '%s'",
                            gobj_short_name(gobj),
                            element.type
                        )
                    );
                    break;
            }
        }
    });
}

/************************************************************
 *  Convert a record column value from backend
 *      to frontend form field
 ************************************************************/
function transform__treedb_value_2_form_value(gobj, col, value)
{
    const field_desc = treedb_get_field_desc(col);

    switch(field_desc.type) {
        case "string":
            break;
        case "integer":
            break;
        case "real":
            break;
        case "boolean":
            break;

        case "object":
        case "dict":
        case "template":
        case "array":
        case "list":
        case "coordinates":
        case "blob":
            // Return as it is, manage by jsoneditor
            value = {json: value};
            // value = {text: JSON.stringify(value)}; // other way
            break;

        case "enum":
            switch(field_desc.real_type) {
                case "string":
                    break;
                case "object":
                case "dict":
                case "array":
                case "list":
                    break;
            }
            break;

        case "now":
        case "time":
            switch(field_desc.real_type) {
                case "string":
                case "integer":
                    value = parseInt(value) || 0;
                    value = new Date(value);
                    break;
            }
            break;

        case "color":
            switch(field_desc.real_type) {
                case "string":
                    // TODO
                    break;
                case "integer":
                    // TODO
                    break;
            }
            break;

        case "hook":    // Convert data from backend to frontend FORM FIELD
            // WARNING hook fields are not writable and by now they are not showed in forms.
            break;

        case "fkey":    // Convert data from backend to frontend FORM FIELD
            let new_value = [];
            if(value) {
                if(is_string(value)) {
                    let fkey = treedb_decoder_fkey(col, value);
                    if(fkey) {
                        new_value.push(fkey.id);
                    }
                } else if(is_array(value)) {
                    for(let i=0; i<value.length; i++) {
                        let fkey = treedb_decoder_fkey(col, value[i]);
                        if(fkey) {
                            new_value.push(fkey.id);
                        }
                    }
                } else {
                    log_error("fkey type unsupported: " + JSON.stringify(value));
                }
            }

            value = new_value;
            break;
    }

    return value;
}

/************************************************************
 *  Convert from frontend to backend
 *  operation: "create" "update"
 ************************************************************/
function transform__form_record_2_treedb_record(gobj, kw, operation)
{
    let row = {};

    for(let field_name of Object.keys(kw)) {
        if(empty_string(field_name)) {
            continue;
        }
        let col = get_schema_col(gobj, field_name);
        if(col) {
            let value = kw[field_name];
            row[field_name] = transform__form_value_2_treedb_value(
                gobj, col, value, operation
            );
        }
    }
    return row;
}

/************************************************************
 *  Convert from frontend to backend
 *  operation: "create" "update"
 ************************************************************/
function transform__form_value_2_treedb_value(gobj, col, value, operation)
{
    const field_desc = treedb_get_field_desc(col);

    switch(field_desc.type) {
        case "rowid":
            if(operation==="create") {
                value = "";
            }
            break;

        case "string":
        case "email":
        case "password":
        case "url":
        case "image":
        case "tel":
            break;

        case "integer":
            value = parseInt(value) || 0;
            break;
        case "real":
            value = parseFloat(value)  || 0.0;
            break;
        case "boolean":
            value = parseBoolean(value);
            break;

        case "hook":    // Convert data from frontend to backend
            value = null;
            break;

        case "fkey":    // Convert data from frontend to backend
            value = build_fkey_ref(gobj, col, value);
            break;

        case "object":
        case "dict":
        case "template":
            if(is_string(value)) {
                // Come from the form
                try {
                    value = JSON.parse(value);
                } catch (e) {
                    value = {};
                }
            } else if(is_object(value)) {
                // Come from the table
            }
            if(!is_object(value)) {
                value = {};
            }
            break;
        case "array":
        case "list":
            if(is_string(value)) {
                // Come from the form
                try {
                    value = JSON.parse(value);
                } catch (e) {
                    value = [];
                }
            } else if(is_array(value)) {
                // Come from the table
            }
            if(!is_array(value)) {
                value = [];
            }
            break;
        case "coordinates":
        case "blob":
            if(is_string(value)) {
                // Come from the form
                try {
                    value = JSON.parse(value);
                } catch (e) {
                    value = {};
                }

            } else if(is_object(value)) {
                // Come from the table
            } else if(is_array(value)) {
                // Come from the table
            } else {
                value = {};
            }
            break;

        case "enum":
            switch(field_desc.real_type) {
                case "string":
                    if(is_array(value)) {
                        if(value.length > 0) {
                            value = value[0];
                        } else {
                            value = "";
                        }
                    }
                    break;
                case "object":
                case "dict":
                case "array":
                case "list":
                    break;
                default:
                    log_error("col type unknown 6: " + field_desc.real_type);
                    break;
            }
            break;

        case "now":
        case "time":
            switch(field_desc.real_type) {
                case "string":
                    if(value && is_date(value)) {
                        value = value.toISOString();
                    }
                    break;
                case "integer":
                    if(value && is_date(value)) {
                        value = (value.getTime())/1000;
                    }
                    break;
                default:
                    log_error("col type unknown 7: " + field_desc.real_type);
                    break;
            }
            break;

        case "color":
            switch(field_desc.real_type) {
                case "string":
                    // TODO
                    break;
                case "integer":
                    // TODO
                    break;
                default:
                    log_error("col type unknown 7: " + field_desc.real_type);
                    break;
            }
            break;

        default:
            log_error("col type unknown 8: " + field_desc.type);
            break;
    }

    return value;
}

/************************************************************
 *
 ************************************************************/
function get_fkey_options(gobj, col)
{
    for(let topic_name of Object.keys(col.fkey)) {
        // HACK we work only with one fkey
        let kw = {
            topic_name: topic_name
        };
        let webix = gobj_command(gobj_parent(gobj), "get_topic_data", kw, gobj);
        let current_list = webix.data;
        return list2options(current_list, "id", "id");
    }
    log_error("No fkey options found" + JSON.stringify(col));
    return null;
}

/************************************************************
 *
 ************************************************************/
function fkey_in_col(col, topic_name, hook_name)
{
    let hook_name_ = col.fkey[topic_name];

    if(hook_name === hook_name_) {
        return true;
    }
    return false;
}

/************************************************************
 *
 ************************************************************/
function build_fkey_ref(gobj, col, value)
{
    let refs = null;

    switch(col.type) {
        case "string":
            break;

        case "object":
        case "dict":
            refs = {};
            break;

        case "array":
        case "list":
            refs = [];
            break;

        default:
            log_error("Merde type: " + col.type);
            return null;
    }

    if(is_array(value)) {
        for(let i=0; i<value.length; i++) {
            let v = value[i];
            if(is_string(v)) {
                // HACK we work only with one fkey
                let topic_name = Object.keys(col.fkey)[0]; // Get the first key
                let hook = col.fkey[topic_name];
                switch(col.type) {
                    case "string":
                        return topic_name + "^" + v + "^" + hook;

                    case "object":
                    case "dict":
                        refs[topic_name + "^" + v + "^" + hook] = true;
                        break;

                    case "array":
                    case "list":
                        refs.push(topic_name + "^" + v + "^" + hook);
                        break;
                }

            } else if(is_object(v)) {
                let topic_name = v.topic_name;
                let hook = v.hook_name;
                if(!fkey_in_col(col, topic_name, hook)) {
                    continue;
                }
                switch(col.type) {
                    case "string":
                        return topic_name + "^" + v.id + "^" + hook;

                    case "object":
                    case "dict":
                        refs[topic_name + "^" + v.id + "^" + hook] = true;
                        break;

                    case "array":
                    case "list":
                        refs.push(topic_name + "^" + v.id + "^" + hook);
                        break;
                }
            } else {
                log_error("Merde value1: " + v);
                return null;
            }
        }
    } else {
        log_error("Merde value2: " + value);
        return null;
    }

    return refs;
}

/***************************************************************************
 *  TODO con límite máximo o máximo height o con scroll
 *      en un gobj propio para gestionar los datos en "page"s
 *      que los hook no vengan rellenos si son muchos y que se puedan gestionar
 *      con un gobj
 ***************************************************************************/
function show_dropdown_popup_menu(gobj, x, y, items, callback)
{
    let $element = createElement2([
        'div', {class: 'dropdown popup' }, [
            ['div', {
                    class: 'dropdown-menu', role: 'menu', style: 'min-width:4rem; border: 2px solid var(--bulma-border); padding: 0px;'
                }, [
                ['div', { class: 'dropdown-content', style: 'padding: 0;' }, []]
            ]]
        ], {
            'click': (evt) => {
                evt.stopPropagation();
                destroyModal();
                if(callback) {
                    callback(evt);
                }
            }
        }
    ]);

    const destroyModal = () => {
        $element.classList.remove('is-active');
        $element.parentNode.removeChild($element);
    };

    let $dropdown_content = $element.querySelector('.dropdown-content');

    let ids = kwid_get_ids(gobj, items);
    for(let id of ids) {
        let $item = createElement2(
            ['a', {class: 'dropdown-item flex-horizontal-section', 'data-value':`${id}`, style:'margin:0px;'}, [
                ['span', {i18n: `${id}`}, `${id}`]
            ], {
                'click': (evt) => {
                    evt.stopPropagation();
                    destroyModal();
                    if(callback) {
                        callback(evt, this.dataset.value);
                    }
                }
            }]
        );
        $dropdown_content.appendChild($item);
    }

    refresh_language($element, t);

    /*
     *  Add to popup layer
     */
    document.getElementById("popup-layer").appendChild($element);

    /*
     *  Set position
     */
    $element.style.position = "absolute";
    $element.style.top = y + "px";
    $element.style.left = x + "px";

    /*
     *  Show
     */
    $element.classList.add('is-active');

    /*
     *  Set focus
     */
    let $with_focus = $element.querySelector('.with-focus');
    if($with_focus) {
        $with_focus.focus();
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *  From external, at the beginning, load all topic data
 ************************************************************/
function ac_load_nodes(gobj, event, kw, src)
{
    let data = kw;
    if(!is_array(data)) {
        log_error("ac_load_nodes(): FormTable, data MUST be an array");
        trace_msg(data);
        return -1;
    }

    let tabulator = gobj_read_attr(gobj, "tabulator");
    if(tabulator) {
        /*
         *  setData: Load data into the table. The old rows will be removed.
         *  Guard: defer until tableBuilt fires if Tabulator isn't ready yet.
         */
        if(tabulator._ready) {
            tabulator.setData(data);
        } else {
            tabulator._pendingData = data;
        }
    }

    /*
     *  TODO situate en el row updated ???
     *  Select only if it has update/create mode
     */
    // if(data.length == 1) {
    //     if(!with_webix_id) {
    //         gobj_send_event(gobj, "EV_RECORD_BY_ID", {id:data[0].id}, gobj);
    //     } else {
    //         gobj_send_event(gobj, "EV_FIRST_RECORD", {}, gobj);
    //     }
    // } else if(data.length > 1) {
    //     if(last_selected_id) {
    //         gobj_send_event(gobj, "EV_RECORD_BY_ID", {id:last_selected_id.id}, gobj);
    //     } else {
    //         gobj_send_event(gobj, "EV_FIRST_RECORD", {}, gobj);
    //     }
    // }

    return 0;
}

/************************************************************
 *  From external, load node created
 ************************************************************/
function ac_load_node_created(gobj, event, kw, src)
{
    let data = kw;
    if(!is_array(data)) {
        log_error("ac_load_node_created(): FormTable, data MUST be an array");
        trace_msg(data);
        return -1;
    }

    let tabulator = gobj_read_attr(gobj, "tabulator");
    if(tabulator) {
        for(let record of data) {
            tabulator.addData([record]); // Add a new row to table
        }
    }

    /*
     *  TODO situate en el row updated ???
     *  Select only if it has update/create mode
     */

    return 0;
}

/************************************************************
 *  From external, load node updated
 ************************************************************/
function ac_load_node_updated(gobj, event, kw, src)
{
    let data = kw;
    if(!is_array(data)) {
        log_error("ac_load_node_updated(): FormTable, data MUST be an array");
        trace_msg(data);
        return -1;
    }

    let tabulator = gobj_read_attr(gobj, "tabulator");
    if(tabulator) {
        for(let record of data) {
            tabulator.updateData([record]);
        }
    }

    /*
     *  TODO situate en el row updated ???
     *  Select only if it has update/create mode
     */

    return 0;
}

/************************************************************
 *  From external
 ************************************************************/
function ac_node_deleted(gobj, event, kw, src)
{
    let data = kw;
    if(!is_array(data)) {
        log_error("ac_node_deleted(): FormTable, data MUST be an array");
        trace_msg(data);
        return -1;
    }

    let tabulator = gobj_read_attr(gobj, "tabulator");
    //tabulator.deselectRow(); // unselectAll TODO ??? is necessary?

    if(tabulator) {
        for(let record of data) {
            const row = tabulator.getRow(record.id);
            if(row) {
                // Delete the row by pkey value
                tabulator.deleteRow(record.id);
            } else {
                log_error("delete_data: record not found: " + record.id);
            }
        }
    }

    return 0;
}

/************************************************************
 *  General Edit button clicked:
 *  Set/Reset edition mode
 *      In edition mode the general New/Delete buttons are activated,
 *      and the Op column is visible (with row's edit/delete icons)
 ************************************************************/
function ac_edition_mode(gobj, event, kw, src)
{
    let tabulator = gobj_read_attr(gobj, "tabulator");

    /*
     *  Set button states according to editable state
     */
    let editable = gobj_read_bool_attr(gobj, "editable");
    editable = !editable;
    gobj_write_bool_attr(gobj, "editable", editable);

    let $container = gobj_read_attr(gobj, "$container");
    let $button_edit_record = $container.querySelector(`.button-edit-record`);
    let $button_new_record = $container.querySelector(`.button-new-record`);
    let $button_delete_record = $container.querySelector(`.button-delete-record`);
    let $button_copy_record = $container.querySelector(`.button-copy-record`);
    let $button_paste_record = $container.querySelector(`.button-paste-record`);

    if(editable) {
        /*
         *  Set edition mode
         */
        $button_edit_record.classList.add('is-primary');
        $button_new_record.classList.add('is-info');
        $button_delete_record.classList.add('is-danger');

        tabulator.showColumn('_operation');
        tabulator.showColumn('_check_box_state_');

        $button_new_record.removeAttribute("disabled");
        $button_paste_record.removeAttribute("disabled");

        let rows = tabulator.getSelectedData();
        if (rows.length) {
            $button_delete_record.removeAttribute("disabled");
            $button_copy_record.removeAttribute("disabled");
        }

    } else {
        /*
         *  Remove edition mode
         */
        $button_edit_record.classList.remove('is-primary');
        $button_new_record.classList.remove('is-info');
        $button_delete_record.classList.remove('is-danger');

        tabulator.hideColumn('_operation');
        tabulator.hideColumn('_check_box_state_');

        $button_new_record.setAttribute("disabled", true);
        $button_delete_record.setAttribute("disabled", true);
        $button_copy_record.setAttribute("disabled", true);
        $button_paste_record.setAttribute("disabled", true);
    }

    return 0;
}

/************************************************************
 *  From internal
 *  From general top toolbar "New" button
 *  FLow:
 *      - the form will create a new record and save it to the backend,
 *      - the backend will broadcast the new record that will be added to table
 ************************************************************/
function ac_new_row(gobj, event, kw, src)
{
    /*
     *  Build default values. TODO no debería estar en desc configuration?
     */
    let row = {};
    let desc = gobj_read_attr(gobj, "desc");
    for (let i = 0; i < desc.cols.length; i++) {
        let col = desc.cols[i];
        if(!col.id || col.id[0]==='_') {
            continue;
        }

        const field_desc = treedb_get_field_desc(col);
        switch(field_desc.type) {
            case "now":
                row[col.id] =  Math.floor(Date.now() / 1000);
                break;
            case "template":
                if(col.template) {
                    row[col.id] = col.template;
                }
                break;
            default:
                if(field_desc.default_value !== undefined) {
                    row[col.id] = field_desc.default_value;
                }
                break;
        }
    }

    show_create_form(gobj, row);

    return 0;
}

/************************************************************
 *  From internal
 *  - From the general top toolbar "Delete" button
 *      It will use the selected rows to delete them
 *          kw {} empty
 *  - From the column _operation delete icon inside a row
 *      It will delete this one row
 *          kw {index:, row:}
 ************************************************************/
function ac_delete_rows(gobj, event, kw, src)
{
    let tabulator = gobj_read_attr(gobj, "tabulator");

    if(json_size(kw) === 0) {
        /*----------------------------*
         *  Delete selected rows
         *----------------------------*/
        let rows = tabulator.getSelectedData();
        if (!rows.length) {
            get_ok(t('please select some row'));
            return 0;
        }

        get_yesnocancel(t('are you sure'), function(evt) {
            if(evt === "yes") {
                for(let row of rows) {
                    // TODO why don't send once EV_DELETE_RECORD(S)
                    gobj_publish_event(
                        gobj,
                        "EV_DELETE_RECORD",
                        {
                            topic_name: gobj_read_str_attr(gobj, "topic_name"),
                            record: row
                        }
                    );
                }
            }
        });

    } else {
        /*----------------------------*
         *  Delete one row
         *  {index: , row: }
         *----------------------------*/
        get_yesnocancel(t('are you sure'), function(evt) {
            if(evt === "yes") {
                gobj_publish_event(
                    gobj,
                    "EV_DELETE_RECORD",
                    {
                        topic_name: gobj_read_str_attr(gobj, "topic_name"),
                        record: kw.row
                    }
                );
            }
        });
    }

    return 0;
}

/************************************************************
 *  From internal
 ************************************************************/
function ac_copy_rows(gobj, event, kw, src)
{
    let tabulator = gobj_read_attr(gobj, "tabulator");

    /*----------------------------*
     *  Copy selected rows
     *----------------------------*/
    let rows = tabulator.getSelectedData();
    if (!rows.length) {
        get_ok(t('please select some row'));
        return 0;
    }

    let copy_rows = [];
    for(let row of rows) {
        let new_row = transform__form_record_2_treedb_record(gobj, row, "create");
        copy_rows.push(new_row);
    }

    let data = {
        treedb_name: gobj_read_str_attr(gobj, "treedb_name"),
        topic_name: gobj_read_str_attr(gobj, "topic_name"),
        rows: copy_rows
    };
    navigator.clipboard.writeText(JSON.stringify(data));

    return 0;
}

/************************************************************
 *  From internal
 ************************************************************/
function ac_paste_rows(gobj, event, kw, src)
{
    /*----------------------------*
     *  Paste rows
     *----------------------------*/
    if(kw.topic_name === gobj_read_str_attr(gobj, "topic_name") &&
            kw.treedb_name === gobj_read_str_attr(gobj, "treedb_name")) {
        for(let row of kw.rows) {
            gobj_publish_event(
                gobj,
                "EV_CREATE_RECORD",
                {
                    topic_name: gobj_read_str_attr(gobj, "topic_name"),
                    record: row
                }
            );
        }
    }

    return 0;
}

/************************************************************
 *  From internal Save button of "create" modal form
 ************************************************************/
function ac_create_record(gobj, event, kw, src)
{
    let new_kw = null;
    try {
        new_kw = transform__form_record_2_treedb_record(gobj, kw, "create");
    } catch (e) {
        log_warning(e);
        throw e;
    }

    gobj_publish_event(
        gobj,
        "EV_CREATE_RECORD",
        {
            topic_name: gobj_read_str_attr(gobj, "topic_name"),
            record: new_kw
        }
    );

    /*
     *  Return -1 will not close the form
     */
    return 0;
}

/************************************************************
 *  From internal Save button of "update" modal form
 ************************************************************/
function ac_update_record(gobj, event, kw, src)
{
    let new_kw = null;
    try {
        new_kw = transform__form_record_2_treedb_record(gobj, kw, "update");
    } catch (e) {
        log_warning(e);
        throw e;
    }

    gobj_publish_event(
        gobj,
        "EV_UPDATE_RECORD",
        {
            topic_name: gobj_read_str_attr(gobj, "topic_name"),
            record: new_kw
        }
    );

    /*
     *  Return -1 will not close the form
     */
    return 0;
}

/************************************************************
 *  { rows: [row] }
 ************************************************************/
function ac_select_rows(gobj, event, kw, src)
{
    let tabulator = gobj_read_attr(gobj, "tabulator");
    let $container = gobj_read_attr(gobj, "$container");

    /*
     *  Update button DELETE, only enable if some row is selected
     */
    let $button_delete_record = $container.querySelector(`.button-delete-record`);
    let $button_copy_record = $container.querySelector(`.button-copy-record`);
    if($button_delete_record) {
        let selectedRows = tabulator.getSelectedData();
        if (selectedRows.length && gobj_read_bool_attr(gobj, "editable")) {
            $button_delete_record.removeAttribute("disabled");
            $button_copy_record.removeAttribute("disabled");
        } else {
            $button_delete_record.setAttribute("disabled", true);
            $button_copy_record.setAttribute("disabled", true);
        }
    }

    if(gobj_read_bool_attr(gobj, "broadcast_select_rows_event")) {
        gobj_publish_event(gobj, event, kw);
    }

    return 0;
}

/************************************************************
 *  { rows: [row] }
 ************************************************************/
function ac_unselect_rows(gobj, event, kw, src)
{
    let tabulator = gobj_read_attr(gobj, "tabulator");
    let $container = gobj_read_attr(gobj, "$container");

    /*
     *  Update button DELETE, only enable if some row is selected
     */
    let $button_delete_record = $container.querySelector(`.button-delete-record`);
    let $button_copy_record = $container.querySelector(`.button-copy-record`);
    if($button_delete_record) {
        let selectedRows = tabulator.getSelectedData();
        if (selectedRows.length && gobj_read_bool_attr(gobj, "editable")) {
            $button_delete_record.removeAttribute("disabled");
            $button_copy_record.removeAttribute("disabled");
        } else {
            $button_delete_record.setAttribute("disabled", true);
            $button_copy_record.setAttribute("disabled", true);
        }
    }

    // WARNING with radio, there is no unselect event.
    if(gobj_read_bool_attr(gobj, "broadcast_select_rows_event")) {
        gobj_publish_event(gobj, event, kw);
    }

    return 0;
}

/************************************************************
 *  { locale: "es" }
 ************************************************************/
function ac_change_locale(gobj, event, kw, src)
{
    return 0;
}

/************************************************************
 *   let kw_hook = {
 *      treedb_name:
 *      topic_name:
 *      row_id:
 *      col_id:
 *      click_x:
 *      click_y:
 *  };
 ************************************************************/
function ac_show_hook_data(gobj, event, kw, src)
{
    let webix = gobj_command(gobj_parent(gobj), "get_topic_data", kw, gobj);

    let row = kwid_find_one_record(gobj, webix.data, kw.row_id, null);
    if(row) {
        let cell = row[kw.col_id];
        /*
         *  WARNING TODO hooks can have millions of kids
         */
        show_dropdown_popup_menu(gobj, kw.click_x, kw.click_y, cell);
    }

    return 0;
}

/************************************************************
 *
 ************************************************************/
function ac_refresh(gobj, event, kw, src)
{
    gobj_publish_event(
        gobj,
        "EV_REFRESH_TOPIC",
        {
            topic_name: gobj_read_str_attr(gobj, "topic_name")
        }
    );

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
    mt_create:          mt_create,
    mt_start:           mt_start,
    mt_stop:            mt_stop,
    mt_destroy:         mt_destroy,
    mt_command_parser:  mt_command_parser,
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
            ["EV_LOAD_NODES",           ac_load_nodes,         null],
            ["EV_LOAD_NODE_CREATED",    ac_load_node_created,  null],
            ["EV_LOAD_NODE_UPDATED",    ac_load_node_updated,  null],
            ["EV_NODE_DELETED",         ac_node_deleted,       null],

            ["EV_EDITION_MODE",         ac_edition_mode,       null],
            ["EV_CREATE_RECORD",        ac_create_record,      null],
            ["EV_UPDATE_RECORD",        ac_update_record,      null],

            ["EV_NEW_ROW",              ac_new_row,            null],
            ["EV_DELETE_ROWS",          ac_delete_rows,        null],
            ["EV_SELECT_ROWS",          ac_select_rows,        null],
            ["EV_UNSELECT_ROWS",        ac_unselect_rows,      null],
            ["EV_COPY_ROWS",            ac_copy_rows,          null],
            ["EV_PASTE_ROWS",           ac_paste_rows,         null],
            ["EV_SHOW_HOOK_DATA",       ac_show_hook_data,     null],
            ["EV_CHANGE_LOCALE",        ac_change_locale,      null],
            ["EV_REFRESH",              ac_refresh,            null],
            ["EV_SHOW",                 ac_show,               null],
            ["EV_HIDE",                 ac_hide,               null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_LOAD_NODES",           0],
        ["EV_LOAD_NODE_CREATED",    0],
        ["EV_LOAD_NODE_UPDATED",    0],
        ["EV_NODE_DELETED",         0],

        ["EV_EDITION_MODE",         0],
        ["EV_NEW_ROW",              0],
        ["EV_DELETE_ROWS",          0],
        ["EV_COPY_ROWS",            0],
        ["EV_PASTE_ROWS",           0],
        ["EV_SELECT_ROWS",          event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_UNSELECT_ROWS",        event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_SHOW_HOOK_DATA",       event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_CHANGE_LOCALE",        0],
        ["EV_REFRESH",              0],

        ["EV_CREATE_RECORD",        event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_UPDATE_RECORD",        event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_DELETE_RECORD",        event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_REFRESH_TOPIC",        event_flag_t.EVF_OUTPUT_EVENT],

        ["EV_SHOW",                 0],
        ["EV_HIDE",                 0]
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
function register_c_yui_treedb_topic_with_form()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_treedb_topic_with_form };
