/***********************************************************************
 *          c_yui_form.js
 *
 *          Form (in Window or not)
 *
 *          Copyright (c) 2024-2025, ArtGins.
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
    is_array,
    createElement2,
    clean_name,
    kw_has_key,
    is_object,
    json_deep_copy,
    gobj_read_pointer_attr,
    gobj_parent,
    gobj_subscribe_event,
    gobj_read_attr,
    gobj_send_event,
    gobj_read_bool_attr,
    gobj_name,
    gobj_write_attr,
    kwid_new_dict,
    sprintf,
    template_get_field_desc,
    str_in_list,
    gobj_write_bool_attr,
    gobj_short_name,
    empty_string,
    createOneHtml,
    create_template_record,
    is_date,
    is_string,
    parseBoolean,
    kw_set_dict_value,
    log_warning,
    kw_get_dict_value,
    gobj_read_str_attr,
    gobj_publish_event,
    refresh_language,
} from "yunetas";

import {t} from "i18next";
import "tom-select/dist/css/tom-select.css"; // Import Tom-Select CSS
import TomSelect from "tom-select"; // Import Tom-Select JS

import "tabulator-tables/dist/css/tabulator.min.css"; // Import Tabulator CSS
import "tabulator-tables/dist/css/tabulator_bulma.css";
import { TabulatorFull as Tabulator } from "tabulator-tables"; // Import Full Tabulator JS
// import { Tabulator } from "tabulator-tables";  // Import light Tabulator JS


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_FORM";

/***************************************************************
 *              Data
 ***************************************************************/
/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",       0,  null,   "Subscriber of publishing messages"),
SDATA(data_type_t.DTP_POINTER,  "$parent",          0,  null,   "$container will be appended to $parent if not null, else do it manually"),
/*
 *  template: object -> TEMPLATE, array -> DESC
 */
SDATA(data_type_t.DTP_POINTER,  "template",         0,  "<div>You must supply a template for form</div>", "Template for form, object -> TEMPLATE, array -> DESC"),
SDATA(data_type_t.DTP_JSON,     "record",           0,  "{}",   "Data to form"),
SDATA(data_type_t.DTP_JSON,     "placeholders",     0,  "{}",   "Placeholder values for form fields"),
SDATA(data_type_t.DTP_BOOLEAN,  "editable",         0,  false,  "Default is editable, set false to readonly"),
SDATA(data_type_t.DTP_BOOLEAN,  "selectable",       0,  true,   "Rows selectable with 'click' (BAD with editable=true)"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_checkbox",    0,  false,  "Show a first column with checkbox to select rows"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_rownum",      0,  false,  "Print first column with row number"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_delete_row_btn", 0,  false, "Print last column with delete row button"),
/*
 *  Defaults for tabulator
 */
SDATA(data_type_t.DTP_JSON,     "tabulator_settings",0,  {
    layout: "fitDataFill",
    validationMode: "highlight",
    columnDefaults: {
        resizable: true
    },
    maxHeight: "100%" // do not let table get bigger than the height of its parent element
}, "Default settings for Tabulator"),
SDATA(data_type_t.DTP_POINTER,  "$container",       0,  null,   "Container element"),
SDATA(data_type_t.DTP_STRING,   "form_id",          0,  "",     "Form identifier"),
SDATA(data_type_t.DTP_BOOLEAN,  "initialized",      0,  false,  "Indicates if it is initialized"),
SDATA(data_type_t.DTP_BOOLEAN,  "changes",          0,  false,  "Indicates if changes have been made"),
SDATA_END()
];

let PRIVATE_DATA = {};

let __gclass__ = null;




                    /***************************
                     *      Framework Methods
                     ***************************/




/************************************************************
 *      Framework Method create
 ************************************************************/
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

    let name = clean_name(gobj_name(gobj));
    gobj_write_attr(gobj, "form_id", "form" + name);

    build_ui(gobj);
}

/************************************************************
 *      Framework Method destroy
 *      In this point, all children
 *      and subscriptions are already deleted.
 ************************************************************/
function mt_destroy(gobj)
{
    destroy_ui(gobj);
}

/************************************************************
 *      Framework Method start
 ************************************************************/
function mt_start(gobj)
{
    let record = gobj_read_attr(gobj, "record");
    if(record) {
        gobj_send_event(gobj, "EV_LOAD_RECORD", record, gobj);
    }
}

/************************************************************
 *      Framework Method stop
 ************************************************************/
function mt_stop(gobj)
{
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
    let $form = $container.querySelector('form');

    let $button_save = $container.querySelector('.button-save');
    let $button_undo = $container.querySelector('.button-undo');

    if(changed) {
        /*
         *  Data changed, enable buttons to save/undo
         */
        gobj_write_bool_attr(gobj, "changes", true);

        $button_save.removeAttribute("disabled");
        $button_undo.removeAttribute("disabled");

        $button_save.querySelector('span i').style.color = "MediumSeaGreen";
        $button_undo.querySelector('span i').style.color = "Magenta";

        $form.querySelectorAll('.yui-tabulator-table').forEach($table => {
            if(isEventSet($table.tabulator, "dataChanged")) {
                $table.tabulator.off("dataChanged");
            }

            $table.querySelectorAll('.yui-tabulator-table-nested').forEach($table_nested => {
                if(isEventSet($table_nested.tabulator, "dataChanged")) {
                    $table_nested.tabulator.off("dataChanged");
                }
            });
        });

    } else {
        /*
         *  Data not changed, disable buttons to save/undo
         */
        gobj_write_bool_attr(gobj, "changes", false);

        $button_save.setAttribute("disabled", true);
        $button_undo.setAttribute("disabled", true);

        $button_save.querySelector('span i').style.color = "hsl(var(--bulma-button-h), var(--bulma-button-s), var(--bulma-button-color-l))";
        $button_undo.querySelector('span i').style.color = "hsl(var(--bulma-button-h), var(--bulma-button-s), var(--bulma-button-color-l))";

        // TODO improve
        setTimeout(function() {
            $form.querySelectorAll('.yui-tabulator-table').forEach($table => {
                if(isEventSet($table.tabulator, "dataChanged")) {
                    $table.tabulator.off("dataChanged");
                }
                $table.tabulator.on("dataChanged", function(data) {
                    //data - the updated table data
                    gobj_send_event(gobj, "EV_RECORD_CHANGED", {}, gobj);
                });

                $table.querySelectorAll('.yui-tabulator-table-nested').forEach($table_nested => {
                    if(isEventSet($table_nested.tabulator, "dataChanged")) {
                        $table_nested.tabulator.off("dataChanged");
                    }
                    $table_nested.tabulator.on("dataChanged", function(data) {
                        //data - the updated table data
                        gobj_send_event(gobj, "EV_RECORD_CHANGED", {}, gobj);
                    });
                });
            });
        }, 200);
    }
}

/************************************************************
 *
 ************************************************************/
