/***********************************************************************
 *          demos.js
 *
 *      The three navigation mechanisms, running — each one a real
 *      GCLASS on the real `@yuneta/gobj-js` runtime (the 7.9.4 build
 *      sitting next to this file), not a hand-rolled imitation.
 *
 *      WHY THAT MATTERS HERE: the page argues that a click IS an action
 *      and belongs in a machine. A page making that argument with three
 *      piles of DOM callbacks would be arguing against itself. So every
 *      button below sends an EVENT, every event lands in an FSM action,
 *      and the panel at the bottom of each demo shows the machine as it
 *      goes: which gobj, which event, which state.
 *
 *      HONEST NOTE ON THAT PANEL: gobj-js does not currently emit the
 *      `machine` trace — the calls exist in gobj.js but are commented
 *      out (only `trace_creation` is live), so there is nothing to
 *      subscribe to. The panel is therefore fed by `send()` below, a
 *      four-line wrapper that records the event and then delegates to
 *      `gobj_send_event`. The STATE it prints is read back from
 *      `gobj_current_state()`, so the transition shown is the one that
 *      really happened.
 *
 *      Nothing else is imported: Canvas 2D and the DOM, wrapped in
 *      gobjs. No framework, no build step, no external request.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    SDATA, SDATA_END, data_type_t,
    gclass_create, log_error,
    gobj_create, gobj_start, gobj_start_up,
    gobj_create_yuno, register_c_yuno, register_c_timer,
    gobj_send_event, gobj_current_state,
    gobj_read_attr, gobj_write_attr,
    gobj_name, gobj_gclass_name,
} from "/navigation/gobj-js.es.min.js";   /*  absolute: see index.html  */


/***************************************************************
 *              The machine panel
 ***************************************************************/
const LOG_MAX = 7;

/*
 *  One panel per demo, resolved from the demo's own container so two
 *  demos on the same page never write into each other's log.
 */
function machine_log(gobj, event, state_before)
{
    const $log = gobj.priv.$log;
    if(!$log) {
        return;
    }
    const state_after = gobj_current_state(gobj);

    const moved = (state_after !== state_before);
    const $li = document.createElement("li");
    if(moved) {
        $li.classList.add("moved");
    }
    const span = (cls, text) => {
        const $s = document.createElement("span");
        $s.className = cls;
        $s.textContent = text;
        return $s;
    };
    $li.appendChild(span("ml-gobj", gobj_gclass_name(gobj) + "^" + gobj_name(gobj)));
    $li.appendChild(span("ml-ev", event));
    $li.appendChild(span("ml-st",
        moved ? state_before + " → " + state_after : state_before));

    $log.appendChild($li);
    while($log.children.length > LOG_MAX) {
        $log.removeChild($log.firstChild);
    }
}

/*
 *  The ONLY way these demos send an event. It exists so the panel can
 *  show what the framework's `machine` trace would show if gobj-js
 *  emitted it — see the file header.
 */
function send(gobj, event, kw)
{
    const before = gobj_current_state(gobj);
    const ret = gobj_send_event(gobj, event, kw || {}, gobj);
    machine_log(gobj, event, before);
    return ret;
}

/*
 *  Small DOM helper. `spec` is [tag, attrs, children|text].
 */
function h(tag, attrs, kids)
{
    const $el = document.createElement(tag);
    for(const k in (attrs || {})) {
        const v = attrs[k];
        /*  A null/false attr is ABSENT, not present with the string
         *  "null": setAttribute("disabled", null) yields disabled="null",
         *  which is a perfectly disabled button.  */
        if(v === null || v === undefined || v === false) {
            continue;
        }
        if(k === "class") {
            $el.className = v;
        } else if(k.startsWith("on")) {
            $el.addEventListener(k.slice(2), v);
        } else {
            $el.setAttribute(k, v);
        }
    }
    if(typeof kids === "string") {
        $el.textContent = kids;
    } else if(Array.isArray(kids)) {
        kids.forEach((k) => {
            if(k) {
                $el.appendChild(k);
            }
        });
    }
    return $el;
}

