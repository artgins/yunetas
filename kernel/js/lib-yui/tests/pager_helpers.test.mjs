/***********************************************************************
 *          pager_helpers.test.mjs
 *
 *      Unit tests for the pure navigation-stack logic of C_YUI_PAGER.
 *      Run with: node --test tests/
 ***********************************************************************/
import { test } from "node:test";
import assert   from "node:assert/strict";

import {
    pager_push,
    pager_pop,
    pager_replace,
    pager_top,
    pager_header_model,
    pager_back_action,
} from "../src/pager_helpers.js";


/*============================================================
 *      pager_push / pager_pop / pager_replace are pure
 *============================================================*/
test("push does not mutate the input stack", () => {
    const s0 = [];
    const s1 = pager_push(s0, { id: "a", title: "A" });
    assert.equal(s0.length, 0);
    assert.equal(s1.length, 1);
    assert.equal(s1[0].id, "a");
});

test("pop returns popped entry and a shorter new stack", () => {
    const s = pager_push(pager_push([], { id: "a" }), { id: "b" });
    const r = pager_pop(s);
    assert.equal(r.popped.id, "b");
    assert.equal(r.stack.length, 1);
    assert.equal(s.length, 2); // original untouched
});

test("pop on empty stack returns null", () => {
    assert.equal(pager_pop([]), null);
});

test("replace swaps the top, keeps depth, reports replaced", () => {
    const s = pager_push(pager_push([], { id: "a" }), { id: "b" });
    const r = pager_replace(s, { id: "b2" });
    assert.equal(r.stack.length, 2);
    assert.equal(r.stack[1].id, "b2");
    assert.equal(r.replaced.id, "b");
});

test("replace on empty stack just pushes", () => {
    const r = pager_replace([], { id: "x" });
    assert.equal(r.stack.length, 1);
    assert.equal(r.replaced, null);
});

test("pager_top returns the last entry or null", () => {
    assert.equal(pager_top([]), null);
    assert.equal(pager_top([{ id: "a" }, { id: "b" }]).id, "b");
});


/*============================================================
 *      pager_header_model
 *============================================================*/
test("root page: title falls back to root_title", () => {
    const m = pager_header_model([{ id: "root", title: "" }], {
        root_title: "Preferences",
        back_on_root: true,
    });
    assert.equal(m.title, "Preferences");
    assert.equal(m.depth, 1);
});

test("root page: show_back follows back_on_root", () => {
    assert.equal(
        pager_header_model([{ id: "r" }], { back_on_root: true }).show_back,
        true
    );
    assert.equal(
        pager_header_model([{ id: "r" }], { back_on_root: false }).show_back,
        false
    );
});

test("deep page: back always shown, title from top entry", () => {
    const m = pager_header_model(
        [{ id: "r", title: "Root" }, { id: "lang", title: "Language" }],
        { root_title: "Root", back_on_root: false }
    );
    assert.equal(m.show_back, true);
    assert.equal(m.title, "Language");
    assert.equal(m.depth, 2);
});

test("discard shown only when with_discard AND top.discardable", () => {
    const top_yes = [{ id: "p", discardable: true }];
    const top_no  = [{ id: "p", discardable: false }];
    assert.equal(
        pager_header_model(top_yes, { with_discard: true }).show_discard,
        true
    );
    assert.equal(
        pager_header_model(top_yes, { with_discard: false }).show_discard,
        false
    );
    assert.equal(
        pager_header_model(top_no, { with_discard: true }).show_discard,
        false
    );
});

test("empty stack: title is root_title, depth 0", () => {
    const m = pager_header_model([], { root_title: "X" });
    assert.equal(m.title, "X");
    assert.equal(m.depth, 0);
});

test("back_kind: root+back_on_root -> 'close'", () => {
    const m = pager_header_model([{ id: "r" }], { back_on_root: true });
    assert.equal(m.back_kind, "close");
    assert.equal(m.show_back, true);
});

test("back_kind: root without back_on_root -> 'none'", () => {
    const m = pager_header_model([{ id: "r" }], { back_on_root: false });
    assert.equal(m.back_kind, "none");
    assert.equal(m.show_back, false);
});

test("back_kind: deeper page -> 'back'", () => {
    const m = pager_header_model(
        [{ id: "r" }, { id: "lang" }], { back_on_root: true }
    );
    assert.equal(m.back_kind, "back");
    assert.equal(m.show_back, true);
});


/*============================================================
 *      pager_back_action
 *============================================================*/
test("back with depth>1 -> pop", () => {
    assert.deepEqual(
        pager_back_action([{ id: "a" }, { id: "b" }], false),
        { type: "pop" }
    );
});

test("back at root with back_on_root -> exit", () => {
    assert.deepEqual(
        pager_back_action([{ id: "a" }], true),
        { type: "exit" }
    );
});

test("back at root without back_on_root -> noop", () => {
    assert.deepEqual(
        pager_back_action([{ id: "a" }], false),
        { type: "noop" }
    );
});

test("back on empty stack without back_on_root -> noop", () => {
    assert.deepEqual(pager_back_action([], false), { type: "noop" });
});
