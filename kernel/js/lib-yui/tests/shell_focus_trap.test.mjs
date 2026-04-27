/***********************************************************************
 *          shell_focus_trap.test.mjs
 *
 *      Unit tests for the generic focus-trap helper.  Uses a tiny
 *      hand-rolled DOM stub injected via the second arg of
 *      `activate_focus_trap_on($panel, doc)` — no extra devDep.
 ***********************************************************************/
import { test } from "node:test";
import assert   from "node:assert/strict";

import {
    FOCUSABLE_SELECTOR,
    activate_focus_trap_on,
} from "../src/shell_focus_trap.js";


/***************************************************************
 *  Minimal stubs.  Just enough to drive the focus-trap module.
 ***************************************************************/
function make_focusable(name, opts = {})
{
    let calls = [];
    return {
        __name: name,
        focus: () => calls.push(`${name}.focus`),
        focus_calls: calls,
        ...opts
    };
}

function make_panel(focusables)
{
    return {
        querySelector(_sel) {
            return focusables[0] || null;
        },
        querySelectorAll(_sel) {
            return focusables;
        },
        contains(node) {
            return focusables.indexOf(node) >= 0;
        }
    };
}

function make_doc(initial_active, body_contains = true)
{
    let listeners = [];
    return {
        activeElement: initial_active,
        addEventListener(_t, fn, _opts) { listeners.push(fn); },
        removeEventListener(_t, fn, _opts) {
            let i = listeners.indexOf(fn);
            if(i >= 0) {
                listeners.splice(i, 1);
            }
        },
        body: {
            contains: () => body_contains
        },
        listeners
    };
}


test("FOCUSABLE_SELECTOR covers the standard focusable elements", () => {
    /*  Sanity: just check the string is non-trivial. */
    assert.ok(FOCUSABLE_SELECTOR.includes("a[href]"));
    assert.ok(FOCUSABLE_SELECTOR.includes("button:not([disabled])"));
    assert.ok(FOCUSABLE_SELECTOR.includes("input:not([disabled])"));
    assert.ok(FOCUSABLE_SELECTOR.includes("[tabindex]:not([tabindex=\"-1\"])"));
});

test("activate moves focus to the first focusable child of the panel", () => {
    let first = make_focusable("first");
    let last  = make_focusable("last");
    let panel = make_panel([first, last]);
    let doc   = make_doc(null);

    let release = activate_focus_trap_on(panel, doc);

    assert.deepEqual(first.focus_calls, ["first.focus"]);
    assert.deepEqual(last.focus_calls, []);
    /*  Listener registered. */
    assert.equal(doc.listeners.length, 1);

    release();
    /*  Listener removed. */
    assert.equal(doc.listeners.length, 0);
});

test("Tab from the last focusable wraps to the first", () => {
    let first = make_focusable("first");
    let last  = make_focusable("last");
    let panel = make_panel([first, last]);
    let doc   = make_doc(null);

    activate_focus_trap_on(panel, doc);

    /*  Reset focus_calls so we only see what the trap does. */
    first.focus_calls.length = 0;
    last.focus_calls.length  = 0;

    /*  Pretend `last` is focused now, then user presses Tab. */
    doc.activeElement = last;
    let prevented = false;
    let ev = {
        key: "Tab",
        shiftKey: false,
        preventDefault() { prevented = true; }
    };
    doc.listeners[0](ev);

    assert.equal(prevented, true);
    assert.deepEqual(first.focus_calls, ["first.focus"]);
    assert.deepEqual(last.focus_calls, []);
});

test("Shift+Tab from the first focusable wraps to the last", () => {
    let first = make_focusable("first");
    let last  = make_focusable("last");
    let panel = make_panel([first, last]);
    let doc   = make_doc(null);

    activate_focus_trap_on(panel, doc);
    first.focus_calls.length = 0;
    last.focus_calls.length  = 0;

    doc.activeElement = first;
    let prevented = false;
    let ev = {
        key: "Tab",
        shiftKey: true,
        preventDefault() { prevented = true; }
    };
    doc.listeners[0](ev);

    assert.equal(prevented, true);
    assert.deepEqual(last.focus_calls, ["last.focus"]);
});

test("non-Tab keys are ignored by the trap", () => {
    let first = make_focusable("first");
    let panel = make_panel([first]);
    let doc   = make_doc(null);

    activate_focus_trap_on(panel, doc);
    first.focus_calls.length = 0;

    let prevented = false;
    let ev = {
        key: "Escape",
        shiftKey: false,
        preventDefault() { prevented = true; }
    };
    doc.listeners[0](ev);

    assert.equal(prevented, false);
    assert.deepEqual(first.focus_calls, []);
});