// Function to read text from the clipboard
async function readClipboard(gobj)
{
    try {
        const text = await navigator.clipboard.readText();
        let kw = JSON.parse(text);
        gobj_send_event(gobj, "EV_PASTE_RECORD", kw, gobj);
    } catch (error) {
        log_error(`Failed to read clipboard contents: ${error}`);
    }
}

/************************************************************
 *   Build UI
 ************************************************************/
function build_ui(gobj)
{
    /*----------------------------------------------*
     *          Layout Schema
     *  HACK to build a layout with rows/columns,
     *  some with width/height variable and some with content size
     *
     *  <is-flex {is-flex-direction-row|is-flex-direction-column}>
     *      <is-flex-shrink-0>  -> content size
     *      <is-flex-grow-1>    -> remain space
     *
     *  the <is-flex> MUST BE defined his size if you want scrollbars
     *
     *----------------------------------------------*/
    let $container = createElement2(
        ['div', {class: 'yui-form-container is-flex is-flex-direction-row overscroll-contain', style:'height:100%; width:100%; box-sizing:border-box;'}, [

            /*----------------------------*
             *      Form
             *----------------------------*/
            ['div', {
                class: 'yui-form is-flex-grow-1 p-2',
                style: 'overflow-y:auto; height:100%;min-height:0;'
            }, build_form(gobj)],

            /*----------------------------*
             *      Right toolbar
             *----------------------------*/
            ['div', {class: 'yui-toolbar-form ml-2 is-flex-shrink-0 is-flex is-flex-direction-column is-justify-content-space-between', style: 'overflow-y:auto; height:100%; max-width:90px;'}, [

                /*----------------------------*
                 *      Top buttons
                 *----------------------------*/
                ['div', {class: 'p-1 is-flex is-flex-direction-column is-align-items-center', style:'width:100%; row-gap:6px;'}, [
                    /*----------------------------*
                     *      Save
                     *----------------------------*/
                    ['button', {class: 'button p-1 is-flex is-flex-direction-column is-align-items-center button-save', style:'width:100%;', disabled: true}, [
                        ['span', {class: 'icon m-0'}, '<i class="fa fa-lg fa-save"></i>'],
                        ['span', {class: 'is-hidden-mobile', i18n: 'save'}, 'save']
                    ], {
                        click: function(evt) {
                            evt.stopPropagation();
                            gobj_send_event(gobj, "EV_SAVE_RECORD", {}, gobj);
                        }
                    }],

                    /*----------------------------*
                     *      Undo
                     *----------------------------*/
                    ['button', {class: 'button p-1 is-flex is-flex-direction-column is-align-items-center button-undo', style:'width:100%;', disabled: true}, [
                        ['span', {class: 'icon m-0'}, '<i class="far fa-lg fa-undo"></i>'],
                        ['span', {class: 'is-hidden-mobile', i18n: 'undo'}, 'undo']
                    ], {
                        click: function(evt) {
                            evt.stopPropagation();
                            gobj_send_event(gobj, "EV_UNDO_RECORD", {}, gobj);
                        }
                    }],

                    /*----------------------------*
                     *      Clear
                     *----------------------------*/
                    ['button', {class: 'button p-1 is-flex is-flex-direction-column is-align-items-center', style:'width:100%;'}, [
                        ['span', {class: 'icon m-0'}, '<i class="fa-solid fa-lg fa-broom-wide"></i>'],
                        ['span', {class: 'is-hidden-mobile', i18n: 'clear'}, 'clear']
                    ], {
                        click: function(evt) {
                            evt.stopPropagation();
                            gobj_send_event(gobj, "EV_CLEAR_RECORD", {}, gobj);
                        }
                    }],
                ]],

                /*----------------------------*
                 *      Bottom buttons
                 *----------------------------*/
                ['div', {class: 'p-1 is-flex is-flex-direction-column is-align-items-center', style:'width:100%; row-gap:6px;'}, [
                    /*----------------------------*
                     *      Copy
                     *----------------------------*/
                    ['button', {class: 'button p-1 is-flex is-flex-direction-column is-align-items-center', style:'width:100%;'}, [
                        ['span', {class: 'icon m-0'}, '<i class="fa fa-lg fa-copy"></i>'],
                        ['span', {class: 'is-hidden-mobile', i18n: 'copy'}, 'copy']
                    ], {click: function(evt) {
                            evt.stopPropagation();
                            gobj_send_event(gobj, "EV_COPY_RECORD", {}, gobj);
                        }
                    }],

                    /*----------------------------*
                     *      Paste
                     *----------------------------*/
                    ['button', {class: 'button p-1 is-flex is-flex-direction-column is-align-items-center', style:'width:100%;'}, [
                        ['span', {class: 'icon m-0'}, '<i class="fa fa-lg fa-paste"></i>'],
                        ['span', {class: 'is-hidden-mobile', i18n: 'paste'}, 'paste']
                    ], {click: async function(evt) {
                            evt.stopPropagation();
                            await readClipboard(gobj);
                        }
                    }]
                ]]
            ]]
        ]]
    );

    gobj_write_attr(gobj, "$container", $container);
    refresh_language($container, t);

    let $parent = gobj_read_attr(gobj, "$parent");
    if($parent) {
        $parent.appendChild($container);
    }
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
 *   Build form with bulma and createElement2
 ******************************************************************/
function build_form(gobj)
{
    let form_id = gobj_read_str_attr(gobj, "form_id");
    let $form = createElement2(['form', {id: form_id, class: 'yui-form'}]);

    /*----------------------------*
     *  Check if template exists
     *----------------------------*/
    let template = gobj_read_attr(gobj, "template");
    if(!is_object(template) && !is_array(template)) {
        // template must a object (template form) or an array (treedb topic form)
        $form.appendChild(createOneHtml(template));
        return;
    }

    if(is_object(template)) {
        /*------------------------------*
         *  Must be a form template
         *------------------------------*/
        build_html_form(gobj, $form, "", template);

    } else if(is_array(template)) {
        /*-----------------------------------------------------*
         *  Must be a treedb desc, transform array in object
         *-----------------------------------------------------*/
        template = kwid_new_dict(gobj, template);
        build_html_form(gobj, $form, "", template);
    }

    return $form;
}

/******************************************************************
 *  Build the html form
 ******************************************************************/
function build_html_form(gobj, $form, prefix, template)
{
    Object.entries(template).forEach(([key, value]) => {
        const field_desc = template_get_field_desc(key, value);
        let html_field_conf = build_form_field_conf(gobj, field_desc);

        /*
         *  items not filled
         */
        html_field_conf.name = field_desc.name;
        html_field_conf.label = field_desc.header;
        html_field_conf.placeholder = field_desc.placeholder?t(field_desc.placeholder):'';

        let placeholders = gobj_read_attr(gobj, "placeholders");
        if(placeholders[key]) {
            html_field_conf.placeholder = placeholders[key];
        }

        /*
         *  Build the html field and add to form, if <tag> is not-null
         */
        if(html_field_conf.tag) {
            let $field = create_form_field(gobj, $form, prefix, field_desc, html_field_conf);
            $form.appendChild($field);
        }
    });
}

/************************************************************
 *  Convert the template types in html types for form
 *  Return a structure to build the html input
 *
 *        return field_conf = {
 *             tag: null,
 *             inputType: '',
 *             name: null,
 *             label: null,
 *             placeholder: '',
 *             options: [],    // options of select, select2, radio
 *             extras: {},     // extra html input/textarea attributes
 *             readonly: !is_writable,
 *             required: is_required,
 *         };
 *
 ************************************************************/
function build_form_field_conf(gobj, field_desc)
{
    let field_conf = {
        tag: null,
        inputType: '',

        name: null,             // Must be filled externally
        label: null,            // Must be filled externally
        placeholder: '',        // Must be filled externally

        options: [],            // select,select2,radio options
        extras: {},             // extra html input/textarea attributes
        readonly: !field_desc.is_writable,
        required: field_desc.is_required,
        hidden: field_desc.is_hidden,
        default_value: field_desc.default_value,
    };

    switch(field_desc.type) {
        case "object":
        case "dict":
            field_conf.tag = "fieldset";
            break;

        case "template":
            field_conf.tag = "fieldset";
            // TODO si es para editar, tiene que ser field_conf.tag = "jsoneditor";
            field_conf.options = field_desc.enum_list;
            break;

        case "table":
            field_conf.tag = "table";
            field_conf.options = field_desc.enum_list;
            break;

        case "blob":
            field_conf.tag = "jsoneditor";
            break;

        case "array":
        case "list":
            field_conf.tag = "table";
            break;

        case "enum":
            switch(field_desc.real_type) {
                case "string":
                    field_conf.tag = "select";
                    field_conf.options = field_desc.enum_list;
                    break;
                case "object":
                case "dict":
                case "array":
                case "list":
                    field_conf.tag = "select2";
                    field_conf.options = field_desc.enum_list;
                    break;
            }
            break;

        case "hook":
            // hooks don't appear in form
            break;

        case "fkey":
            // build later
            break;

        case "image":
        case "string":
            field_conf.tag = 'input';
            field_conf.inputType = 'text';
            break;

        case "integer":
            field_conf.tag = 'input';
            field_conf.inputType = 'text';
            field_conf.extras.inputmode = "decimal";
            field_conf.extras.pattern = "^-?\\d+$";
            break;

        case "real":
        case "number":
            field_conf.tag = 'input';
            field_conf.inputType = 'text';
            field_conf.extras.inputmode = "decimal";
            field_conf.extras.pattern = "^-?\\d+(\\.\\d+)?$";
            break;

        case "rowid":
            field_conf.tag = 'input';
            field_conf.inputType = 'text';
            break;

        case "boolean":
            field_conf.tag = 'checkbox';
            break;

        case "now":
        case "time":
            field_conf.tag = 'input';
            field_conf.inputType = 'datetime-local';
            switch(field_desc.real_type) {
                case "string":
                    break;
                case "integer":
                    break;
            }
            break;

        case "color":
            field_conf.tag = 'input';
            field_conf.inputType = 'color';
            switch(field_desc.real_type) {
                case "string":
                    break;
                case "integer":
                    break;
            }
            break;

        case "email":
            field_conf.tag = 'input';
            // field_conf.inputType = 'email';
            // field_conf.extras.inputmode = "email";
            break;

        case "password":
            field_conf.tag = 'input';
            field_conf.inputType = 'password';
            break;

        case "url":
            field_conf.tag = 'input';
            field_conf.inputType = 'url';
            field_conf.extras.inputmode = "url";
            break;

        case "tel":
            field_conf.tag = 'input';
            field_conf.inputType = 'tel';
            field_conf.extras.inputmode = "tel";
            break;

        case "id":
            field_conf.tag = 'input';
            break;

        case "currency":
            field_conf.tag = 'input';
            break;

        case "hex":
            field_conf.tag = 'input';
            break;

        case "binary":
            field_conf.tag = 'input';
            break;

        case "percent":
            field_conf.tag = 'input';
            break;

        case "base64":
            field_conf.tag = 'input';
            break;

        case "coordinates":
            field_conf.tag = 'coordinates';
            break;

        default:
            log_error(sprintf("%s: type unknown '%s'", gobj_short_name(gobj), field_desc.type));
    }

    return field_conf;
}

/******************************************************************
 *  With html field conf builds an html element,
 *  ready to add to the form
 *
 *  Available html types:
 *      - input
 *      - textarea
 *      - select
 *      - select2
 *      - checkbox
 *      - radio
 *      - jsoneditor
 *      - fieldset
 *      - table
 *
 *  Input:
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
 *      default_value:
 *  }
 ******************************************************************/
function create_form_field(
    gobj,
    $form,
    prefix,
    field_desc,
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
        default_value
    }
)
{
    /*--------------------------------------*
     *      Create the FORM bulma structure
     *
     *      First part:
     *
     *      -> field
     *          -> field-label
     *              -> label
     *          -> field-body
     *              -> field
     *                  -> control
     *
     *      Second part below -> control
     *
     *                      -> <input>
     *                      -> <textarea>
     *                      -> <select>
     *                      -> <select2>
     *                      -> <checkbox>
     *                      -> <radio>
     *                      -> <jsoneditor>
     *                      -> <fieldset>
     *                      -> <table>
     *
     *--------------------------------------*/
    let $element;
    let kw_add_table_row = {
        $row: null,     // not null if it's a nested table
        $table: null,   // Element of tabulator
        record: {},
        loading: false,
    };

    switch (tag) {
        case 'input':
            switch (inputType) {
                case 'color':
                    $element = createElement2(
                        ['div', {class: 'xfield field is-horizontal'}, [
                            ['div', {class: 'field-label is-normal' }, [
                                ['label', {class:'label ', i18n:label, for:name}, label]
                            ]],

                            ['div', {class: 'field-body'}, [
                                ['div', {class: 'field'}, [
                                    ['div', {class: 'xcontrol control', style: 'max-width:100px;'}, []]
                                ]]
                            ]]
                        ]]
                    );
                    break;
                default:
                    $element = createElement2(
                        ['div', {class: 'xfield field is-horizontal'}, [
                            ['div', {class: 'field-label is-normal' }, [
                                ['label', {class:'label ', i18n:label, for:name}, label]
                            ]],

                            ['div', {class: 'field-body'}, [
                                ['div', {class: 'field'}, [
                                    ['div', {class: 'xcontrol control', style: ''}, []]
                                ]]
                            ]]
                        ]]
                    );
                    break;
            }
            break;
        case 'textarea':
        case "select":
        case "select2":
        case 'checkbox':
        case 'radio':
        case "jsoneditor":
        case "fieldset":
            $element = createElement2(
                ['div', {class: 'xfield field is-horizontal'}, [
                    ['div', {class: 'field-label is-normal' }, [
                        ['label', {class:'label ', i18n:label, for:name}, label]
                    ]],

                    ['div', {class: 'field-body'}, [
                        ['div', {class: 'field'}, [
                            ['div', {class: 'xcontrol control' }, []]
                        ]]
                    ]]
                ]
                ]);
            break;

        case "coordinates":
            $element = createElement2(
                ['div', {class: 'xfield field is-horizontal'}, [
                    ['div', {class: 'field-label is-normal' }, [
                        ['label', {class:'label ', i18n:label, for:name}, label]
                    ]],

                    ['div', {class: 'field-body'}, [
                        ['div', {class: 'field'}, [
                            ['div', {class: 'xcontrol control' }, []]
                        ]]
                    ]]
                ]
            ]);
            break;

        case "table":
            /*---------------------------------------*
             *  Create a "+" button to add new rows
             *---------------------------------------*/
            $element = createElement2(
                ['div', {class: 'field is-horizontal tabulator-buttons'}, [
                    ['div', {class: 'field-label is-normal' }, [
                        ['label', {class:'label', i18n:label}, label]
                    ]],

                    ['div', {class: 'field-body'}, [
                        ['div', {class: 'field'}, [
                            ['div', {class: '' }, [
                                ['button', {class: 'button p-1'}, [
                                    ['span', {class: 'icon m-0'}, '<i class="fa-solid fa-plus"></i>'],
                                    ['span', {class: 'p-1 pr-2 is-hidden-mobile', i18n: 'add'}, 'add'],
                                ], {
                                    click: function(evt) {
                                        evt.stopPropagation();
                                        evt.preventDefault();
                                        gobj_send_event(gobj,
                                            "EV_ADD_TABLE_ROW", kw_add_table_row, gobj
                                        );
                                    }
                                }]
                            ]]
                        ]]
                    ]]
                ]]
            );
            $form.appendChild($element);
            $element = createElement2(
                ['div', {class: 'xfield field is-horizontal'}, [
                    ['div', {class: 'field-label is-normal' }, [
                        ['label', {class:'label'}, '']
                    ]],

                    ['div', {class: 'field-body', style: 'overflow-x: auto;'}, [
                        ['div', {class: 'field', style: 'overflow: hidden;'}, [
                            ['div', {class: 'xcontrol control', style: ''}, []]
                        ]]
                    ]]
                ]]
            );
            break;

        default:
            $element = createElement2(
                ['div', {class: 'xfield field is-horizontal'}, [
                    ['div', {class: 'field-label is-normal' }, [
                        ['label', {class:'label ', i18n:label, for:name}, label]
                    ]],

                    ['div', {class: 'field-body'}, [
                        ['div', {class: 'field'}, [
                            ['div', {class: 'xcontrol control' }, []]
                        ]]
                    ]]
                ]
            ]);
    }

    /*---------------------*
     *      Is hidden?
     *---------------------*/
    if(hidden) {
        $element.style.display = "none";
    }

    /*-------------------------------------------*
     *  Create $extend, the real input element
     *-------------------------------------------*/
    let $control = $element.querySelector('.xcontrol');
    let $extend = null;

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
            } else if(required) {
                attrs.required = '';
            }
            let extend = ['input', attrs, '', {
                'invalid': function (evt) {
                    this.classList.add('is-danger');
                    let $h = this.parentNode.querySelector('.help');
                    $h.textContent = this.validationMessage;
                    $h.style.display = 'block';
                },
                'input': function (evt) {
                    gobj_send_event(gobj, "EV_RECORD_CHANGED", {}, gobj);
                },
                'blur': function (evt) {
                    if (!this.checkValidity()) {
                        this.classList.add('is-danger');
                        let $h = this.parentNode.querySelector('.help');
                        $h.textContent = this.validationMessage;
                        $h.style.display = 'block';
                    } else {
                        this.classList.remove('is-danger');
                        let $h = this.parentNode.querySelector('.help');
                        $h.textContent = '';
                        $h.style.display = 'none';
                    }
                }
            }];

            $extend = createElement2(extend);
            $control.appendChild($extend);
            break;
        }

        case 'textarea':
        {
            let attrs = {
                class: 'textarea',
                name: name,
                placeholder: placeholder
            };
            Object.assign(attrs, extras);

            if(readonly) {
                attrs.readonly = '';
            } else if(required) {
                attrs.required = '';
            }
            let extend = ['textarea', attrs];
            $extend = createElement2(extend);
            $control.appendChild($extend);
            break;
        }

        case "select":
        {
            // ["item"]
            // <option value="item" data-i18n:"item"> item </option>
            let extend = ['div', {class: "select" },
                ["select",
                    {
                        name: name
                    },
                    options.map(option =>
                        ['option', {value:option, i18n:option}, option]
                    )
                ]
            ];
            $extend = createElement2(extend);
            $control.appendChild($extend);
            break;
        }

        case "select2":
        {
            /*
             *  https://tom-select.js.org/docs/api/
             */
            let extend = ["select",
                {
                    name: name,
                    class: '',
                    multiple: true,
                },
                options.map(option =>
                    ['option', {value:option, i18n:option}, option]
                )
            ];
            $extend = createElement2(extend);
            $control.appendChild($extend);
            $extend.tom_select = new TomSelect($extend, {
                allowEmptyOption: true,
                placeholder: placeholder,
                plugins: {
                    'clear_button': {
                        'title': t('Remove all selected options'),
                    },
                    'no_active_items': {},
                    'no_backspace_delete': {},
                    'remove_button': {}
                }
            });
            $extend.tom_select.on('change', function() {
                gobj_send_event(gobj, "EV_RECORD_CHANGED", {}, gobj);
            });
            break;
        }

        case 'checkbox':
        {
            let extend = ['label', {class: 'checkbox'},
                ['input', {type: 'checkbox', name:name}]
            ];
            $extend = createElement2(extend);
            $control.appendChild($extend);
            break;
        }

        case 'radio':
        {
            // TODO review, not tested
            // for(const option of options) {
            //     // WARNING here can't use data-i18n, the radio element is destroyed.
                let elm = `
                   <label class="radio">
                      <input type="radio" name="${name}">
                      ${t(options)}
                    </label>
                `;

                $extend = createOneHtml(elm);
                $control.appendChild($extend);
            // }
            break;
        }

        case "jsoneditor":
        {
            let attrs = {
                class: 'jsoneditor jse-theme-dark',
                style: ''
            };
            Object.assign(attrs, extras);

            if(readonly) {
                attrs.readonly = '';
            } else if(required) {
                attrs.required = '';
            }
            let extend = ['div', attrs];
            $extend = createElement2(extend);
            $control.appendChild($extend);
            break;
        }

        case "fieldset":
        {
            // <fieldset>
            //     <legend> Fieldset Name </legend>
            let extend = ["fieldset", {name:name}];
            $extend = createElement2(extend);
            $control.appendChild($extend);
            $extend.template = options;
            let new_prefix = "";
            if(empty_string(prefix)) {
                new_prefix = name;
            } else {
                new_prefix = prefix + '`' + name;
            }
            build_html_form(gobj, $extend, new_prefix, options);
            break;
        }

        case "table":
        {
            let extend = ['div',
                {
                    class: 'yui-tabulator-table is-bordered is-striped',
                    style: ''
                }
            ];
            $extend = createElement2(extend);
            $control.appendChild($extend);
            $extend.tabulator = create_tabulator(gobj, $extend, name, options);
            kw_add_table_row["$table"] = $extend;
            break;
        }

        case "coordinates":
        {
            let attrs = {
                class: 'yui-coordinates',
                name: name,
                // type: inputType,
                placeholder: placeholder
            };
            Object.assign(attrs, extras);

            if(readonly) {
                attrs.readonly = '';
            } else if(required) {
                attrs.required = '';
            }
            $extend = create_coordinates(gobj, attrs);
            $control.appendChild($extend);
            break;
        }

        default:
            $extend = createElement2(['strong', {class: 'has-text-danger'}, `Unsupported element type: "${tag}"`]);
            $control.appendChild($extend);
    }

    if($extend) {
        $control.appendChild(createElement2(['p', {class: 'help'}, '']));
        $extend.field_desc = field_desc;
        $extend.dataset.treedb_name = name;
        $extend.dataset.treedb_prefix = prefix;
        $extend.dataset.form_tag = tag;
        $extend.dataset.form_input_type = inputType;
        $extend.classList.add('yui-form-data-input');
    }

    return $element;
}


