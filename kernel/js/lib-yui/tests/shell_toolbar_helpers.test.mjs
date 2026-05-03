/***********************************************************************
 *          shell_toolbar_helpers.test.mjs
 *
 *      Unit tests for the pure toolbar helpers used by C_YUI_SHELL.
 *      Run with: node --test tests/
 ***********************************************************************/
import { test } from "node:test";
import assert   from "node:assert/strict";

import {
    TOOLBAR_ITEM_KINDS,
    TOOLBAR_ACTION_TYPES,
    classify_toolbar_item,
    validate_toolbar_item,
    validate_action,
    validate_dropdown_action,
} from "../src/shell_toolbar_helpers.js";


/*============================================================
 *      classify_toolbar_item
 *============================================================*/
test("classify: legacy item without 'type' is 'action'", () => {
    assert.equal(
        classify_toolbar_item({ id: "home", icon: "yi-home" }),
        "action"
    );
});

test("classify: type:'brand' classifies as 'brand'", () => {
    assert.equal(
        classify_toolbar_item({ id: "b", type: "brand", logo: "/x.svg", wordmark: "X" }),
        "brand"
    );
});

test("classify: type:'avatar' classifies as 'avatar'", () => {
    assert.equal(
        classify_toolbar_item({ id: "u", type: "avatar" }),
        "avatar"
    );
});

test("classify: unknown type falls back to 'action'", () => {
    assert.equal(
        classify_toolbar_item({ id: "?", type: "phantom" }),
        "action"
    );
});

test("classify: null/undefined are 'action'", () => {
    assert.equal(classify_toolbar_item(null), "action");
    assert.equal(classify_toolbar_item(undefined), "action");
});


/*============================================================
 *      validate_toolbar_item — brand
 *============================================================*/
test("validate brand: ok with logo + wordmark", () => {
    let r = validate_toolbar_item({
        id: "brand", type: "brand",
        logo: "/wm.svg", wordmark: "Wattyzer",
        action: { type: "navigate", route: "/welcome" }
    });
    assert.equal(r.ok, true);
    assert.deepEqual(r.warnings, []);
});

test("validate brand: missing logo flags warning", () => {
    let r = validate_toolbar_item({
        id: "brand", type: "brand", wordmark: "X"
    });
    assert.equal(r.ok, false);
    assert.match(r.warnings.join("\n"), /requires 'logo'/);
});

test("validate brand: missing wordmark flags warning", () => {
    let r = validate_toolbar_item({
        id: "brand", type: "brand", logo: "/x.svg"
    });
    assert.equal(r.ok, false);
    assert.match(r.warnings.join("\n"), /requires 'wordmark'/);
});

test("validate brand: empty-string fields count as missing", () => {
    let r = validate_toolbar_item({
        id: "brand", type: "brand", logo: "  ", wordmark: ""
    });
    assert.equal(r.ok, false);
    assert.equal(r.warnings.length, 2);
});


/*============================================================
 *      validate_toolbar_item — avatar
 *============================================================*/
test("validate avatar: ok with no extra fields", () => {
    let r = validate_toolbar_item({ id: "user", type: "avatar" });
    assert.equal(r.ok, true);
});

test("validate avatar: ok with dropdown action", () => {
    let r = validate_toolbar_item({
        id: "user", type: "avatar",
        action: {
            type: "dropdown",
            items: [
                { id: "p", name: "Profile",
                  action: { type: "navigate", route: "/me" } }
            ]
        }
    });
    assert.equal(r.ok, true);
});


/*============================================================
 *      validate_action
 *============================================================*/
test("action: navigate without route flags warning", () => {
    let w = validate_action({ type: "navigate" }, "x");
    assert.match(w.join("\n"), /requires 'route'/);
});

test("action: event without event name flags warning", () => {
    let w = validate_action({ type: "event" }, "x");
    assert.match(w.join("\n"), /requires 'event'/);
});

test("action: unknown action.type flags warning", () => {
    let w = validate_action({ type: "teleport" }, "x");
    assert.match(w.join("\n"), /unknown action\.type/);
});

test("action: missing type flags warning", () => {
    let w = validate_action({}, "x");
    assert.match(w.join("\n"), /action without 'type'/);
});

test("action: drawer with op only is accepted", () => {
    let w = validate_action({ type: "drawer", op: "toggle" }, "x");
    assert.deepEqual(w, []);
});


/*============================================================
 *      validate_dropdown_action
 *============================================================*/
test("dropdown: missing items[] flags warning", () => {
    let w = validate_dropdown_action({ type: "dropdown" }, "user");
    assert.match(w.join("\n"), /requires items\[\]/);
});

test("dropdown: empty items[] flags warning", () => {
    let w = validate_dropdown_action({ type: "dropdown", items: [] }, "user");
    assert.match(w.join("\n"), /empty items\[\]/);
});

test("dropdown: dividers are accepted without action", () => {
    let w = validate_dropdown_action({
        type: "dropdown",
        items: [
            { id: "p", name: "Profile",
              action: { type: "navigate", route: "/me" } },
            { type: "divider" },
            { id: "out", name: "Logout",
              action: { type: "event", event: "EV_LOGOUT" } }
        ]
    }, "user");
    assert.deepEqual(w, []);
});

test("dropdown: sub-item without action flags warning", () => {
    let w = validate_dropdown_action({
        type: "dropdown",
        items: [{ id: "x", name: "X" }]
    }, "user");
    assert.match(w.join("\n"), /requires an action object/);
});

test("dropdown: nested dropdowns rejected", () => {
    let w = validate_dropdown_action({
        type: "dropdown",
        items: [{
            id: "x", name: "X",
            action: { type: "dropdown", items: [] }
        }]
    }, "user");
    assert.match(w.join("\n"), /nested dropdowns are not supported/);
});

test("dropdown: invalid sub-action surfaces underlying warning", () => {
    let w = validate_dropdown_action({
        type: "dropdown",
        items: [{ id: "x", name: "X", action: { type: "navigate" } }]
    }, "user");
    assert.match(w.join("\n"), /requires 'route'/);
});


/*============================================================
 *      Constants exported are stable
 *============================================================*/
test("kinds + action-type constants are exposed", () => {
    assert.deepEqual(TOOLBAR_ITEM_KINDS, ["brand", "avatar", "action"]);
    assert.deepEqual(
        TOOLBAR_ACTION_TYPES,
        ["navigate", "drawer", "event", "dropdown"]
    );
});