test("focus jumps into the panel when activeElement is outside", () => {
    let first = make_focusable("first");
    let last  = make_focusable("last");
    let panel = make_panel([first, last]);
    let doc   = make_doc(null);
    let outside = make_focusable("outside");

    activate_focus_trap_on(panel, doc);
    first.focus_calls.length = 0;

    doc.activeElement = outside;   /*  not in panel.contains() */

    let prevented = false;
    let ev = {
        key: "Tab",
        shiftKey: false,
        preventDefault() { prevented = true; }
    };
    doc.listeners[0](ev);

    assert.equal(prevented, true);
    assert.deepEqual(first.focus_calls, ["first.focus"]);
});

test("release restores the previously-active element", () => {
    let prev = make_focusable("prev");
    let inside_first = make_focusable("first");
    let panel = make_panel([inside_first]);
    let doc   = make_doc(prev);   /*  prev was active before the trap */

    let release = activate_focus_trap_on(panel, doc);
    /*  After activation, the trap focuses the first inside element. */
    assert.deepEqual(inside_first.focus_calls, ["first.focus"]);

    release();
    assert.deepEqual(prev.focus_calls, ["prev.focus"]);
});

test("release is idempotent — calling it twice is safe", () => {
    let panel = make_panel([make_focusable("first")]);
    let doc   = make_doc(null);

    let release = activate_focus_trap_on(panel, doc);
    assert.equal(doc.listeners.length, 1);

    release();
    assert.equal(doc.listeners.length, 0);

    /*  Second release call must not throw and must not double-act. */
    release();
    assert.equal(doc.listeners.length, 0);
});

test("missing $panel returns a no-op release function", () => {
    let release = activate_focus_trap_on(null, make_doc(null));
    assert.equal(typeof release, "function");
    release();   /*  must not throw */
});

test("LIFO stacking: only the topmost trap acts on Tab", () => {
    /*  Two panels active simultaneously.  Pressing Tab must use
     *  the SECOND trap's panel, not the first — even though both
     *  listeners fire (capture phase). */
    let panel_a_first = make_focusable("a-first");
    let panel_a_last  = make_focusable("a-last");
    let panel_a = make_panel([panel_a_first, panel_a_last]);

    let panel_b_first = make_focusable("b-first");
    let panel_b_last  = make_focusable("b-last");
    let panel_b = make_panel([panel_b_first, panel_b_last]);

    let doc = make_doc(null);
    activate_focus_trap_on(panel_a, doc);
    activate_focus_trap_on(panel_b, doc);

    /*  Both listeners are attached. */
    assert.equal(doc.listeners.length, 2);

    /*  Reset focus_calls so we only see the Tab response. */
    panel_a_first.focus_calls.length = 0;
    panel_b_first.focus_calls.length = 0;

    /*  Pretend focus is "outside" both panels.  Press Tab. */
    doc.activeElement = make_focusable("outside");
    let ev = {
        key: "Tab",
        shiftKey: false,
        preventDefault() {}
    };
    /*  Listeners run in registration order. */
    doc.listeners[0](ev);
    doc.listeners[1](ev);

    /*  Only panel_b (the topmost) should have grabbed focus. */
    assert.deepEqual(panel_a_first.focus_calls, []);
    assert.deepEqual(panel_b_first.focus_calls, ["b-first.focus"]);
});

test("LIFO stacking: releasing the top trap re-empowers the one underneath", () => {
    let panel_a_first = make_focusable("a-first");
    let panel_a = make_panel([panel_a_first]);
    let panel_b_first = make_focusable("b-first");
    let panel_b = make_panel([panel_b_first]);

    let doc = make_doc(null);
    activate_focus_trap_on(panel_a, doc);
    let release_b = activate_focus_trap_on(panel_b, doc);

    release_b();
    assert.equal(doc.listeners.length, 1);

    /*  Now Tab should drive panel_a — it is again the top. */
    panel_a_first.focus_calls.length = 0;
    doc.activeElement = make_focusable("outside");
    let ev = {
        key: "Tab",
        shiftKey: false,
        preventDefault() {}
    };
    doc.listeners[0](ev);
    assert.deepEqual(panel_a_first.focus_calls, ["a-first.focus"]);
});

test("an empty panel installs the listener but does nothing on Tab", () => {
    let panel = make_panel([]);
    let doc   = make_doc(null);

    activate_focus_trap_on(panel, doc);
    assert.equal(doc.listeners.length, 1);

    let prevented = false;
    let ev = {
        key: "Tab",
        shiftKey: false,
        preventDefault() { prevented = true; }
    };
    doc.listeners[0](ev);
    /*  No focusables → trap stays out of the way. */
    assert.equal(prevented, false);
});