/******************************************************************
 *  Create coordinates
 ******************************************************************/
function create_coordinates(gobj, attrs)
{
    function getLocation(gobj, $input)
    {
        function showPosition(position) {
            const latitude = position.coords.latitude;
            const longitude = position.coords.longitude;
            $input.value = `${longitude}, ${latitude}`;

            $input.classList.remove('is-danger');
            let $h = $input.closest('.xcontrol').parentNode.querySelector('.help');
            $h.textContent = '';
            $h.style.display = 'none';

            gobj_send_event(gobj, "EV_RECORD_CHANGED", {}, gobj);
        }

        function showError(error)
        {
            let errorMsg = "";

            switch (error.code) {
                case error.PERMISSION_DENIED:
                    errorMsg = t("permission denied");
                    break;
                case error.POSITION_UNAVAILABLE:
                    errorMsg = t("location unavailable");
                    break;
                case error.TIMEOUT:
                    errorMsg = t("timed out");
                    break;
                default:
                    errorMsg = t("unknown error");
                    break;
            }

            $input.classList.add('is-danger');
            let $h = $input.closest('.xcontrol').parentNode.querySelector('.help');
            $h.textContent = errorMsg;
            $h.style.display = 'block';
        }

        // Check if Geolocation API is supported
        if (navigator.geolocation) {
            navigator.geolocation.getCurrentPosition(showPosition, showError);
        } else {
            $input.value = `0, 0`;
            let $h = $input.closest('.xcontrol').parentNode.querySelector('.help');
            $h.textContent = t("geolocation is not supported");
            $h.style.display = 'block';
        }
    }

    let extend = ['div', {class: 'field is-flex is-align-items-center'}, [
        ['div', {class: 'control has-icons-left has-icons-right'}, [
            [
                'input',
                {class: 'input', type: 'text', placeholder: t('coordinates') + '...'},
                '',
                {
                    'input': function (evt) {
                        let $btn = this.closest('.control').querySelector('.clear-button');
                        if (this.value) {
                            $btn.style.display = 'inline-flex';
                        } else {
                            $btn.style.display = 'none';
                        }
                        gobj_send_event(gobj, "EV_RECORD_CHANGED", {}, gobj);
                    }
                }
            ],
            [
                'span',
                {
                    class: 'icon is-left is-clickable'
                },
                '<i class="fa-solid fa-location-crosshairs"></i>',
                {
                    'click': function (evt) {
                        evt.stopPropagation();
                        let $input = this.closest('.control').querySelector('input');
                        getLocation(gobj, $input);
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
                    'click': function (evt) {
                        evt.stopPropagation();
                        let $input = this.closest('.control').querySelector('input');
                        $input.value = '';
                        this.style.display = 'none';
                        // gobj.config.select_by_name = '';
                        // gobj_send_event(gobj, "EV_SELECT", {}, gobj);
                    }
                }
            ]
        ]]
    ]];

    return createElement2(extend);
}

/******************************************************************
 *  Create tabulator
 ******************************************************************/
function create_tabulator(gobj, $extend, name, template)
{
    function get_selectable_state(row) {
        return gobj_write_bool_attr(gobj, "selectable");
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

    let sub_elements = [];
    tabulator_settings.columns = template2columns(gobj, columns, template, sub_elements);

    if(sub_elements.length > 0) {
        tabulator_settings.rowFormatter = function(row) {
            for(let sub_element of sub_elements) {
                let extend = ['div',
                    {
                        class: 'yui-tabulator-table-nested pl-6 is-bordered is-striped',
                        "name": sub_element.name,
                    }
                ];
                let $extend = createElement2(extend);
                $extend.tabulator = create_tabulator(
                    gobj, $extend, sub_element.name, sub_element.template
                );

                // Append the container to the row's element
                row.getElement().appendChild($extend);
            }
        };
    }

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
    tabulator.on("rowClick", function(e, row) {
        //e - the click event object
        //row - row component
        e.preventDefault();
        if(get_selectable_state(row)) {
            row.toggleSelect();
        }
    });

    tabulator.on("cellClick", function(e, cell) {
        //e - the click event object
        //cell - cell component
        e.preventDefault();
        let row = cell.getRow();
        // if(get_selectable_state(row)) {
        //     // cannot set at same time with rowClick, one select and other de-select
        //     row.toggleSelect();
        // }
    });

    return tabulator;
}

/******************************************************************
 *  Convert a template's items in tabulator columns
 ******************************************************************/
function template2columns(gobj, columns, template, sub_elements)
{
    function get_editable_state(cell) {
        return gobj_read_bool_attr(gobj, "editable");
    }

    Object.entries(template).forEach(([key, value]) => {
        /*
         *  Decode the item in template into html item
         */
        const field_desc = template_get_field_desc(key, value);

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
                if(value === undefined) {
                    return field_desc.default_value;

                } else {
                    return value;
                }
            };
        }

        if(field_desc.is_required) {
            validator.push("required");
        }

        // TODO añade los validator necesarios, como validator.push("integer");

        switch(field_desc.type) {
            case "object":
            case "dict":
                // TODO es otro formulario, pongo un button y que abra un popup?
                // Object.entries(value).forEach(([key, value]) => {
                //     build_html_form_field_conf_from_template(gobj, $form, key, value);
                // });
                break;

            case "template":
                // field_conf.tag = "fieldset";
                // TODO si es para editar, tiene que ser field_conf.tag = "jsoneditor";
                break;

            case "blob":
                // field_conf.tag = "jsoneditor";
                column.editor = "textarea";
                column.editorParams = {
                };
                break;

            case "table":
            case "array":
            case "list":
                // field_conf.tag = "table";
                // field_conf.options =  enum_list;
                // column.editor = "false";
                // TODO es otra tabla, poner buttons:
                // TODO     - para mostrar/ocultar la segunda tabla

                let template = field_desc.enum_list;
                sub_elements.push({
                    name: field_desc.name,
                    template: template
                });

                Object.assign(column, {
                    formatter: function(cell, formatterParams) {
                        return '<i style="" class="fa-solid fa-plus has-text-link"></i>';
                    },
                    hozAlign:"center",
                    cellClick: function(evt, cell) {
                        evt.stopPropagation();
                        let $row = cell.getRow().getElement();
                        let $extend = $row.querySelector(`[name="${field_desc.name}"]`);
                        gobj_send_event(gobj,
                            "EV_ADD_TABLE_ROW",
                            {
                                $row: $row,         // not null if it's a nested table
                                $table: $extend,    // Element of tabulator
                                record: {},
                                loading: false,
                            },
                            gobj
                        );
                    }
                });
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
                break;

            case "now":
            case "time":
                // field_conf.tag = 'input';
                // field_conf.inputType = 'datetime-local';
                column.editor = "datetime";
                column.editorParams = {
                    format: "dd/MM/yyyy hh:mm", // the format of the date value stored in the cell
                    verticalNavigation: "table", //navigate cursor around table without changing the value
                    elementAttributes: {
                        title:"slide bar to choose option" // custom tooltip
                    }
                };
                switch(field_desc.real_type) {
                    case "string":
                        break;
                    case "integer":
                        break;
                }
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

            case "coordinates":
                // case not used by the moment
                log_warning("coordinates in template");
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
    });

    return columns;
}

/************************************************************
 *
 ************************************************************/
function validate_form(gobj, $form)
{
    let validated = true;

    if(!$form.checkValidity()) {
        validated = false;
    }

    $form.querySelectorAll('.yui-tabulator-table').forEach($table => {
        if(!$table.tabulator.validate()) {
            validated = false;
        }

        $table.querySelectorAll('.yui-tabulator-table-nested').forEach($table_nested => {
            if(!$table_nested.tabulator.validate()) {
                validated = false;
            }
        });
    });

    return validated;
}

/************************************************************
 *  Function to get input value based on its type
 ************************************************************/
function getInputValue($input) {
    if ($input.type === "checkbox" || $input.type === "radio") {
        return $input.checked;
    } else {
        return $input.value;
    }
}

/************************************************************
 *  Function to get input value based on its type
 ************************************************************/
function get_form_values(gobj, $form)
{
    const record = {};

    $form.querySelectorAll('.yui-form-data-input').forEach($input => {
        let $xfield = $input.closest('.xfield');
        let field_desc = $input.field_desc;
        let name = $input.dataset.treedb_name;
        let prefix = $input.dataset.treedb_prefix;
        let tag = $input.dataset.form_tag;
        let input_type = $input.dataset.form_input_type;

        let value = null;

        switch(tag) {
            case "input":
                switch(input_type) {
                    case "datetime-local":
                        // Create a new Date object from the datetime-local $input value
                        const date = new Date($input.value);
                        // Convert the Date object to epoch time (in seconds)
                        value = Math.floor(date.getTime() / 1000);
                        break;
                    default:
                        value = getInputValue($input);
                        break;
                }
                break;
            case "textarea":
                value = getInputValue($input);
                break;
            case "select":
                value = getInputValue($input);
                break;
            case "select2":
                value = $input.tom_select.getValue();
                break;
            case "checkbox":
                value = getInputValue($input);
                break;
            case "radio":
                value = getInputValue($input);
                break;

            case "jsoneditor":
                let jsoneditor = $input.jsoneditor;
                if(jsoneditor) {
                    name = $input.getAttribute('name');
                    let value_ = jsoneditor.get();
                    if(value_.text===undefined) {
                        value = JSON.stringify(value_.json, null, null);
                    } else {
                        value = value_.text;
                    }
                }
                break;

            case "fieldset":
                /*
                 *  HACK: really this is the path of the value.
                 *  We are navigating by a tree data structure; this is one node, not a leaf.
                 */
                break;

            case "table":
                let template = gobj_read_attr(gobj, "template");
                if(!empty_string(name) && kw_has_key(template, name)) {
                    value = [];
                    let rows = $input.tabulator.getRows();

                    for(let row of rows) {
                        let d = row.getData();
                        value.push(d);
                        let $row = row.getElement();
                        $row.querySelectorAll('.yui-tabulator-table-nested').forEach($nested => {
                            let name_n = $nested.getAttribute("name");
                            d[name_n] = $nested.tabulator.getData();
                        });
                    }
                }
                break;

            case "coordinates":
                /*
                 *
                 */
                if($input.tagName !== "INPUT") {
                    $input = $input.querySelector("input");
                }
                value = getInputValue($input);
                break;

            default:
                log_error(
                    sprintf("%s: $form type unknown: '%s'",
                        gobj_short_name(gobj),
                        tag
                    )
                );
                break;
        }

        value = form_value_2_treedb_value(gobj, field_desc, value);
        let path;
        if(empty_string(prefix)) {
            path = name;
        } else {
            path = prefix + '`' + name;
        }
        kw_set_dict_value(gobj, record, path, value);
        //record[name] = value;
    });

    return record;
}

/************************************************************
 *
 ************************************************************/
function clear_data(gobj, $form)
{
    $form.querySelectorAll('input, select, textarea, .jsoneditor').forEach($input => {
        if($input.name !== 'id') { // TODO review
            if($input.classList.contains("jsoneditor")) {
                let jsoneditor = $input.jsoneditor;
                if(jsoneditor) {
                    jsoneditor.set({text: ''});
                }
            } else if($input.type === 'checkbox') {
                $input.checked = false;
            } else if($input.type === 'radio') {
                $input.checked = false;

            } else if($input.type === 'datetime-local') { /* time */
                $input.value = null;

            } else if($input.type === 'select-multiple') {
                $input.tom_select.clear();
            } else {
                $input.value = null;
            }
        }
    });

    $form.querySelectorAll('.yui-tabulator-table').forEach($table => {
        if($table.tabulator.initialized) {
            $table.tabulator.clearData();
        }

        $table.querySelectorAll('.yui-tabulator-table-nested').forEach($table_nested => {
            if($table_nested.tabulator.initialized) {
                $table_nested.tabulator.clearData();
            }
        });
    });
}

/************************************************************
 *
 ************************************************************/
function clear_validation(gobj, $form)
{
    $form.querySelectorAll('input').forEach($input => {
        $input.classList.remove('is-danger');
    });

    $form.querySelectorAll('.help').forEach($input => {
        $input.textContent = '';
        $input.style.display = 'none';
    });

    $form.querySelectorAll('.yui-tabulator-table').forEach($table => {
        $table.tabulator.clearCellValidation();

        $table.querySelectorAll('.yui-tabulator-table-nested').forEach($table_nested => {
            $table_nested.tabulator.clearCellValidation();
        });
    });
}

/************************************************************
 *
 ************************************************************/
function convertColor(color)
{
    function rgbToHex(rgb) {
        const result = /^rgb\((\d+),\s*(\d+),\s*(\d+)\)$/.exec(rgb);
        if (!result) {
            return null;
        }
        const r = parseInt(result[1], 10);
        const g = parseInt(result[2], 10);
        const b = parseInt(result[3], 10);
        return `#${((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1).toUpperCase()}`;
    }

    const tempElement = document.createElement('div');
    tempElement.style.color = color;
    document.body.appendChild(tempElement);

    const rgbColor = window.getComputedStyle(tempElement).color;
    document.body.removeChild(tempElement);

    const hexColor = rgbToHex(rgbColor);
    return hexColor?hexColor:color;
}

/************************************************************
 *
 ************************************************************/
function set_form_values(gobj, template, $form, record)
{
    let modified = false;

    clear_data(gobj, $form);

    if(!record || record.length === 0) {
        record = create_template_record(template);
    }

    $form.querySelectorAll('.yui-form-data-input').forEach($input => {
        // let $xfield = $input.closest('.xfield');
        let field_desc = $input.field_desc;
        let name = $input.dataset.treedb_name;
        let prefix = $input.dataset.treedb_prefix;
        let tag = $input.dataset.form_tag;
        let input_type = $input.dataset.form_input_type;

        let path = "";
        if(empty_string(prefix)) {
            path = name;
        } else {
            path = prefix + '`' + name;
        }

        let value = kw_get_dict_value(gobj, record, path);
        if(value === undefined && field_desc.default_value !== undefined) {
            value = field_desc.default_value;
            modified = true;
        }
        value = treedb_value_2_form_value(gobj, field_desc, value);

        switch(tag) {
            case "input":
                switch(input_type) {
                    case "datetime-local":
                        // TODO $input.value = epochToDateTimeLocal(value);
                        break;
                    case "color":
                        $input.value = convertColor(value);
                        break;
                    default:
                        $input.value = value;
                        break;
                }
                break;
            case "textarea":
                $input.value = value;
                break;
            case "select":
                $input.value = value;
                break;
            case "select2":
                $input.tom_select.clear();
                $input.tom_select.setValue(value);
                break;
            case "checkbox":
                $input.checked = value;  // Assume the value is true or false
                break;
            case "radio":
                // TODO review
                // let radio = $form.querySelector(`input[name="${key}"][value="${record[key]}"]`);
                // if (radio) {
                //     radio.checked = true;
                // }
                $input.value = value;
                break;

            case "jsoneditor":
                let jsoneditor = $input.jsoneditor;
                if(jsoneditor) {
                    jsoneditor.set(value);
                }
                break;

            case "fieldset":
                /*
                 *  HACK: really this is the path of the value.
                 *  We are navigating by a tree data structure; this is one node, not a leaf.
                 */
                break;

            case "table":
                load_tabulator_data(gobj, $input, value);
                break;

            case "coordinates":
                /*
                 *
                 */
                // input is below!
                if($input.tagName !== "INPUT") {
                    $input = $input.querySelector("input");
                }
                $input.value = value;
                break;

            default:
                log_error(
                    sprintf("%s: $form type unknown: '%s'",
                        gobj_short_name(gobj),
                        tag
                    )
                );
                break;
        }
    });

    return modified;
}

/************************************************************
 *  loading data from backend to tabulator,
 *  check if the table is built
 ************************************************************/
function load_tabulator_data(gobj, $table, value)
{
    if(!value) {
        log_error(sprintf("%s: no data to load_tabulator_data()", gobj_short_name(gobj)));
        return;
    }
    let tabulator = $table.tabulator;
    if(tabulator.initialized) {
        for(let row of value) {
            gobj_send_event(gobj,
                "EV_ADD_TABLE_ROW",
                {
                    $row: null,     // not null if it's a nested table
                    $table: $table, // Element of tabulator
                    record: row,
                    loading: true,
                },
                gobj
            );
        }
    } else {
        tabulator.on("tableBuilt", function() {
            for(let row of value) {
                gobj_send_event(gobj,
                    "EV_ADD_TABLE_ROW",
                    {
                        $row: null,     // not null if it's a nested table
                        $table: $table, // Element of tabulator
                        record: row,
                        loading: true,
                    },
                    gobj
                );
            }
        });
    }
}

/************************************************************
 *  Convert a record column value from backend
 *      to frontend form field
 ************************************************************/
function treedb_value_2_form_value(gobj, field_desc, value)
{
    let type = field_desc.type;
    let real_type = field_desc.real_type;

    switch(type) {
        case "fkey":
        case "hook":
        case "enum":
        case "uuid":
        case "rowid":
        case "password":
        case "email":
        case "url":
            break;
        case "now":
        case "time":
            value = parseInt(value) || 0;
            value = new Date(value);
            break;
        case "date":
        case "color":
        case "image":
        case "tel":
        case "template":
        case "table":
        case "id":
        case "currency":
        case "hex":
        case "binary":
        case "percent":
        case "base64":
            break;

        case "string":
        case "integer":
        case "object":
        case "dict":
        case "array":
        case "list":
        case "real":
        case "boolean":
            break;
        case "blob":
            // Return as it is, manage by jsoneditor
            // TODO si es para editar, tiene que ser
            // value = {json: value};
            break;
        case "number":
            break;
        case "coordinates":
            let longitude = 0;
            let latitude = 0;

            let coordinates = value;
            if(is_array(coordinates) && coordinates.length >=2) {
                /*
                 *  We use GeoJSON order: [lng,lat]
                 *
                 *  "geometry": {
                 *      "type": "Point",
                 *      "coordinates": [0, 0]
                 *  }
                 *
                 *  WARNING Google Maps is [lat, lng]
                 */
                longitude = coordinates[0];
                latitude = coordinates[1];
            } else if(is_object(coordinates)) {
                try {
                    coordinates = coordinates.geometry.coordinates;
                    longitude = coordinates[0];
                    latitude = coordinates[1];
                } catch (e) {
                    log_error(e);
                }
            }
            value = `${longitude}, ${latitude}`;
            break;

        default:
            log_error("treedb_value_2_form_value() type unknown: " + type);
            break;
    }

    return value;
}

/************************************************************
 *  Convert from frontend to backend
 *  operation: "create" "update"
 ************************************************************/
function form_value_2_treedb_value(gobj, field_desc, value)
{
    let type = field_desc.type;
    let real_type = field_desc.real_type;

    switch(type) {
        case "fkey":
            break;
        case "hook":
            value = null;
            break;
        case "enum":
        case "uuid":
        case "rowid":
        case "password":
        case "email":
        case "url":
            break;
        case "now":
        case "time":
            switch(real_type) {
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
                    log_error("incompatible type for time: " + real_type);
                    break;
            }
            break;
        case "date":
        case "color":
        case "image":
        case "tel":
            break;
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
        case "table":
        case "id":
        case "currency":
        case "hex":
        case "binary":
        case "percent":
        case "base64":
            break;

        case "string":
            break;
        case "integer":
            value = parseInt(value) || 0;
            break;
        case "object":
        case "dict":
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
        case "real":
            value = parseFloat(value)  || 0.0;
            break;
        case "boolean":
            value = parseBoolean(value);
            break;
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
        case "number":
            break;
        case "coordinates":
            // Split the string into an array and map it to floats
            let lnglat = value.split(",").map(num => parseFloat(num.trim()));
            value = {
                "geometry": {
                    "type": "Point",
                    "coordinates": [lnglat[0], lnglat[1]]
                }
            };
            break;

        default:
            log_error("form_value_2_treedb_value() type unknown: " + type);
            break;
    }



    return value;
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
 *  From external, load new record
 ************************************************************/
function ac_load_record(gobj, event, kw, src)
{
    gobj_write_attr(gobj, "record", kw);
    let $container = gobj_read_attr(gobj, "$container");
    let $form = $container.querySelector('form');

    gobj_write_bool_attr(gobj, "initialized", false);
    let modified = set_form_values(gobj, gobj_read_attr(gobj, "template"), $form, kw);
    set_changed_stated(gobj, modified);
    gobj_write_bool_attr(gobj, "initialized", true);

    return 0;
}

/************************************************************
 *  From internal form button
 *  Publish record to be saved by external clients
 ************************************************************/
function ac_save_record(gobj, event, kw, src)
{
    let $container = gobj_read_attr(gobj, "$container");
    let $form = $container.querySelector('form');

    if(validate_form(gobj, $form)) {
        let values = get_form_values(gobj, $form);
        gobj_publish_event(gobj, "EV_SAVE_RECORD", values);
        set_changed_stated(gobj, false);

    } else {
        kw.abort_close = true; // Don't close the window until fields repaired
        kw.warning = "Cannot save with wrong fields";
    }
    return 0;
}

/************************************************************
 *  From internal form button
 ************************************************************/
function ac_undo_record(gobj, event, kw, src)
{
    let $container = gobj_read_attr(gobj, "$container");
    let $form = $container.querySelector('form');

    gobj_write_bool_attr(gobj, "initialized", false);
    set_form_values(gobj, gobj_read_attr(gobj, "template"), $form, gobj_read_attr(gobj, "record"));

    set_changed_stated(gobj, false);
    gobj_write_bool_attr(gobj, "initialized", true);

    return 0;
}

/************************************************************
 *  From internal form button
 ************************************************************/
function ac_clear_record(gobj, event, kw, src)
{
    let $container = gobj_read_attr(gobj, "$container");
    let $form = $container.querySelector('form');
    clear_validation(gobj, $form);
    clear_data(gobj, $form);
    return 0;
}

/************************************************************
 *  From internal form button for new rows
 *  or from load_tabulator_data() to load data from backend
 ************************************************************/
function ac_add_table_row(gobj, event, kw, src)
{
    let $row = kw["$row"];  // not null if it's a nested table in this row
    let $table = kw.$table; // Element of tabulator
    let tabulator = $table.tabulator;
    let record = kw.record;
    let loading = kw.loading;

    tabulator.addRow(record)
        .then(function(row) {
            if(loading) {
                /*
                 *  Loading from backend, can have nested tables
                 */
                let template = $table.template;
                Object.entries(template).forEach(([key, template_value]) => {
                    if(str_in_list(template_value.flag, "table")) {
                        let $row_nested = row.getElement();
                        let $table_nested = $row_nested.querySelector(`.yui-tabulator-table-nested[name="${key}"]`);
                        if($table_nested) {
                            load_tabulator_data(gobj, $table_nested, record[key]);
                        } else {
                            log_error(sprintf("%s: nested table not found '%s'", gobj_short_name(gobj), key));
                        }
                    }
                });
            }
        })
        .catch(function(error){
            //handle error updating data
        });

    // TODO improve, to use only once the timeout?
    setTimeout(function() {
        if($row) {
            let $tabulator = $row.closest('.yui-tabulator-table');
            if($tabulator && $tabulator.tabulator) {
                $tabulator.tabulator.redraw(); /* Delete double scrollbar */
            }
        } else {
            tabulator.redraw(); /* Delete double scrollbar */
        }
    }, 30);

    return 0;
}

/************************************************************
 *  Internal event
 ************************************************************/
function ac_record_changed(gobj, event, kw, src)
{
    if(gobj_read_bool_attr(gobj, "initialized")) {
        set_changed_stated(gobj, true);
    }
    return 0;
}

/************************************************************
 *  From internal form button
 ************************************************************/
function ac_copy_record(gobj, event, kw, src)
{
    let $container = gobj_read_attr(gobj, "$container");
    let $form = $container.querySelector('form');
    if(validate_form(gobj, $form)) {
        let record = get_form_values(gobj, $form);
        let data = {
            record: record
        };
        navigator.clipboard.writeText(JSON.stringify(data));
    }
    return 0;
}

/************************************************************
 *  From internal form button
 ************************************************************/
function ac_paste_record(gobj, event, kw, src)
{
    let $container = gobj_read_attr(gobj, "$container");
    let $form = $container.querySelector('form');

    let record = kw.record;
    if(is_object(record)) {
        gobj_write_bool_attr(gobj, "initialized", true);
        set_form_values(gobj, gobj_read_attr(gobj, "template"), $form, record);
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
 *
 ************************************************************/
function ac_show(gobj, event, kw, src)
{

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
            ["EV_LOAD_RECORD",          ac_load_record,         null],
            ["EV_SAVE_RECORD",          ac_save_record,         null],
            ["EV_UNDO_RECORD",          ac_undo_record,         null],
            ["EV_CLEAR_RECORD",         ac_clear_record,        null],
            ["EV_ADD_TABLE_ROW",        ac_add_table_row,       null],
            ["EV_RECORD_CHANGED",       ac_record_changed,      null],
            ["EV_COPY_RECORD",          ac_copy_record,         null],
            ["EV_PASTE_RECORD",         ac_paste_record,        null],

            ["EV_WINDOW_MOVED",         null,                   null],
            ["EV_WINDOW_RESIZED",       null,                   null],
            ["EV_WINDOW_TO_CLOSE",      ac_window_to_close,     null],
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
        ["EV_LOAD_RECORD",          0],
        ["EV_SAVE_RECORD",          event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_UNDO_RECORD",          0],
        ["EV_CLEAR_RECORD",         0],
        ["EV_ADD_TABLE_ROW",        0],
        ["EV_RECORD_CHANGED",       0],
        ["EV_COPY_RECORD",          0],
        ["EV_PASTE_RECORD",         0],

        ["EV_WINDOW_MOVED",         0],
        ["EV_WINDOW_RESIZED",       0],
        ["EV_WINDOW_TO_CLOSE",      0],
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
function register_c_yui_form()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_form };