/*  A demo's frame: the fake url bar, the stage, and the machine panel. */
function build_frame(gobj, url_label)
{
    const $url   = h("code", {class: "demo-url"}, "");
    const $stage = h("div", {class: "demo-stage"}, []);
    const $log   = h("ol", {class: "machine-log"}, []);

    const $root = h("div", {class: "demo"}, [
        h("div", {class: "demo-bar"}, [
            h("span", {class: "demo-bar-lbl"}, url_label),
            $url
        ]),
        $stage,
        h("div", {class: "machine"}, [
            h("span", {class: "machine-lbl"}, "machine"),
            $log
        ])
    ]);

    gobj.priv.$url = $url;
    gobj.priv.$stage = $stage;
    gobj.priv.$log = $log;
    gobj_write_attr(gobj, "$container", $root);
}




                    /******************************
                     *      1 — the url
                     ******************************/




const URL_GCLASS = "C_NAV_DEMO_URL";
const REPORTS = [
    {id: "sales",   name: "Sales",   body: "Revenue by region, this quarter."},
    {id: "costs",   name: "Costs",   body: "Where the money went."},
    {id: "traffic", name: "Traffic", body: "Requests per second, by node."}
];

let __url_gclass__ = null;

function url_mt_create(gobj)
{
    build_frame(gobj, "url");
    gobj.priv.history = ["/reports"];
    gobj.priv.pos = 0;
    url_render(gobj);
}

/*
 *  A card is an <a>: the click sends the event, it does not do the work.
 *  (The real shell needs no handler at all — the href IS the navigation,
 *  and the shell's hashchange mounts the view. Here the page's own url
 *  is off limits, so the demo keeps a history of its own.)
 */
function url_render(gobj)
{
    const priv = gobj.priv;
    const route = priv.history[priv.pos];
    priv.$url.textContent = "#" + route;
    priv.$stage.replaceChildren();

    const detail = REPORTS.find((r) => route === "/reports/" + r.id);

    if(detail) {
        priv.$stage.appendChild(h("div", {class: "demo-detail"}, [
            h("h4", {}, detail.name),
            h("p", {}, detail.body)
        ]));
    } else {
        priv.$stage.appendChild(h("div", {class: "demo-cards"},
            REPORTS.map((r) => h("a", {
                class: "demo-card",
                href: "#" + "/reports/" + r.id,
                onclick: (ev) => {
                    ev.preventDefault();
                    send(gobj, "EV_OPEN_CARD", {id: r.id});
                }
            }, r.name))
        ));
    }

    priv.$stage.appendChild(h("div", {class: "demo-hist"}, [
        h("button", {
            class: "demo-btn", type: "button",
            disabled: priv.pos > 0 ? null : "disabled",
            onclick: () => send(gobj, "EV_BACK", {})
        }, "← Back"),
        h("button", {
            class: "demo-btn", type: "button",
            disabled: priv.pos < priv.history.length - 1 ? null : "disabled",
            onclick: () => send(gobj, "EV_FORWARD", {})
        }, "Forward →"),
        h("span", {class: "demo-note"},
            "browser history: " + (priv.pos + 1) + " / " + priv.history.length)
    ]));
}

function url_ac_open_card(gobj, event, kw)
{
    const priv = gobj.priv;
    /*  A human chose it: push. Everything ahead is dropped, exactly as a
     *  browser does when you navigate away from a back-stack.  */
    priv.history = priv.history.slice(0, priv.pos + 1);
    priv.history.push("/reports/" + kw.id);
    priv.pos = priv.history.length - 1;
    url_render(gobj);
    return 0;
}

function url_ac_back(gobj)
{
    const priv = gobj.priv;
    if(priv.pos > 0) {
        priv.pos--;
    }
    url_render(gobj);
    return 0;
}

function url_ac_forward(gobj)
{
    const priv = gobj.priv;
    if(priv.pos < priv.history.length - 1) {
        priv.pos++;
    }
    url_render(gobj);
    return 0;
}

