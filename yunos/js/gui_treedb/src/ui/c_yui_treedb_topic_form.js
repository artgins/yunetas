/***********************************************************************
 *          c_yui_treedb_topic_form.js
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

import jQuery from "jquery";
import $ from "jquery";

import "tom-select/dist/css/tom-select.css"; // Import Tom-Select CSS
import TomSelect from "tom-select"; // Import Tom-Select JS

import "bootstrap-table/dist/bootstrap-table.css";
import "bootstrap-table/dist/bootstrap-table.js";
import "bootstrap-table/dist/themes/bulma/bootstrap-table-bulma.js";
import "bootstrap-table/dist/locale/bootstrap-table-es-ES.min.js";
import "bootstrap-table/dist/locale/bootstrap-table-en-US.min.js";

import "bootstrap-table/dist/extensions/fixed-columns/bootstrap-table-fixed-columns.css";
import "bootstrap-table/dist/extensions/fixed-columns/bootstrap-table-fixed-columns.js";
import "bootstrap-table/dist/extensions/custom-view/bootstrap-table-custom-view.js";
import "bootstrap-table/dist/extensions/export/bootstrap-table-export.js";

/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_TREEDB_TOPIC_FORM";

/***************************************************************
 *              Data
 ***************************************************************/
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
SDATA(data_type_t.DTP_BOOLEAN,  "with_paste_button",    0,  true,   "Button toolbar PASTE"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_in_row_edit_icons",0,  true,   "Add a last column with internal EDIT/DELETE icon"),

SDATA(data_type_t.DTP_BOOLEAN,  "editable",             0,  false,  "Edit state"),

/*---------------- Selection Mode ----------------*/
SDATA(data_type_t.DTP_BOOLEAN,  "with_checkbox",        0,  true,   "Auxiliary first column to select rows"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_radio",           0,  false,  "Auxiliary first column to select one row"),
SDATA(data_type_t.DTP_BOOLEAN,  "broadcast_select_rows_event",   0,  false, "Broadcast select rows event"),
SDATA(data_type_t.DTP_BOOLEAN,  "broadcast_unselect_rows_event", 0,  false, "Broadcast unselect rows event"),

/*---------------- Bootstrap Table Config ----------------*/
SDATA(data_type_t.DTP_DICT, "bootstrap_table_config", 0, {
    maintainMetaData: true,
    cardView: false,
    search: true,
    searchAccentNeutralise: false,
    searchHighlight: false,
    searchOnEnterKey: false,
    showButtonIcons: true,
    showButtonText: false,
    showColumns: false,
    showColumnsSearch: false,
    showColumnsToggleAll: true,
    showExtendedPagination: true,
    showFooter: true,
    showFullscreen: false,
    showHeader: true,
    showPaginationSwitch: true,
    showRefresh: true,
    showSearchButton: false,
    showSearchClearButton: false,
    showToggle: true,
    showExport: false,
    advancedSearch: false,
    fixedColumns: true,
    pagination: true,
    pageList: [12, 25, 50, 100, "all"],
    pageSize: 12,
    detailView: false,
    detailViewByClick: false,
    clickToSelect: false,
    multipleSelectRow: false,
    minimumCountColumns: 2,
    uniqueId: "id",
    icons: {
        paginationSwitchDown: 'fa-caret-square-down',
        paginationSwitchUp: 'fa-caret-square-up',
        refresh: 'fa-sync',
        toggleOff: 'fa-toggle-off',
        toggleOn: 'fa-toggle-on',
        columns: 'fa-th-list',
        fullscreen: 'fa-arrows-alt',
        detailOpen: 'fa-regular fa-eye',
        detailClose: 'fa-regular fa-eye-slash'
    }
}, "Bootstrap-table configuration"),

/*---------------- Internal Attributes ----------------*/
SDATA(data_type_t.DTP_POINTER,  "$container",           0,  null,   "HTML container for UI"),
SDATA(data_type_t.DTP_POINTER,  "$$table",              0,  null,   "Bootstrap-table instance"),
SDATA(data_type_t.DTP_STRING,   "table_id",             0,  null,   "Bootstrap-table ID"),
SDATA(data_type_t.DTP_STRING,   "toolbar_id",           0,  null,   "Toolbar of bootstrap-table"),
SDATA(data_type_t.DTP_STRING,   "form_id",              0,  null,   "Edit form ID"),
SDATA(data_type_t.DTP_STRING,   "popup_id",             0,  null,   "Edit form popup ID"),

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
    table__build(gobj);
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
    function create_bootstrap_toolbar()
    {
        // TODO pon autorización, solo si está autorizado a modificar los datos!!!
        // TODO deja que estos botones se queden en el top cuando se hace scroll (clip ?)
        let $bootstrap_toolbar = [];
        let with_edition_mode = gobj_read_bool_attr(gobj, "with_edition_mode");
        let with_new_button = gobj_read_bool_attr(gobj, "with_new_button");
        let with_delete_button = gobj_read_bool_attr(gobj, "with_delete_button");
        let with_copy_button = gobj_read_bool_attr(gobj, "with_copy_button");
        let with_paste_button = gobj_read_bool_attr(gobj, "with_paste_button");

        let toolbar_id = gobj_read_str_attr(gobj, "toolbar_id");

        if(with_edition_mode) {
            $bootstrap_toolbar = createElement2(
                ['div', {id: `${toolbar_id}`, class: 'buttons is-gapless'}]
            );
            let $edit_button = createElement2(
                ['button', {id: ``, class: 'button button-edit-record mr-1'}, [
                    ['i', {class: 'fa-solid fa-pen'}],
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
            $bootstrap_toolbar.appendChild($edit_button);

            if(with_new_button) {
                let $new_button = createElement2(
                    ['button', {id: ``, class: 'button button-new-record mr-1', disabled: true}, [
                        ['i', {class: 'fa-solid fa-plus'}],
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
                $bootstrap_toolbar.appendChild($new_button);
            }

            if(with_delete_button) {
                let $delete_button = createElement2(
                    ['button', {id: ``, class: 'button button-delete-record mr-1', disabled: true}, [
                        ['i', {class: 'fa fa-trash'}],
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
                $bootstrap_toolbar.appendChild($delete_button);
            }

            if(with_copy_button) {
                let $copy_button = createElement2(
                    ['button', {id: ``, class: 'button button-copy-record mr-1', disabled: true}, [
                        ['i', {class: 'fa fa-copy'}],
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
                $bootstrap_toolbar.appendChild($copy_button);
            }

            if(with_paste_button) {
                let $paste_button = createElement2(
                    ['button', {id: ``, class: 'button button-paste-record mr-1', disabled: true}, [
                        ['i', {class: 'fa fa-paste'}],
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
                $bootstrap_toolbar.appendChild($paste_button);
            }
        }
        return $bootstrap_toolbar;
    }

    let $bootstrap_toolbar = create_bootstrap_toolbar();

    /*----------------------------------------------*
     *  Layout Schema
     *----------------------------------------------*/
    let table_id = gobj_read_str_attr(gobj, "table_id");
    let $container = createElement2(
        // dataTables needs a wrapper ('div') over 'table'
        ['div', {class: 'container-treedb-topic-form', style: 'height:100%;'}, [
            $bootstrap_toolbar,
            ['table',
                {
                    id: `${table_id}`,
                    style: 'margin-top:0px !important;',
                }
            ]
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
 *   Build table with bootstrap-table.com
 *   This fn is called on start and when desc attribute is set.
 *   desc contains the description (columns) of table to create
 ******************************************************************/
function table__build(gobj)
{
    /*
     *  We work Only in inline mode.
     */

    /*
     *  Recover the <table> tag created in startup
     */
    let table_id = gobj_read_str_attr(gobj, "table_id");
    let $$table = jQuery(`#${table_id}`);
    gobj_write_attr(gobj, "$$table", $$table);

    let columns = [
    ];

    /*
     *  Add the checkbox column.
     */
    let with_checkbox = gobj_read_bool_attr(gobj, "with_checkbox");
    let with_radio = gobj_read_bool_attr(gobj, "with_radio");
    if(with_checkbox) {
        columns.push({
            checkbox: true,
            visible: false,
            field: '_check_box_state_'  // WARNING _check_box_state_ widely used
        });
    } else if(with_radio) {
        columns.push({
            radio: true,
            visible: false,
            field: '_check_box_state_'  // WARNING _check_box_state_ widely used
        });
    }

    /*
     *  Cell formatter, defined at column level,
     *  call on displaying to every cell
     *      index: nº de row en la tabla, propio de la tabla, base in 0
     *      row: nuestro registro
     *      field: name (id) of column
     */
    function formatter(value, row, index, field)
    {
        let col = get_schema_col(gobj, field);
        if(col) {
            return transform__treedb_value_2_table_value(gobj, col, value, row, field);
        }
        return "???";
    }

    let desc = gobj_read_attr(gobj, "desc");
    for (let i = 0; i < desc.cols.length; i++) {
        let col = desc.cols[i];
        if(!col.id || col.id[0]==='_') {
            continue;
        }

        let align;
        let events;
        const field_desc = treedb_get_field_desc(col);
        if(field_desc.is_hidden) {
            continue;
        }
        switch(field_desc.type) {
            case "hook":
                align = "center";
                events = {
                    'click .hook_cell': function (evt, value, row, index) {
                        evt.stopPropagation();
                        // Convert dataset to object
                        let data = datasetToObject(evt.currentTarget.dataset);
                        let kw_hook = {};
                        if(data && data.row_id) {
                            let pos = getPositionRelativeToBody(evt.currentTarget);

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
                    }
                };
                break;
            case "boolean":
                align = "center";
                break;
            case "integer":
            case "real":
                align = "right";
                break;
        }

        columns.push({
            title: col.header || col.id,
            field: col.id,
            sortable: true,
            align: align,
            formatter: formatter,
            events: events,
            alwaysUseFormatter: true
        });
    }

    /*
     *  Column with operators: delete,
     */
    function operateFormatter(value, row, index) {
        return [
            '<button class="button without-border px-2 edit">',
                '<i style="" class="fa-solid fa-pen has-text-link"></i>',
            '</button>',
            '<button class="button without-border px-2 remove">',
                '<i style="" class="fa fa-trash has-text-danger"></i>',
            '</button>'
        ].join('');
    }

    let with_in_row_edit_icons = gobj_read_bool_attr(gobj, "with_in_row_edit_icons");
    if(with_in_row_edit_icons) {
        columns.push({
            field: '_operation',
            title: 'Op',
            align: 'center',
            clickToSelect: false,
            visible: false,
            formatter: operateFormatter,
            events: {
                'click .edit': function (evt, value, row, index) {
                    evt.stopPropagation();
                    show_edit_form(gobj, row, index);
                },
                'click .remove': function (evt, value, row, index) {
                    // Delete this row ({index, row} filled)
                    evt.stopPropagation();
                    gobj_send_event(gobj, "EV_DELETE_ROWS", {index:index, row:row}, gobj);
                }
            }
        });
    }

    /*
     *  Detail row formatter, defined at table level (Fallback) and column level.
     *  Call when user view detail
     *      index: nº de row en la tabla, propio de la tabla, base in 0
     *      row: nuestro registro
     */
    function detailFormatter(index, row, $element) {
        const html = [];
        $.each(row, function (key, value) {
            let col = get_schema_col(gobj, key);
            if(col) {
                // TODO se necesitará otro transformer, o incluso externo configurable
                value = transform__treedb_value_2_table_value(gobj, col, value, row, key);
            }
            html.push('<p><b>' + key + ':</b> ' + value + '</p>');
        });

        return html.join('');
    }

    let bootstrap_table_config = Object.assign({}, gobj_read_attr(gobj, "bootstrap_table_config"));
    let toolbar_id = gobj_read_str_attr(gobj, "toolbar_id");
    Object.assign(bootstrap_table_config, {
        toolbar: `#${toolbar_id}`,
        locale: kw_get_local_storage_value("locale", "es", false),
        uniqueId: desc.pkey?
            desc.pkey : bootstrap_table_config.uniqueId,
        columns: columns,
        detailFormatter: detailFormatter,
        onCheck: function (row, $element) {
            gobj_send_event(gobj, "EV_SELECT_ROWS", {rows: [row]}, gobj);
        },
        onCheckAll: function (rowsAfter, rowsBefore) {
            gobj_send_event(gobj, "EV_UNSELECT_ROWS", {rows: rowsBefore}, gobj);
            gobj_send_event(gobj, "EV_SELECT_ROWS", {rows: rowsAfter}, gobj);
        },
        onCheckSome: function (rows) {
            gobj_send_event(gobj, "EV_SELECT_ROWS", {rows: rows}, gobj);
        },
        onUncheck: function (row, $element) {
            gobj_send_event(gobj, "EV_UNSELECT_ROWS", {rows: [row]}, gobj);
        },
        onUncheckAll: function (rowsAfter, rowsBefore) {
            gobj_send_event(gobj, "EV_UNSELECT_ROWS", {rows: rowsBefore}, gobj);
            gobj_send_event(gobj, "EV_SELECT_ROWS", {rows: rowsAfter}, gobj);
        },
        onUncheckSome: function (rows) {
            gobj_send_event(gobj, "EV_UNSELECT_ROWS", {rows: rows}, gobj);
        },
        onRefresh: function (rows) {
            gobj_send_event(gobj, "EV_REFRESH", {rows: rows}, gobj);
        },
    });

    /*
     *  Convert the tag in a bootstrap table
     */
    $$table.bootstrapTable(bootstrap_table_config);

    refresh_language(gobj_read_attr(gobj, "$container"), t);
}

/************************************************************
 *   Destroy table with bootstrap-table.com
 ************************************************************/
function table__destroy(gobj)
{
    let table_id = gobj_read_str_attr(gobj, "table_id");
    let $table = document.getElementById(`${table_id}`);
    if($table) {
        let $$table = jQuery(`#${table_id}`);
        $$table.bootstrapTable('destroy');
        $table.remove();
        gobj_write_attr(gobj, "$$table", null);
    }
}

/************************************************************
 *  Convert a record column value from backend to frontend
 ************************************************************/
function transform__treedb_value_2_table_value(gobj, col, value, row, field)
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
            if(value) {
                value = `<span class=""><i style="color:limegreen; font-size:1.2rem;" class="fa-regular fa-square-check"></i></i></span>`;
            } else {
                value = `<span class=""><i style="color:orangered; font-size:1.2rem;" class="fa-solid fa-xmark"></i></span>`;
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
                    '<span style="" class="icon fas fa-eye"></span>',
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

            value = new_value;
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
    let $form = createElement2(['form', {id: form_id , class: 'box p-3'}]);

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
            if(id !== 'id') {
                // Show only fields writable (except id)
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
            not_null: is_notnull
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
            } else if(required || not_null) {
                attrs.required = '';
            }
            let extend = ['input', attrs, '', {
                'invalid': function (evt) {
                    this.classList.add('is-danger');
                    let $h = this.parentNode.querySelector('.help');
                    if($h) {
                        $h.textContent = this.validationMessage;
                        $h.style.display = 'block';
                    }
                },
                'blur': function (evt) {
                    if (!this.checkValidity()) {
                        this.classList.add('is-danger');
                        let $h = this.parentNode.querySelector('.help');
                        if($h) {
                            $h.textContent = this.validationMessage;
                            $h.style.display = 'block';
                        }
                    } else {
                        this.classList.remove('is-danger');
                        let $h = this.parentNode.querySelector('.help');
                        if($h) {
                            $h.textContent = '';
                            $h.style.display = 'none';
                        }
                    }
                }
            }];

            let $extend = createElement2(extend);
            $control.appendChild($extend);
            $control.appendChild(createElement2(['p', {class: 'help'}, '']));
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
            } else if(required || not_null) {
                attrs.required = '';
            }
            let extend = ['div', attrs];
            let $extend = createElement2(extend);
            $extend.setAttribute('name', name);
            $control.appendChild($extend);
            break;
        }

        case 'textarea':
        {
            let attrs = {class:'textarea', name:name, placeholder: placeholder};
            Object.assign(attrs, extras);

            if(readonly) {
                attrs.readonly = '';
            } else if(required || not_null) {
                attrs.required = '';
            }
            let extend = ['textarea', attrs];
            let $extend = createElement2(extend);
            $control.appendChild($extend);
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
            break;
        }

        case 'checkbox':
        {
            let extend = ['label', {class: 'checkbox'},
                ['input', {type: 'checkbox', name:name}]
            ];
            let $extend = createElement2(extend);
            $control.appendChild($extend);
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
                                ['i', {class:'fa fa-save'}]
                            ]],
                            ['span', {i18n:'save'}, 'save']
                        ], {
                            click: function(evt) {
                                evt.stopPropagation();
                                evt.preventDefault();
                                let $form = $element.querySelector('form');
                                if ($form.checkValidity()) {
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
                                ['i', {class:'fa-solid fa-xmark'}]
                            ]],
                            ['span', {i18n:'cancel'}, 'cancel']
                        ]],

                        ['button', { class: 'button px-4'}, [
                            ['span', {class:'icon'}, [
                                ['i', {class:'fas fa-broom'}]
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
 *
 ************************************************************/
function clear_validation(gobj, $form)
{
    $form.querySelectorAll('input').forEach(input => {
        input.classList.remove('is-danger');
    });

    $form.querySelectorAll('.help').forEach(input => {
        input.textContent = '';
        input.style.display = 'none';
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
        } else {
            /*
             *  create mode
             */
            if(is_rowid) {
                $id.setAttribute('readonly', 'readonly');
            } else {
                $id.removeAttribute('readonly');
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
                $(input).val(null).trigger('change');
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

    let $$table = gobj_read_attr(gobj, "$$table");
    if($$table) {
        /*
         *  'load': Load the data to the table. The old rows will be removed.
         */
        $$table.bootstrapTable('load', data);
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

    let $$table = gobj_read_attr(gobj, "$$table");
    if($$table) {
        for(let record of data) {
            $$table.bootstrapTable('append', [record]); // Add a new row to table
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

    let $$table = gobj_read_attr(gobj, "$$table");
    if($$table) {
        for(let record of data) {
            $$table.bootstrapTable('updateByUniqueId', {
                id: record.id,
                row: record
            });
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

    let $$table = gobj_read_attr(gobj, "$$table");
    //$$table.bootstrapTable('uncheckAll'); // unselectAll TODO ??? is necessary?

    if($$table) {
        for(let record of data) {
            const row = $$table.bootstrapTable('getRowByUniqueId', record.id);
            if (row) {
                // Delete the row by ID
                $$table.bootstrapTable('removeByUniqueId', record.id);
            } else {
                log_error("delete_data: record not found: " + record.id);
            }
        }
    }

    /*
     *  TODO situate en el row updated ???
     *  Select only if it has update/create mode
     */

    // COMO BORRAR EN LA TABLA
    // Borra con index
    // $$table.bootstrapTable('remove', {
    //     field: '$index',
    //     values: [index]
    // });

    // Borra con id
    // let ids = jQuery.map(rows, function (row) {
    //     return row.id;
    // });
    // $$table.bootstrapTable('remove', {
    //     field: 'id',
    //     values: ids
    // });

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
    let $$table = gobj_read_attr(gobj, "$$table");

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

        $$table.bootstrapTable('showColumn', '_operation');
        $$table.bootstrapTable('showColumn', '_check_box_state_');

        $button_new_record.removeAttribute("disabled");
        $button_paste_record.removeAttribute("disabled");

        let rows = $$table.bootstrapTable('getSelections');
        if (rows.length) {
            $button_delete_record.removeAttribute("disabled");
            $button_copy_record.removeAttribute("disabled");
        }

        $$table.bootstrapTable('refreshOptions', {
            fixedNumber: 1,
            fixedRightNumber: 1
        });

    } else {
        /*
         *  Remove edition mode
         */
        $button_edit_record.classList.remove('is-primary');
        $button_new_record.classList.remove('is-info');
        $button_delete_record.classList.remove('is-danger');

        $$table.bootstrapTable('refreshOptions', {
            fixedNumber: 0,
            fixedRightNumber: 0
        });

        $$table.bootstrapTable('hideColumn', '_operation');
        $$table.bootstrapTable('hideColumn', '_check_box_state_');

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
    let $$table = gobj_read_attr(gobj, "$$table");

    if(json_size(kw) === 0) {
        /*----------------------------*
         *  Delete selected rows
         *----------------------------*/
        let rows = $$table.bootstrapTable('getSelections');
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
    let $$table = gobj_read_attr(gobj, "$$table");

    /*----------------------------*
     *  Copy selected rows
     *----------------------------*/
    let rows = $$table.bootstrapTable('getSelections');
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
    let $$table = gobj_read_attr(gobj, "$$table");
    let $container = gobj_read_attr(gobj, "$container");

    /*
     *  Update button DELETE, only enable if some row is selected
     */
    let $button_delete_record = $container.querySelector(`.button-delete-record`);
    let $button_copy_record = $container.querySelector(`.button-copy-record`);
    if($button_delete_record) {
        let selectedRows = $$table.bootstrapTable('getSelections');
        if (selectedRows.length && gobj_read_bool_attr(gobj, "editable")) {
            $button_delete_record.removeAttribute("disabled");
            $button_copy_record.removeAttribute("disabled");
        } else {
            $button_delete_record.setAttribute("disabled", true);
            $button_copy_record.setAttribute("disabled", true);
        }
    }

    if(gobj_read_bool_attr(gobj, "broadcast_select_rows_event")) {
        //let rows = $$table.bootstrapTable('getSelections');
        gobj_publish_event(gobj, event, kw);
    }
    //console.log("selected rows ", kw.rows);

    return 0;
}

/************************************************************
 *  { rows: [row] }
 ************************************************************/
function ac_unselect_rows(gobj, event, kw, src)
{
    let $$table = gobj_read_attr(gobj, "$$table");
    let $container = gobj_read_attr(gobj, "$container");

    /*
     *  Update button DELETE, only enable if some row is selected
     */
    let $button_delete_record = $container.querySelector(`.button-delete-record`);
    let $button_copy_record = $container.querySelector(`.button-copy-record`);
    if($button_delete_record) {
        let selectedRows = $$table.bootstrapTable('getSelections');
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
        //let rows = $$table.bootstrapTable('getSelections');
        gobj_publish_event(gobj, event, kw);
    }
    //console.log("unselected rows ", kw.rows);

    return 0;
}

/************************************************************
 *  { locale: "es" }
 ************************************************************/
function ac_change_locale(gobj, event, kw, src)
{
    let $$table = gobj_read_attr(gobj, "$$table");
    if($$table) {
        $$table.bootstrapTable('changeLocale', kw.locale);
    }
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
            ["EV_LOAD_NODES",        ac_load_nodes,         null],
            ["EV_LOAD_NODE_CREATED", ac_load_node_created,  null],
            ["EV_LOAD_NODE_UPDATED", ac_load_node_updated,  null],
            ["EV_NODE_DELETED",      ac_node_deleted,       null],

            ["EV_EDITION_MODE",      ac_edition_mode,       null],
            ["EV_CREATE_RECORD",     ac_create_record,      null],
            ["EV_UPDATE_RECORD",     ac_update_record,      null],

            ["EV_NEW_ROW",           ac_new_row,            null],
            ["EV_DELETE_ROWS",       ac_delete_rows,        null],
            ["EV_SELECT_ROWS",       ac_select_rows,        null],
            ["EV_UNSELECT_ROWS",     ac_unselect_rows,      null],
            ["EV_COPY_ROWS",         ac_copy_rows,          null],
            ["EV_PASTE_ROWS",        ac_paste_rows,         null],
            ["EV_SHOW_HOOK_DATA",    ac_show_hook_data,     null],
            ["EV_CHANGE_LOCALE",     ac_change_locale,      null],
            ["EV_REFRESH",           ac_refresh,            null],
            ["EV_SHOW",              ac_show,               null],
            ["EV_HIDE",              ac_hide,               null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_LOAD_NODES",        0],
        ["EV_LOAD_NODE_CREATED", 0],
        ["EV_LOAD_NODE_UPDATED", 0],
        ["EV_NODE_DELETED",      0],

        ["EV_EDITION_MODE",      0],
        ["EV_NEW_ROW",           0],
        ["EV_DELETE_ROWS",       0],
        ["EV_COPY_ROWS",         0],
        ["EV_PASTE_ROWS",        0],
        ["EV_SELECT_ROWS",       event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_UNSELECT_ROWS",     event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_SHOW_HOOK_DATA",    event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_CHANGE_LOCALE",     0],
        ["EV_REFRESH",           0],

        ["EV_CREATE_RECORD",     event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_UPDATE_RECORD",     event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_DELETE_RECORD",     event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_REFRESH_TOPIC",     event_flag_t.EVF_OUTPUT_EVENT],

        ["EV_SHOW",              0],
        ["EV_HIDE",              0]
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
function register_c_yui_treedb_topic_form()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_treedb_topic_form };
