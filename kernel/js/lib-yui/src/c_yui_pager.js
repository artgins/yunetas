/***********************************************************************
 *          c_yui_pager.js
 *
 *  Drill-down navigation stack (Pattern A).
 *
 *  Container-agnostic: the gclass owns ONLY the navigation chrome
 *  (a "<- title" header + a body that stacks panels).  The parent
 *  mounts `gobj_read_attr(pager, "$container")` wherever it wants
 *  (inside a C_YUI_WINDOW body, a Bulma modal-card, or inline) and
 *  feeds content with EV_PUSH_PAGE.
 *
 *  No Confirm/Cancel chrome on purpose: changes are auto-saved by
 *  the content panel itself (e.g. C_YUI_FORM).  "<- back" only
 *  navigates; popping past the root emits EV_PAGER_EXIT so the host
 *  can close.  An optional per-page "discard" affordance emits
 *  EV_PAGE_DISCARD; the page decides what discard means.
 *
 *  A page `content` may be:
 *      - a gobj exposing a "$container" attr (its DOM is reused), or
 *      - an HTMLElement, or
 *      - a createElement2() spec (array).
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
    pager_header_model,
    pager_back_action,
} from "./pager_helpers.js";


/***************************************************************
 *              Constants
 ***************************************************************/
const GCLASS_NAME = "C_YUI_PAGER";

/***************************************************************
 *              Data
 ***************************************************************/
const attrs_table = [
SDATA(data_type_t.DTP_POINTER,  "subscriber",   0,  null,   "Subscriber of output events"),

SDATA(data_type_t.DTP_STRING,   "root_title",   0,  "",     "Title shown on the root page"),
SDATA(data_type_t.DTP_BOOLEAN,  "back_on_root", 0,  true,   "On the root page the back affordance emits EV_PAGER_EXIT"),
SDATA(data_type_t.DTP_BOOLEAN,  "with_discard", 0,  false,  "Enable the per-page 'discard' affordance"),

/*---------------- UI ----------------*/
SDATA(data_type_t.DTP_POINTER,  "$container",   0,  null,   "HTMLElement root, mounted by the parent"),
SDATA_END()
];

