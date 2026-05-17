/***********************************************************************
 *          wizard_helpers.js
 *
 *      Pure (DOM-free) step logic for C_YUI_WIZARD.
 *      Kept apart so it is unit-testable with `node --test`
 *      (same split as shell_toolbar_helpers.js / pager_helpers.js).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

/***************************************************************
 *  A step is a plain object: { id, title } (content/gobj are
 *  tracked by the gclass, never here).
 ***************************************************************/

/***************************************************************
 *  Clamp an index into [0, count-1] (0 when there are no steps).
 ***************************************************************/
function wizard_clamp_index(idx, count)
{
    if(count <= 0) {
        return 0;
    }
    if(idx < 0) {
        return 0;
    }
    if(idx > count - 1) {
        return count - 1;
    }
    return idx;
}

/***************************************************************
 *  Next / previous index (clamped, never wrap).
 ***************************************************************/
function wizard_next_index(idx, count)
{
    return wizard_clamp_index(idx + 1, count);
}

function wizard_prev_index(idx, count)
{
    return wizard_clamp_index(idx - 1, count);
}

/***************************************************************
 *  Chrome model for the current step.
 *
 *      opts = { allow_back, confirm_label, next_label }
 *
 *  Returns { idx, count, id, title, is_first, is_last,
 *            show_back, primary_label }.
 ***************************************************************/
function wizard_step_model(steps, idx, opts)
{
    opts = opts || {};
    let count = steps.length;
    let i = wizard_clamp_index(idx, count);
    let s = steps[i] || {};

    let is_first = i <= 0;
    let is_last  = i >= count - 1;

    let primary_label;
    if(is_last) {
        primary_label = opts.confirm_label || "confirm";
    } else {
        primary_label = opts.next_label || "next";
    }

    return {
        idx:           i,
        count:         count,
        id:            s.id || "",
        title:         s.title || "",
        is_first:      is_first,
        is_last:       is_last,
        show_back:     !!opts.allow_back && i > 0,
        primary_label: primary_label,
    };
}

/***************************************************************
 *  Accumulate a step's validated kw.
 *  Returns a NEW { merged, by_step } (never mutates).
 ***************************************************************/
function wizard_accumulate(acc, step_id, kw)
{
    if(!acc) {
        acc = { merged: {}, by_step: {} };
    }
    kw = kw || {};
    let merged  = Object.assign({}, acc.merged, kw);
    let by_step = Object.assign({}, acc.by_step);
    by_step[step_id] = kw;
    return { merged: merged, by_step: by_step };
}

/***************************************************************
 *  Whether the current step must be asked to validate before
 *  advancing: only linear wizards, and only when the step has a
 *  gobj that can answer EV_STEP_VALIDATE.
 ***************************************************************/
function wizard_should_validate(linear, has_gobj)
{
    return !!linear && !!has_gobj;
}

export {
    wizard_clamp_index,
    wizard_next_index,
    wizard_prev_index,
    wizard_step_model,
    wizard_accumulate,
    wizard_should_validate,
};
