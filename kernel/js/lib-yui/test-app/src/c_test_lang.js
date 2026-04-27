/***********************************************************************
 *          c_test_lang.js
 *
 *      C_TEST_LANG — Language toggle controller for the test app.
 *      Subscribes to the shell's `EV_TOGGLE_LANGUAGE` (published by the
 *      toolbar item with `action.type:"event"`), flips between two
 *      trivial dictionaries on each click and refreshes the DOM via
 *      the canonical `refresh_language(element, t)` helper from
 *      `@yuneta/gobj-js`.
 *
 *      This is the same pattern `c_yui_main.js` uses in its
 *      `change_language()` function: every translatable text node is
 *      tagged at render time with `data-i18n="<canonical key>"` and a
 *      single DOM walk replaces every text node by `t(key)`.  No DOM
 *      rebuild, no shell-specific helper.
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
    refresh_language,
} from "@yuneta/gobj-js";

import {
    yui_shell_show_info,
    yui_shell_confirm_yesno,
} from "@yuneta/lib-yui";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_TEST_LANG";

/*  Two minimal dictionaries — keys are the canonical English label
 *  strings the test app uses in its app_config*.json.  English is
 *  the identity dictionary; anything not in the Spanish map falls
 *  through to the canonical key. */
const DICT_EN = {};
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
    "Hello":             "Hola",
    "Ask":               "Preguntar",
    "Open menu":         "Abrir menú",
    "Open quick menu":   "Abrir menú rápido",
    "ES/EN":             "EN/ES",
    "App toolbar":       "Barra de herramientas",

    /*  Strings that show up inside modals/toasts the test-app fires
     *  from C_TEST_LANG.ac_show_hello / ac_ask_question.  Helpers are
     *  passed `opts.t` so they render in the active language at
     *  open time AND still respond to refresh_language afterwards. */
    "Hello from the shell!":             "¡Hola desde el shell!",
    "A small smoke check":               "Una pequeña comprobación",
    "Is the new shell working as expected?":
        "¿Funciona el nuevo shell como esperabas?",
    "Glad to hear it!":                  "¡Me alegro!",
    "Noted — check the console.":        "Tomo nota — revisa la consola.",

    /*  Default modal button labels (Yes/No/Cancel/OK).  The shell
     *  helpers render these literally; refresh_language picks them
     *  up via `data-i18n="<canonical key>"`. */
    "Yes":    "Sí",
    "No":     "No",
    "Cancel": "Cancelar",
    "OK":     "Aceptar"
};


/***************************************************************
 *              Attrs
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",  0,  null,  "Subscriber of output events"),

SDATA(data_type_t.DTP_POINTER,  "shell",       0,  null,  "C_YUI_SHELL gobj whose DOM we refresh"),
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
        gobj_subscribe_event(shell, "EV_SHOW_HELLO",      {}, gobj);
        gobj_subscribe_event(shell, "EV_ASK_QUESTION",    {}, gobj);
    } catch(e) {
        log_error(`C_TEST_LANG: subscribe to shell events failed: ${e}`);
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
 *  mapped value when present, falls back to the input key (so
 *  untranslated labels still display).
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
 *  walk the shell's `$container` calling refresh_language with
 *  the matching dictionary.  Same flow `c_yui_main.js` uses for
 *  its language switch.
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
    let t = make_translator(dict);
    let $container = gobj_read_attr(shell, "$container") || document;
    refresh_language($container, t);
    return 0;
}

/*  Build a translator for the active language.  Used both by
 *  ac_toggle_language (to refresh the whole DOM) and by the
 *  show_hello / ask_question handlers (to render their modals in
 *  the current language at open time, not the canonical English
 *  key). */
function current_translator(gobj)
{
    let is_es = gobj_read_attr(gobj, "is_es");
    return make_translator(is_es ? DICT_ES : DICT_EN);
}

/***************************************************************
 *  Toolbar fired EV_SHOW_HELLO — paints a Bulma .notification
 *  via the shell helper, no view code involved.  Smoke test for
 *  yui_shell_show_info.
 ***************************************************************/
function ac_show_hello(gobj, event, kw, src)
{
    let shell = gobj_read_pointer_attr(gobj, "shell");
    if(!shell) {
        return 0;
    }
    yui_shell_show_info(shell, "Hello from the shell!", {
        t: current_translator(gobj)
    });
    return 0;
}

/***************************************************************
 *  Toolbar fired EV_ASK_QUESTION — opens a yes/no dialog and
 *  reports the answer via a follow-up info toast.  Smoke test
 *  for yui_shell_confirm_yesno + Escape stack.
 ***************************************************************/
function ac_ask_question(gobj, event, kw, src)
{
    let shell = gobj_read_pointer_attr(gobj, "shell");
    if(!shell) {
        return 0;
    }
    let t = current_translator(gobj);
    yui_shell_confirm_yesno(
        shell,
        "Is the new shell working as expected?",
        { title: "A small smoke check", t: t }
    ).then(answer => {
        let msg = answer ? "Glad to hear it!" : "Noted — check the console.";
        yui_shell_show_info(shell, msg, { t: current_translator(gobj) });
    });
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
            ["EV_TOGGLE_LANGUAGE", ac_toggle_language, null],
            ["EV_SHOW_HELLO",      ac_show_hello,      null],
            ["EV_ASK_QUESTION",    ac_ask_question,    null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_TOGGLE_LANGUAGE", 0],
        ["EV_SHOW_HELLO",      0],
        ["EV_ASK_QUESTION",    0]
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