let PRIVATE_DATA = {
    stack: null
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

    priv.stack = [];

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
        ['div', {class: 'yui-pager', style: 'height:100%; display:flex; flex-direction:column;'}, [
            ['div', {class: 'yui-pager-header is-flex is-align-items-center is-flex-grow-0',
                     style: 'gap:.25rem; padding:.25rem .25rem;'}, [
                ['button', {class: 'yui-pager-back button is-white is-hidden', 'aria-label': 'back'}, [
                    ['span', {class: 'icon'}, ['i', {class: 'yi-arrow-left'}]]
                ], {
                    click: function(evt) {
                        evt.stopPropagation();
                        gobj_send_event(gobj, "EV_POP_PAGE", {}, gobj);
                    }
                }],
                ['span', {class: 'yui-pager-title is-flex-grow-1', style: 'font-weight:600;'}, ''],
                ['button', {class: 'yui-pager-discard button is-white is-hidden', 'aria-label': 'discard'}, [
                    ['span', {class: 'icon'}, ['i', {class: 'yi-broom'}]],
                    ['span', {class: 'is-hidden-mobile', i18n: 'discard'}, 'discard']
                ], {
                    click: function(evt) {
                        evt.stopPropagation();
                        gobj_send_event(gobj, "EV_DISCARD_PAGE", {}, gobj);
                    }
                }]
            ]],
            ['div', {class: 'yui-pager-body is-flex-grow-1',
                     style: 'flex:1 1 auto; min-height:0; position:relative; overflow:auto;'}, []
            ]
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
 *   Resolve a page `content` into { $el, gobj }
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
 *   Re-render the header chrome from the current stack
 ************************************************************/
function render_header(gobj)
{
    let priv = gobj.priv;
    let proj = priv.stack.map(function(e) {
        return { id: e.id, title: e.title, discardable: e.discardable };
    });
    let model = pager_header_model(proj, {
        root_title:   gobj_read_attr(gobj, "root_title"),
        back_on_root: gobj_read_attr(gobj, "back_on_root"),
        with_discard: gobj_read_attr(gobj, "with_discard"),
    });

    let $container = gobj_read_attr(gobj, "$container");
    let $back    = $container.querySelector('.yui-pager-back');
    let $title   = $container.querySelector('.yui-pager-title');
    let $discard = $container.querySelector('.yui-pager-discard');

    $back.classList.toggle('is-hidden', !model.show_back);
    /*
     *  Root (back_on_root) shows a close cross INSIDE the popup;
     *  deeper pages show a back arrow.
     */
    let $back_icon = $back.querySelector('i');
    if(model.back_kind === "close") {
        $back_icon.className = 'yi-xmark';
        $back.setAttribute('aria-label', 'close');
    } else {
        $back_icon.className = 'yi-arrow-left';
        $back.setAttribute('aria-label', 'back');
    }
    $discard.classList.toggle('is-hidden', !model.show_discard);
    $title.setAttribute('i18n', model.title);
    $title.textContent = model.title;
}

/************************************************************
 *   Forward an event to the current top page (if it is a gobj)
 ************************************************************/
function forward_top(gobj, event, kw)
{
    let priv = gobj.priv;
    let top = priv.stack[priv.stack.length - 1];
    if(top && top.gobj) {
        gobj_send_event(top.gobj, event, kw || {}, gobj);
    }
}




                    /***************************
                     *      Actions
                     ***************************/




/************************************************************
 *   EV_PUSH_PAGE { id, title, content, discardable? }
 ************************************************************/
function ac_push_page(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let resolved = resolve_panel(kw.content);
    if(!resolved.$el) {
        log_error(`${GCLASS_NAME}: EV_PUSH_PAGE without content`);
        return -1;
    }

    let $body = gobj_read_attr(gobj, "$container").querySelector('.yui-pager-body');

    let cur = priv.stack[priv.stack.length - 1];
    if(cur && cur.$el) {
        cur.$el.classList.add('is-hidden');
    }

    resolved.$el.classList.remove('is-hidden');
    if(resolved.$el.parentNode !== $body) {
        $body.appendChild(resolved.$el);
    }

    priv.stack.push({
        id:          kw.id || "",
        title:       kw.title || "",
        discardable: !!kw.discardable,
        $el:         resolved.$el,
        gobj:        resolved.gobj,
    });

    render_header(gobj);
    gobj_publish_event(gobj, "EV_PAGE_SHOWN", {id: kw.id || "", depth: priv.stack.length});
    return 0;
}

/************************************************************
 *   EV_POP_PAGE / EV_BACK
 ************************************************************/
function ac_pop_page(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let proj = priv.stack.map(function(e) {
        return { id: e.id };
    });
    let action = pager_back_action(proj, gobj_read_attr(gobj, "back_on_root"));

    if(action.type === "noop") {
        return 0;
    }
    if(action.type === "exit") {
        gobj_publish_event(gobj, "EV_PAGER_EXIT", {});
        return 0;
    }

    /*
     *  pop: detach the top panel (do NOT destroy it, the parent owns it)
     */
    let popped = priv.stack.pop();
    if(popped && popped.$el && popped.$el.parentNode) {
        popped.$el.parentNode.removeChild(popped.$el);
    }

    let top = priv.stack[priv.stack.length - 1];
    if(top && top.$el) {
        top.$el.classList.remove('is-hidden');
    }

    render_header(gobj);
    gobj_publish_event(gobj, "EV_PAGE_SHOWN", {
        id:    top ? top.id : "",
        depth: priv.stack.length,
    });
    return 0;
}

/************************************************************
 *   EV_REPLACE_PAGE { id, title, content, discardable? }
 ************************************************************/
function ac_replace_page(gobj, event, kw, src)
{
    let priv = gobj.priv;

    let resolved = resolve_panel(kw.content);
    if(!resolved.$el) {
        log_error(`${GCLASS_NAME}: EV_REPLACE_PAGE without content`);
        return -1;
    }

    let $body = gobj_read_attr(gobj, "$container").querySelector('.yui-pager-body');

    let old = priv.stack.pop();
    if(old && old.$el && old.$el.parentNode) {
        old.$el.parentNode.removeChild(old.$el);
    }

    resolved.$el.classList.remove('is-hidden');
    if(resolved.$el.parentNode !== $body) {
        $body.appendChild(resolved.$el);
    }

    priv.stack.push({
        id:          kw.id || "",
        title:       kw.title || "",
        discardable: !!kw.discardable,
        $el:         resolved.$el,
        gobj:        resolved.gobj,
    });

    render_header(gobj);
    gobj_publish_event(gobj, "EV_PAGE_SHOWN", {id: kw.id || "", depth: priv.stack.length});
    return 0;
}

/************************************************************
 *   EV_DISCARD_PAGE — emit EV_PAGE_DISCARD for the top page
 ************************************************************/
function ac_discard_page(gobj, event, kw, src)
{
    let priv = gobj.priv;
    let top = priv.stack[priv.stack.length - 1];
    if(top) {
        gobj_publish_event(gobj, "EV_PAGE_DISCARD", {id: top.id});
    }
    return 0;
}

/************************************************************
 *   EV_SHOW — parent (routing/host) shows us
 ************************************************************/
function ac_show(gobj, event, kw, src)
{
    let $container = gobj_read_attr(gobj, "$container");
    if($container) {
        $container.classList.remove('is-hidden');
    }
    forward_top(gobj, "EV_SHOW", kw);
    return 0;
}

/************************************************************
 *   EV_HIDE — parent (routing/host) hides us
 ************************************************************/
function ac_hide(gobj, event, kw, src)
{
    let $container = gobj_read_attr(gobj, "$container");
    if($container) {
        $container.classList.add('is-hidden');
    }
    forward_top(gobj, "EV_HIDE", {});
    return 0;
}

/************************************************************
 *   EV_REFRESH — re-render chrome and forward to the top page
 ************************************************************/
function ac_refresh(gobj, event, kw, src)
{
    render_header(gobj);
    forward_top(gobj, "EV_REFRESH", kw);
    return 0;
}

/************************************************************
 *   EV_RESIZE — forward to the top page
 ************************************************************/
function ac_resize(gobj, event, kw, src)
{
    forward_top(gobj, "EV_RESIZE", kw);
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
            ["EV_PUSH_PAGE",            ac_push_page,           null],
            ["EV_POP_PAGE",             ac_pop_page,            null],
            ["EV_BACK",                 ac_pop_page,            null],
            ["EV_REPLACE_PAGE",         ac_replace_page,        null],
            ["EV_DISCARD_PAGE",         ac_discard_page,        null],
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
        ["EV_PUSH_PAGE",            0],
        ["EV_POP_PAGE",             0],
        ["EV_BACK",                 0],
        ["EV_REPLACE_PAGE",         0],
        ["EV_DISCARD_PAGE",         0],
        ["EV_SHOW",                 0],
        ["EV_HIDE",                 0],
        ["EV_REFRESH",              0],
        ["EV_RESIZE",               0],
        ["EV_PAGE_SHOWN",           event_flag_t.EVF_OUTPUT_EVENT|event_flag_t.EVF_NO_WARN_SUBS],
        ["EV_PAGE_DISCARD",         event_flag_t.EVF_OUTPUT_EVENT],
        ["EV_PAGER_EXIT",           event_flag_t.EVF_OUTPUT_EVENT]
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
function register_c_yui_pager()
{
    return create_gclass(GCLASS_NAME);
}

export { register_c_yui_pager };