function create_url_gclass()
{
    if(__url_gclass__) {
        return 0;
    }
    const states = [
        ["ST_IDLE", [
            ["EV_OPEN_CARD", url_ac_open_card, null],
            ["EV_BACK",      url_ac_back,      null],
            ["EV_FORWARD",   url_ac_forward,   null]
        ]]
    ];
    const event_types = [
        ["EV_OPEN_CARD", 0],
        ["EV_BACK",      0],
        ["EV_FORWARD",   0]
    ];
    __url_gclass__ = gclass_create(
        URL_GCLASS, event_types, states,
        {mt_create: url_mt_create},
        0,
        [SDATA(data_type_t.DTP_POINTER, "$container", 0, null, "Root element"), SDATA_END()],
        {$url: null, $stage: null, $log: null, history: null, pos: 0},
        0, 0, 0, 0
    );
    return __url_gclass__ ? 0 : -1;
}




                    /******************************
                     *      2 — the node tree
                     ******************************/




const TREE_GCLASS = "C_NAV_DEMO_TREE";

/*  The same shape the page describes: /admin declared, the rest subpath. */
const TREE = {
    id: "admin", label: "admin", children: [
        {id: "treedb", label: "treedb", children: [
            {id: "db", label: "db", children: [
                {id: "data",  label: "data",  children: []},
                {id: "graph", label: "graph", children: []}
            ]},
            {id: "authzs", label: "authzs", children: [
                {id: "data",  label: "data",  children: []},
                {id: "graph", label: "graph", children: []}
            ]}
        ]},
        {id: "users", label: "users", children: []}
    ]
};

let __tree_gclass__ = null;

function node_at(path)
{
    let node = TREE;
    for(const seg of path) {
        const next = node.children.find((c) => c.id === seg);
        if(!next) {
            return node;
        }
        node = next;
    }
    return node;
}

function chain_of(path)
{
    const chain = [{node: TREE, route: "/admin"}];
    let node = TREE, route = "/admin";
    for(const seg of path) {
        const next = node.children.find((c) => c.id === seg);
        if(!next) {
            break;
        }
        node = next;
        route += "/" + seg;
        chain.push({node: node, route: route});
    }
    return chain;
}

function tree_mt_create(gobj)
{
    build_frame(gobj, "url");
    gobj.priv.path = [];
    gobj.priv.mode = "stack";
    tree_render(gobj);
}

function tree_render(gobj)
{
    const priv = gobj.priv;
    const chain = chain_of(priv.path);
    const tip = chain[chain.length - 1];

    priv.$url.textContent = "#" + tip.route;
    priv.$stage.replaceChildren();

    /*  The mode picker is the demo's own control, not part of the tree. */
    const $modes = h("div", {class: "demo-modes"}, [h("span", {class: "demo-note"}, "nav_mode")]);
    ["stack", "back", "path"].forEach((m) => {
        $modes.appendChild(h("button", {
            class: "demo-btn" + (priv.mode === m ? " on" : ""),
            type: "button",
            "aria-pressed": priv.mode === m ? "true" : "false",
            onclick: () => send(gobj, "EV_SET_NAV_MODE", {nav_mode: m})
        }, m));
    });
    priv.$stage.appendChild($modes);

    /*
     *  The chrome, exactly as nav_mode_renders() decides it:
     *    stack — one strip per ancestor
     *    back  — the tip's parent only, as "← parent"
     *    path  — the whole trail on one line, on the root
     */
    const $chrome = h("div", {class: "demo-chrome"}, []);
    if(priv.mode === "stack") {
        chain.slice(0, -1).forEach((step, depth) => {
            const $strip = h("div", {class: "demo-strip"}, []);
            step.node.children.forEach((c) => {
                const active = priv.path[depth] === c.id;
                $strip.appendChild(h("button", {
                    class: "demo-tab" + (active ? " on" : ""),
                    type: "button",
                    onclick: () => send(gobj, "EV_GO", {depth: depth, id: c.id})
                }, c.label));
            });
            $chrome.appendChild($strip);
        });
    } else if(priv.mode === "back") {
        if(chain.length > 1) {
            const parent = chain[chain.length - 2];
            $chrome.appendChild(h("div", {class: "demo-strip"}, [
                h("button", {
                    class: "demo-tab back", type: "button",
                    onclick: () => send(gobj, "EV_UP", {})
                }, "← " + parent.node.label)
            ]));
        }
    } else {
        const $trail = h("div", {class: "demo-strip crumb"}, []);
        chain.forEach((step, i) => {
            if(i) {
                $trail.appendChild(h("span", {class: "demo-sep"}, "/"));
            }
            $trail.appendChild(h("button", {
                class: "demo-tab" + (i === chain.length - 1 ? " on" : ""),
                type: "button",
                onclick: () => send(gobj, "EV_GO_DEPTH", {depth: i})
            }, step.node.label));
        });
        $chrome.appendChild($trail);
    }
    priv.$stage.appendChild($chrome);

    /*  What the tip shows: its own children as an index, or the view. */
    const $body = h("div", {class: "demo-node"}, []);
    if(tip.node.children.length) {
        $body.appendChild(h("div", {class: "demo-cards"},
            tip.node.children.map((c) => h("a", {
                class: "demo-card", href: "#" + tip.route + "/" + c.id,
                onclick: (ev) => {
                    ev.preventDefault();
                    send(gobj, "EV_GO", {depth: priv.path.length, id: c.id});
                }
            }, c.label))
        ));
    } else {
        $body.appendChild(h("div", {class: "demo-detail"}, [
            h("h4", {}, tip.node.label),
            h("p", {}, "The link node ends the structure. From here the "
                + "subpath belongs to the view.")
        ]));
    }
    priv.$stage.appendChild($body);

    priv.$stage.appendChild(h("div", {class: "demo-hist"}, [
        h("span", {class: "demo-note"},
            "depth " + priv.path.length + " · one declared route: /admin")
    ]));
}

