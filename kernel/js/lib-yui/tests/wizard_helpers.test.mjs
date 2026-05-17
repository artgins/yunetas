/***********************************************************************
 *          wizard_helpers.test.mjs
 *
 *      Unit tests for the pure step logic of C_YUI_WIZARD.
 *      Run with: node --test tests/
 ***********************************************************************/
import { test } from "node:test";
import assert   from "node:assert/strict";

import {
    wizard_clamp_index,
    wizard_next_index,
    wizard_prev_index,
    wizard_step_model,
    wizard_accumulate,
    wizard_should_validate,
} from "../src/wizard_helpers.js";


/*============================================================
 *      clamp / next / prev
 *============================================================*/
test("clamp: negative -> 0, overflow -> count-1, empty -> 0", () => {
    assert.equal(wizard_clamp_index(-3, 4), 0);
    assert.equal(wizard_clamp_index(9, 4), 3);
    assert.equal(wizard_clamp_index(2, 4), 2);
    assert.equal(wizard_clamp_index(0, 0), 0);
});

test("next/prev never wrap", () => {
    assert.equal(wizard_next_index(0, 3), 1);
    assert.equal(wizard_next_index(2, 3), 2);   // already last
    assert.equal(wizard_prev_index(0, 3), 0);   // already first
    assert.equal(wizard_prev_index(2, 3), 1);
});


/*============================================================
 *      wizard_step_model
 *============================================================*/
const STEPS = [
    { id: "a", title: "Step A" },
    { id: "b", title: "Step B" },
    { id: "c", title: "Step C" },
];

test("first step: is_first, back hidden, primary='next'", () => {
    const m = wizard_step_model(STEPS, 0, {
        allow_back: true, confirm_label: "confirm", next_label: "next",
    });
    assert.equal(m.idx, 0);
    assert.equal(m.count, 3);
    assert.equal(m.id, "a");
    assert.equal(m.is_first, true);
    assert.equal(m.is_last, false);
    assert.equal(m.show_back, false);          // first step, even if allow_back
    assert.equal(m.primary_label, "next");
});

test("middle step: back shown when allow_back", () => {
    assert.equal(
        wizard_step_model(STEPS, 1, { allow_back: true }).show_back, true
    );
    assert.equal(
        wizard_step_model(STEPS, 1, { allow_back: false }).show_back, false
    );
});

test("last step: is_last, primary='confirm'", () => {
    const m = wizard_step_model(STEPS, 2, {
        allow_back: true, confirm_label: "confirm", next_label: "next",
    });
    assert.equal(m.is_last, true);
    assert.equal(m.primary_label, "confirm");
    assert.equal(m.title, "Step C");
});

test("out-of-range idx is clamped by the model", () => {
    const m = wizard_step_model(STEPS, 99, {});
    assert.equal(m.idx, 2);
    assert.equal(m.is_last, true);
});

test("default labels when opts omitted", () => {
    const m = wizard_step_model(STEPS, 0, {});
    assert.equal(m.primary_label, "next");
    const last = wizard_step_model(STEPS, 2, {});
    assert.equal(last.primary_label, "confirm");
});


/*============================================================
 *      wizard_accumulate (pure, immutable)
 *============================================================*/
test("accumulate merges flat and keeps per-step, no mutation", () => {
    const a1 = wizard_accumulate(null, "a", { x: 1 });
    const a2 = wizard_accumulate(a1, "b", { y: 2 });
    assert.deepEqual(a2.merged, { x: 1, y: 2 });
    assert.deepEqual(a2.by_step, { a: { x: 1 }, b: { y: 2 } });
    // a1 untouched
    assert.deepEqual(a1.merged, { x: 1 });
    assert.deepEqual(a1.by_step, { a: { x: 1 } });
});

test("accumulate: later step overrides merged key but keeps by_step", () => {
    const a1 = wizard_accumulate(null, "a", { v: "first" });
    const a2 = wizard_accumulate(a1, "b", { v: "second" });
    assert.equal(a2.merged.v, "second");
    assert.equal(a2.by_step.a.v, "first");
    assert.equal(a2.by_step.b.v, "second");
});

test("accumulate tolerates missing kw", () => {
    const a = wizard_accumulate(null, "a");
    assert.deepEqual(a.merged, {});
    assert.deepEqual(a.by_step, { a: {} });
});


/*============================================================
 *      wizard_should_validate
 *============================================================*/
test("validate only when linear AND step has a gobj", () => {
    assert.equal(wizard_should_validate(true, true), true);
    assert.equal(wizard_should_validate(true, false), false);
    assert.equal(wizard_should_validate(false, true), false);
    assert.equal(wizard_should_validate(false, false), false);
});
