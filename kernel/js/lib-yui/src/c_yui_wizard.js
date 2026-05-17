/***********************************************************************
 *          c_yui_wizard.js
 *
 *  Multi-step wizard (Pattern B).
 *
 *  Container-agnostic: the gclass owns ONLY the navigation chrome
 *  (title + "N / M" + a body that shows one step + a footer with
 *  Back / Next, and Confirm on the last step).  The parent mounts
 *  `gobj_read_attr(wizard, "$container")` wherever it wants and
 *  supplies the ordered steps up-front with EV_SET_STEPS.
 *
 *  Unlike C_YUI_PAGER, a wizard DOES confirm at the end.  In a
 *  `linear` wizard each step's content gobj is asked to validate
 *  before advancing: the wizard sends EV_STEP_VALIDATE and the
 *  step answers EV_STEP_VALID { ...kw } or EV_STEP_INVALID
 *  { msg }.  Validated kw is accumulated and delivered merged in
 *  EV_WIZARD_DONE.  Steps that are plain elements (no gobj) and
 *  non-linear wizards skip validation.
 *
 *  A step `content` may be a gobj exposing a "$container" attr,
 *  an HTMLElement, or a createElement2() spec (array).
 *
 *          Copyright (c) 2026, ArtGins.
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
    gobj_write_attr,
    gobj_read_attr,
    gobj_send_event,
    gobj_publish_event,
    gobj_stop_children,
} from "@yuneta/gobj-js";

import {
    wizard_step_model,
    wizard_next_index,
    wizard_prev_index,
    wizard_clamp_index,
    wizard_accumulate,
    wizard_should_validate,
} from "./wizard_helpers.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_WIZARD";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",   0,  null,       "Subscriber of output events"),

SDATA(data_type_t.DTP_STRING,   "confirm_label",0,  "confirm",  "i18n key of the last-step primary button"),
SDATA(data_type_t.DTP_STRING,   "next_label",   0,  "next",     "i18n key of the primary button (not last step)"),
SDATA(data_type_t.DTP_STRING,   "back_label",   0,  "back",     "i18n key of the back button"),
SDATA(data_type_t.DTP_BOOLEAN,  "allow_back",   0,  true,       "Allow going back to previous steps"),
SDATA(data_type_t.DTP_BOOLEAN,  "linear",       0,  true,       "Each step's gobj must validate before advancing"),

/*---------------- UI ----------------*/
SDATA(data_type_t.DTP_POINTER,  "$container",   0,  null,       "HTMLElement root, mounted by the parent"),
SDATA_END()
];

let PRIVATE_DATA = {
    steps:    null,
    idx:      0,
    acc:      null,
    awaiting: false
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

    priv.steps    = [];
    priv.idx      = 0;
    priv.acc      = null;
    priv.awaiting = false;

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
}

/***************************************************************
 *          Framework Method: Stop
 ***************************************************************/