function tree_ac_go(gobj, event, kw)
{
    const priv = gobj.priv;
    priv.path = priv.path.slice(0, kw.depth);
    priv.path.push(kw.id);
    tree_render(gobj);
    return 0;
}

function tree_ac_go_depth(gobj, event, kw)
{
    gobj.priv.path = gobj.priv.path.slice(0, kw.depth);
    tree_render(gobj);
    return 0;
}

function tree_ac_up(gobj)
{
    gobj.priv.path = gobj.priv.path.slice(0, -1);
    tree_render(gobj);
    return 0;
}

function tree_ac_set_nav_mode(gobj, event, kw)
{
    gobj.priv.mode = kw.nav_mode;
    tree_render(gobj);
    return 0;
}

function create_tree_gclass()
{
    if(__tree_gclass__) {
        return 0;
    }
    const states = [
        ["ST_IDLE", [
            ["EV_GO",           tree_ac_go,           null],
            ["EV_GO_DEPTH",     tree_ac_go_depth,     null],
            ["EV_UP",           tree_ac_up,           null],
            ["EV_SET_NAV_MODE", tree_ac_set_nav_mode, null]
        ]]
    ];
    const event_types = [
        ["EV_GO", 0], ["EV_GO_DEPTH", 0], ["EV_UP", 0], ["EV_SET_NAV_MODE", 0]
    ];
    __tree_gclass__ = gclass_create(
        TREE_GCLASS, event_types, states,
        {mt_create: tree_mt_create},
        0,
        [SDATA(data_type_t.DTP_POINTER, "$container", 0, null, "Root element"), SDATA_END()],
        {$url: null, $stage: null, $log: null, path: null, mode: "stack"},
        0, 0, 0, 0
    );
    return __tree_gclass__ ? 0 : -1;
}




                    /******************************
                     *      3 — the page stack
                     ******************************/




const PAGER_GCLASS = "C_NAV_DEMO_PAGER";
const PAGES = ["Pick a device", "Edit settings", "Confirm"];

let __pager_gclass__ = null;

/*
 *  Two states, because the root IS different: popping there does not
 *  navigate, it leaves — which is what EV_PAGER_EXIT means in the real
 *  C_YUI_PAGER.
 */
function pager_mt_create(gobj)
{
    build_frame(gobj, "url");
    gobj.priv.stack = [];
    pager_render(gobj);
}

