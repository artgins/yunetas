/***********************************************************************
 *          c_test_lang.js
 *
 *      C_TEST_LANG — Language toggle controller for the test app.
 *      Subscribes to the shell's `EV_TOGGLE_LANGUAGE` (published by the
 *      toolbar item with `action.type:"event"`), flips between two
 *      trivial dictionaries on each click, and applies the new
 *      resolver via `yui_shell_set_translate`.
 *
 *      This is the canonical pattern for any app that needs to react
 *      to custom toolbar events: a small CHILD gobj that declares the
 *      event in its FSM and subscribes to the shell.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA, SDATA_END, data_type_t, event_flag_t,
    gclass_create, log_error,
    gobj_parent,
    gobj_read_attr, gobj_read_pointer_attr, gobj_write_attr,
    gobj_subscribe_event,
} from "@yuneta/gobj-js";

import {
    yui_shell_set_translate,
} from "@yuneta/lib-yui";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_TEST_LANG";

/*  Two minimal dictionaries — keys are the canonical English label
 *  strings the test-app uses in its app_config*.json.
 *
 *  Real apps would resolve via i18next / similar; here we keep it
 *  inline so the harness has no extra dependency. */
const DICT_EN = {};   /*  Identity for English. */
const DICT_ES = {
    "Dashboard":         "Panel",
    "Overview":          "Resumen",
    "Devices":           "Dispositivos",
    "Alerts":            "Alertas",
    "Reports":           "Informes",
    "Daily":             "Diario",
    "Monthly":           "Mensual",
    "Settings":          "Ajustes",
    "Help":              "Ayuda",
    "Help (eager)":      "Ayuda (eager)",
    "Daily reports":     "Informes diarios",
    "Monthly reports":   "Informes mensuales",
    "Quick: Overview":   "Rápido: Resumen",
    "Quick: Alerts":     "Rápido: Alertas",
    "Quick: Settings":   "Rápido: Ajustes",
    "Home":              "Inicio",
    "Open menu":         "Abrir menú",
    "Open quick menu":   "Abrir menú rápido",
    "ES/EN":             "EN/ES",
    "App toolbar":       "Barra de herramientas",
};


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",  0,  null,  "Subscriber of output events"),

SDATA(data_type_t.DTP_POINTER,  "shell",       0,  null,  "C_YUI_SHELL gobj to drive translate on"),
SDATA(data_type_t.DTP_BOOLEAN,  "is_es",       0,  false, "Current language flag (true = ES, false = EN)"),
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
}

/***************************************************************
 *          Framework Method: Start
 ***************************************************************/
function mt_start(gobj)
{
    let shell = gobj_read_pointer_attr(gobj, "shell");
    if(!shell) {
        log_error("C_TEST_LANG: shell attr not set, language toggle disabled");
        return;
    }
    /*  Subscribe to the toolbar's user-defined event.  The shell is
     *  built with gcflag_no_check_output_events so it forwards any
     *  event name from the JSON without us having to extend its
     *  event_types table. */
    try {
        gobj_subscribe_event(shell, "EV_TOGGLE_LANGUAGE", {}, gobj);
    } catch(e) {
        log_error(`C_TEST_LANG: subscribe to shell EV_TOGGLE_LANGUAGE failed: ${e}`);
    }
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
}




                    /***************************
                     *      Local Methods
                     ***************************/




/***************************************************************
 *  Build a translate function backed by `dict`: returns the
 *  mapped value when present, falls back to the input key
 *  (so untranslated labels still display).
 ***************************************************************/
function make_translator(dict)
{
    return function(key) {
        if(typeof key !== "string") {
            return key;
        }
        if(Object.prototype.hasOwnProperty.call(dict, key)) {
            return dict[key];
        }
        return key;
    };
}




                    /***************************
                     *      Actions
                     ***************************/




/***************************************************************
 *  Toolbar fired EV_TOGGLE_LANGUAGE — flip the language flag and
 *  hot-swap the shell's translate hook.  The shell takes care of
 *  rebuilding every nav and the toolbar.
 ***************************************************************/
function ac_toggle_language(gobj, event, kw, src)
{
    let shell = gobj_read_pointer_attr(gobj, "shell");
    if(!shell) {
        return 0;
    }
    let is_es = !gobj_read_attr(gobj, "is_es");
    gobj_write_attr(gobj, "is_es", is_es);
    let dict = is_es ? DICT_ES : DICT_EN;
    yui_shell_set_translate(shell, make_translator(dict));
    return 0;
}




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
        ["ST_IDLE", [
            ["EV_TOGGLE_LANGUAGE", ac_toggle_language, null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_TOGGLE_LANGUAGE", 0]
    ];

    __gclass__ = gclass_create(
        gclass_name,
        event_types,
        states,
        gmt,
        0,                          /* lmt */
        attrs_table,
        PRIVATE_DATA,
        0,                          /* authz_table */
        0,                          /* command_table */
        0,                          /* s_user_trace_level */
        0                           /* gclass_flag */
    );

    if(!__gclass__) {
        return -1;
    }

    return 0;
}

/***************************************************************
 *          Register GClass
 ***************************************************************/
function register_c_test_lang()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_test_lang };
