/***********************************************************************
 *          shell_show_on.test.mjs
 *
 *      Unit tests for the pure `show_on` expression parser used by
 *      C_YUI_SHELL.  Run with:
 *          node --test tests/
 ***********************************************************************/
import { test } from "node:test";
import assert   from "node:assert/strict";

import {
    BULMA_BP_ORDER,
    breakpoints_from_expr,
    bulma_hidden_class,
} from "../src/shell_show_on.js";


/*  Helpers */
const VISIBLE   = { mobile:true,  tablet:true,  desktop:true,  widescreen:true,  fullhd:true  };
const INVISIBLE = { mobile:false, tablet:false, desktop:false, widescreen:false, fullhd:false };


test("empty expression ⇒ visible at every breakpoint", () => {
    assert.deepEqual(breakpoints_from_expr(""),   VISIBLE);
    assert.deepEqual(breakpoints_from_expr("  "), VISIBLE);
    assert.deepEqual(breakpoints_from_expr("*"),  VISIBLE);
    assert.deepEqual(breakpoints_from_expr(null), VISIBLE);
    assert.deepEqual(breakpoints_from_expr(undefined), VISIBLE);
});

test("single breakpoint ⇒ only that one", () => {
    assert.deepEqual(
        breakpoints_from_expr("mobile"),
        { ...INVISIBLE, mobile: true }
    );
    assert.deepEqual(
        breakpoints_from_expr("desktop"),
        { ...INVISIBLE, desktop: true }
    );
    assert.deepEqual(
        breakpoints_from_expr("fullhd"),
        { ...INVISIBLE, fullhd: true }
    );
});

test("OR combination with '|'", () => {
    assert.deepEqual(
        breakpoints_from_expr("mobile|tablet"),
        { ...INVISIBLE, mobile: true, tablet: true }
    );
    assert.deepEqual(
        breakpoints_from_expr("tablet | desktop | widescreen"),
        { ...INVISIBLE, tablet: true, desktop: true, widescreen: true }
    );
});

test(">=desktop means desktop and everything wider", () => {
    assert.deepEqual(
        breakpoints_from_expr(">=desktop"),
        { ...INVISIBLE, desktop: true, widescreen: true, fullhd: true }
    );
});

test(">mobile is strictly wider than mobile", () => {
    assert.deepEqual(
        breakpoints_from_expr(">mobile"),
        { ...INVISIBLE, tablet: true, desktop: true, widescreen: true, fullhd: true }
    );
});

test("<desktop is strictly narrower than desktop", () => {
    assert.deepEqual(
        breakpoints_from_expr("<desktop"),
        { ...INVISIBLE, mobile: true, tablet: true }
    );
});

test("<=tablet includes tablet and everything narrower", () => {
    assert.deepEqual(
        breakpoints_from_expr("<=tablet"),
        { ...INVISIBLE, mobile: true, tablet: true }
    );
});

test("< against the smallest breakpoint yields nothing", () => {
    /*  Historical guard: '<mobile' should be empty, not the whole set. */
    assert.deepEqual(breakpoints_from_expr("<mobile"), INVISIBLE);
});

test("> against the widest breakpoint yields nothing", () => {
    assert.deepEqual(breakpoints_from_expr(">fullhd"), INVISIBLE);
});

test("unknown tokens are ignored (no throw)", () => {
    assert.deepEqual(breakpoints_from_expr("phablet"), INVISIBLE);
    /*  Valid ones in the mix still apply. */
    assert.deepEqual(
        breakpoints_from_expr("phablet|mobile"),
        { ...INVISIBLE, mobile: true }
    );
});

test("OR combining ranges is the union of ranges", () => {
    assert.deepEqual(
        breakpoints_from_expr(">=desktop | mobile"),
        { ...INVISIBLE, mobile: true, desktop: true, widescreen: true, fullhd: true }
    );
});

test("BULMA_BP_ORDER is low→high and complete", () => {
    assert.deepEqual(
        BULMA_BP_ORDER,
        ["mobile", "tablet", "desktop", "widescreen", "fullhd"]
    );
});

test("bulma_hidden_class maps each breakpoint correctly", () => {
    assert.equal(bulma_hidden_class("mobile"),     "yui-hidden-mobile");
    assert.equal(bulma_hidden_class("tablet"),     "yui-hidden-tablet-only");
    assert.equal(bulma_hidden_class("desktop"),    "yui-hidden-desktop-only");
    assert.equal(bulma_hidden_class("widescreen"), "yui-hidden-widescreen-only");
    assert.equal(bulma_hidden_class("fullhd"),     "yui-hidden-fullhd");
    assert.equal(bulma_hidden_class("nonsense"),   "");
});