function pager_render(gobj)
{
    const priv = gobj.priv;

    /*  The point of the whole demo: this never changes.  */
    priv.$url.textContent = "#/devices";
    priv.$stage.replaceChildren();

    const $deck = h("div", {class: "demo-deck"}, []);
    $deck.appendChild(h("div", {class: "demo-page root"}, "the view at rest"));
    priv.stack.forEach((title, i) => {
        $deck.appendChild(h("div", {
            class: "demo-page" + (i === priv.stack.length - 1 ? " top" : "")
        }, "page · " + title));
    });
    priv.$stage.appendChild($deck);

    const next = PAGES[priv.stack.length];
    priv.$stage.appendChild(h("div", {class: "demo-hist"}, [
        h("button", {
            class: "demo-btn", type: "button",
            disabled: next ? null : "disabled",
            onclick: () => send(gobj, "EV_PUSH_PAGE", {title: next})
        }, "EV_PUSH_PAGE"),
        h("button", {
            class: "demo-btn", type: "button",
            onclick: () => send(gobj, "EV_POP_PAGE", {})
        }, "EV_POP_PAGE"),
        h("span", {class: "demo-note"},
            "depth " + priv.stack.length + " · the url never moved")
    ]));

    if(priv.exited) {
        priv.$stage.appendChild(h("p", {class: "demo-exit"},
            "popped past the root → EV_PAGER_EXIT: the host closes."));
        priv.exited = false;
    }
}

function pager_ac_push(gobj, event, kw)
{
    gobj.priv.stack.push(kw.title);
    pager_render(gobj);
    return 0;
}

function pager_ac_pop(gobj)
{
    const priv = gobj.priv;
    if(!priv.stack.length) {
        priv.exited = true;
    } else {
        priv.stack.pop();
    }
    pager_render(gobj);
    return 0;
}

function create_pager_gclass()
{
    if(__pager_gclass__) {
        return 0;
    }
    const states = [
        ["ST_IDLE", [
            ["EV_PUSH_PAGE", pager_ac_push, null],
            ["EV_POP_PAGE",  pager_ac_pop,  null]
        ]]
    ];
    const event_types = [["EV_PUSH_PAGE", 0], ["EV_POP_PAGE", 0]];
    __pager_gclass__ = gclass_create(
        PAGER_GCLASS, event_types, states,
        {mt_create: pager_mt_create},
        0,
        [SDATA(data_type_t.DTP_POINTER, "$container", 0, null, "Root element"), SDATA_END()],
        {$url: null, $stage: null, $log: null, stack: null, exited: false},
        0, 0, 0, 0
    );
    return __pager_gclass__ ? 0 : -1;
}




                    /******************************
                     *          Mount
                     ******************************/




let __yuno__ = null;

function mount(slot_id, gclass_name, gobj_name_)
{
    const $slot = document.getElementById(slot_id);
    if(!$slot) {
        log_error("demos.js: no slot '" + slot_id + "'");
        return;
    }
    /*  Every gobj hangs from the yuno: the runtime refuses an orphan
     *  ("gobj NEEDS a parent!"), and a demo is not an exception to the
     *  tree — it is a small one.  */
    const gobj = gobj_create(gobj_name_, gclass_name, {}, __yuno__);
    if(!gobj) {
        log_error("demos.js: cannot create " + gclass_name);
        return;
    }
    gobj_start(gobj);
    $slot.appendChild(gobj_read_attr(gobj, "$container"));
}

/*
 *  The same boot a real SPA does, minus the persistence hooks: these
 *  demos have nothing to remember between reloads, on purpose — the
 *  page is about where a POSITION lives, and a demo that quietly
 *  restored its own would be making the opposite point.
 */
gobj_start_up(null, null, null, null, null, null, null);
register_c_yuno();
register_c_timer();   /*  C_YUNO creates one as a child  */
__yuno__ = gobj_create_yuno("navigation_demos", "C_YUNO", {
    yuno_name:    "navigation_demos",
    yuno_role:    "doc",
    yuno_version: "1.0.0"
});

if(create_url_gclass() === 0 && create_tree_gclass() === 0 && create_pager_gclass() === 0) {
    mount("demo-url",   URL_GCLASS,   "url");
    mount("demo-tree",  TREE_GCLASS,  "tree");
    mount("demo-pager", PAGER_GCLASS, "pager");
}