function mt_stop(gobj)
{
    gobj_stop_children(gobj);
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
    let $container = createElement2(
        ['div', {class: 'yui-wizard', style: 'height:100%; display:flex; flex-direction:column;'}, [
            ['div', {class: 'yui-wizard-header is-flex is-align-items-center is-flex-grow-0',
                     style: 'gap:.5rem; padding:.25rem .5rem;'}, [
                ['span', {class: 'yui-wizard-title is-flex-grow-1', style: 'font-weight:600;'}, ''],
                ['span', {class: 'yui-wizard-counter has-text-weight-semibold', style: 'opacity:.7;'}, '']
            ]],
            ['div', {class: 'yui-wizard-body is-flex-grow-1',
                     style: 'flex:1 1 auto; min-height:0; position:relative; overflow:auto;'}, []
            ],
            ['div', {class: 'yui-wizard-footer is-flex is-justify-content-space-between is-flex-grow-0',
                     style: 'gap:.5rem; padding:.5rem;'}, [
                ['button', {class: 'yui-wizard-back button', 'aria-label': 'back'}, [
                    ['span', {class: 'icon'}, ['i', {class: 'yi-arrow-left'}]],
                    ['span', {class: 'is-hidden-mobile', i18n: 'back'}, 'back']
                ], {
                    click: function(evt) {
                        evt.stopPropagation();
                        gobj_send_event(gobj, "EV_PREV", {}, gobj);
                    }
                }],
                ['button', {class: 'yui-wizard-primary button is-link', 'aria-label': 'next'}, [
                    ['span', {class: 'yui-wizard-primary-label', i18n: 'next'}, 'next']
                ], {
                    click: function(evt) {
                        evt.stopPropagation();
                        gobj_send_event(gobj, "EV_NEXT", {}, gobj);
                    }
                }]
            ]]
        ]]
    );
    gobj_write_attr(gobj, "$container", $container);
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
 *   Resolve a step `content` into { $el, gobj }
 ************************************************************/
function resolve_panel(content)
{
    if(content instanceof HTMLElement) {
        return { $el: content, gobj: null };
    }
    if(Array.isArray(content)) {
        return { $el: createElement2(content), gobj: null };
    }
    /*
     *  Assume a gobj exposing a "$container" attr
     */
    let $el = gobj_read_attr(content, "$container");
    return { $el: $el, gobj: content };
}

/************************************************************
 *   Re-render the chrome (title / counter / buttons) and
 *   show only the current step.
 ************************************************************/
function render_chrome(gobj)
{
    let priv = gobj.priv;
    let proj = priv.steps.map(function(s) {
        return { id: s.id, title: s.title };
    });
    let model = wizard_step_model(proj, priv.idx, {
        allow_back:    gobj_read_attr(gobj, "allow_back"),
        confirm_label: gobj_read_attr(gobj, "confirm_label"),
        next_label:    gobj_read_attr(gobj, "next_label"),
    });

    let $container = gobj_read_attr(gobj, "$container");
    let $title   = $container.querySelector('.yui-wizard-title');
    let $counter = $container.querySelector('.yui-wizard-counter');
    let $back    = $container.querySelector('.yui-wizard-back');
    let $plabel  = $container.querySelector('.yui-wizard-primary-label');

    $title.setAttribute('i18n', model.title);
    $title.textContent = model.title;

    if(model.count > 0) {
        $counter.textContent = `${model.idx + 1} / ${model.count}`;
    } else {
        $counter.textContent = "";
    }

    $back.classList.toggle('is-hidden', !model.show_back);

    $plabel.setAttribute('i18n', model.primary_label);
    $plabel.textContent = model.primary_label;

    /*
     *  Show only the current step
     */
    for(let i = 0; i < priv.steps.length; i++) {
        let s = priv.steps[i];
        if(s.$el) {
            s.$el.classList.toggle('is-hidden', i !== model.idx);
        }
    }
}

/************************************************************
 *   Forward an event to the current step (if it is a gobj)
 ************************************************************/
function forward_current(gobj, event, kw)
{
    let priv = gobj.priv;
    let s = priv.steps[priv.idx];
    if(s && s.gobj) {
        gobj_send_event(s.gobj, event, kw || {}, gobj);
    }
}

/************************************************************
 *   Accumulate the current step's kw, then advance or finish
 ************************************************************/
function advance(gobj, kw)
{
    let priv = gobj.priv;
    let cur = priv.steps[priv.idx];

    priv.acc = wizard_accumulate(priv.acc, cur ? cur.id : "", kw);

    let proj = priv.steps.map(function(s) {
        return { id: s.id, title: s.title };
    });
    let model = wizard_step_model(proj, priv.idx, {
        allow_back:    gobj_read_attr(gobj, "allow_back"),
        confirm_label: gobj_read_attr(gobj, "confirm_label"),
        next_label:    gobj_read_attr(gobj, "next_label"),
    });

    if(model.is_last) {
        gobj_publish_event(gobj, "EV_WIZARD_DONE", {
            kw:      priv.acc.merged,
            by_step: priv.acc.by_step,
        });
        return;
    }

    priv.idx = wizard_next_index(priv.idx, priv.steps.length);
    render_chrome(gobj);
    let now = priv.steps[priv.idx];
    gobj_publish_event(gobj, "EV_STEP_SHOWN", {
        idx: priv.idx,
        id:  now ? now.id : "",
    });
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *   EV_SET_STEPS { steps: [ { id, title, content } ] }
 ************************************************************/
function ac_set_steps(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let steps_in = (kw && kw.steps) || [];
    if(steps_in.length === 0) {
        log_error(`${GCLASS_NAME}: EV_SET_STEPS with no steps`);
        return -1;
    }

    let $body = gobj_read_attr(gobj, "$container").querySelector('.yui-wizard-body');
    $body.innerHTML = "";

    let steps = [];
    for(let i = 0; i < steps_in.length; i++) {
        let st = steps_in[i];
        let resolved = resolve_panel(st.content);
        if(!resolved.$el) {
            log_error(`${GCLASS_NAME}: step '${st.id}' has no content`);
            return -1;
        }
        resolved.$el.classList.add('is-hidden');
        $body.appendChild(resolved.$el);
        steps.push({
            id:    st.id || "",
            title: st.title || "",
            $el:   resolved.$el,
            gobj:  resolved.gobj,
        });
    }

    priv.steps    = steps;
    priv.idx      = 0;
    priv.acc      = null;
    priv.awaiting = false;

    render_chrome(gobj);
    let now = priv.steps[0];
    gobj_publish_event(gobj, "EV_STEP_SHOWN", {idx: 0, id: now ? now.id : ""});
    return 0;
}

/************************************************************
 *   EV_NEXT — validate (if linear) then advance / confirm
 ************************************************************/
function ac_next(gobj, event, kw, src)
{
    let priv = gobj.priv;
    if(priv.steps.length === 0) {
        return 0;
    }

    let cur = priv.steps[priv.idx];
    let linear = gobj_read_attr(gobj, "linear");

    if(wizard_should_validate(linear, !!(cur && cur.gobj))) {
        if(priv.awaiting) {
            return 0;
        }
        priv.awaiting = true;
        gobj_send_event(cur.gobj, "EV_STEP_VALIDATE", {}, gobj);
        return 0;
    }

    advance(gobj, {});
    return 0;
}

/************************************************************
 *   EV_PREV — go to the previous step
 ************************************************************/
function ac_prev(gobj, event, kw, src)
{
    let priv = gobj.priv;
    if(priv.steps.length === 0) {
        return 0;
    }
    priv.awaiting = false;
    priv.idx = wizard_prev_index(priv.idx, priv.steps.length);
    render_chrome(gobj);
    let now = priv.steps[priv.idx];
    gobj_publish_event(gobj, "EV_STEP_SHOWN", {idx: priv.idx, id: now ? now.id : ""});
    return 0;
}

/************************************************************
 *   EV_GOTO { idx } — jump to a step (no validation)
 ************************************************************/
function ac_goto(gobj, event, kw, src)
{
    let priv = gobj.priv;
    if(priv.steps.length === 0) {
        return 0;
    }
    priv.awaiting = false;
    priv.idx = wizard_clamp_index((kw && kw.idx) || 0, priv.steps.length);
    render_chrome(gobj);
    let now = priv.steps[priv.idx];
    gobj_publish_event(gobj, "EV_STEP_SHOWN", {idx: priv.idx, id: now ? now.id : ""});
    return 0;
}

/************************************************************
 *   EV_STEP_VALID { ...kw } — step accepted, accumulate+advance
 ************************************************************/
function ac_step_valid(gobj, event, kw, src)
{
    let priv = gobj.priv;
    if(!priv.awaiting) {
        return 0;
    }
    priv.awaiting = false;
    advance(gobj, kw || {});
    return 0;
}

/************************************************************
 *   EV_STEP_INVALID { msg } — step rejected, stay put
 *   (the step shows its own error)
 ************************************************************/
function ac_step_invalid(gobj, event, kw, src)
{
    let priv = gobj.priv;
    priv.awaiting = false;
    return 0;
}

/************************************************************
 *   EV_CANCEL — host asked to cancel the wizard
 ************************************************************/
function ac_cancel(gobj, event, kw, src)
{
    gobj_publish_event(gobj, "EV_WIZARD_CANCEL", {});
    return 0;
}

/************************************************************
 *   EV_SHOW
 ************************************************************/
function ac_show(gobj, event, kw, src)
{
    let $container = gobj_read_attr(gobj, "$container");
    if($container) {
        $container.classList.remove('is-hidden');
    }
    forward_current(gobj, "EV_SHOW", kw);
    return 0;
}

/************************************************************
 *   EV_HIDE
 ************************************************************/
function ac_hide(gobj, event, kw, src)
{
    let $container = gobj_read_attr(gobj, "$container");
    if($container) {
        $container.classList.add('is-hidden');
    }
    forward_current(gobj, "EV_HIDE", {});
    return 0;
}

/************************************************************
 *   EV_REFRESH
 ************************************************************/
function ac_refresh(gobj, event, kw, src)
{
    render_chrome(gobj);
    forward_current(gobj, "EV_REFRESH", kw);
    return 0;
}

/************************************************************
 *   EV_RESIZE
 ************************************************************/
function ac_resize(gobj, event, kw, src)
{
    forward_current(gobj, "EV_RESIZE", kw);
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
    mt_destroy: mt_destroy,
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
            ["EV_SET_STEPS",            ac_set_steps,           null],
            ["EV_NEXT",                 ac_next,                null],
            ["EV_PREV",                 ac_prev,                null],
            ["EV_GOTO",                 ac_goto,                null],
            ["EV_STEP_VALID",           ac_step_valid,          null],
            ["EV_STEP_INVALID",         ac_step_invalid,        null],
            ["EV_CANCEL",               ac_cancel,              null],
            ["EV_SHOW",                 ac_show,                null],
            ["EV_HIDE",                 ac_hide,                null],
            ["EV_REFRESH",              ac_refresh,             null],
            ["EV_RESIZE",               ac_resize,              null]
        ]]
    ];

    /*---------------------------------------------*
     *          Events
     *---------------------------------------------*/
    const event_types = [
        ["EV_SET_STEPS",            0],
        ["EV_NEXT",                 0],
        ["EV_PREV",                 0],
        ["EV_GOTO",                 0],
        ["EV_STEP_VALID",           0],
        ["EV_STEP_INVALID",         0],
        ["EV_CANCEL",               0],
        ["EV_SHOW",                 0],
        ["EV_HIDE",                 0],
        ["EV_REFRESH",              0],
        ["EV_RESIZE",               0],
        ["EV_STEP_SHOWN",           event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
        ["EV_WIZARD_DONE",          event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_WIZARD_CANCEL",        event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_STEP_VALIDATE",        event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS]
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
function register_c_yui_wizard()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_wizard };
