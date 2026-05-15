/***********************************************************************
 *          shell_toolbar_helpers.js
 *
 *      Pure helpers for the C_YUI_SHELL declarative toolbar.  Split out
 *      of c_yui_shell.js so the additive item types ("brand", "avatar")
 *      and the new "dropdown" action type can be unit-tested without a
 *      DOM.
 *
 *      Three responsibilities:
 *          - classify_toolbar_item()  — pick the renderer kind
 *          - validate_toolbar_item()  — shape check, returns warnings
 *          - validate_dropdown_action() — shape check for dropdowns
 *
 *      Item kinds:
 *          "brand"   — logo (img) + wordmark (text); default action navigate
 *          "avatar"  — circular initials, populated by an app-registered
 *                      provider callback (yui_shell_set_avatar_provider)
 *          "action"  — every other toolbar item: icon and/or label that
 *                      triggers an action.type on click
 *
 *      Action types accepted on a toolbar item or on a dropdown sub-item:
 *          "navigate" | "drawer" | "event" | "dropdown"
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

export const TOOLBAR_ITEM_KINDS = ["brand", "avatar", "connection", "action"];
export const TOOLBAR_ACTION_TYPES = ["navigate", "drawer", "event", "dropdown"];


/************************************************************
 *  Decide which renderer to use for a toolbar item.
 *
 *  Returns one of "brand" | "avatar" | "action".  Items with
 *  no `type` (legacy contract) classify as "action" so existing
 *  configs keep working unchanged.
 ************************************************************/
export function classify_toolbar_item(item)
{
    if(!item || typeof item !== "object") {
        return "action";
    }
    let t = item.type;
    if(t === "brand" || t === "avatar" || t === "connection") {
        return t;
    }
    return "action";
}


/************************************************************
 *  Validate one toolbar item.  Returns { ok, warnings }.
 *
 *  Lenient by design: unknown fields are ignored so apps can
 *  carry extra metadata.  We only flag shapes that would render
 *  visibly broken (brand without logo/wordmark, dropdown without
 *  items, unknown action.type, etc.).
 ************************************************************/
export function validate_toolbar_item(item)
{
    let warnings = [];
    if(!item || typeof item !== "object") {
        warnings.push("toolbar item must be an object");
        return { ok: false, warnings: warnings };
    }

    let kind = classify_toolbar_item(item);
    let id = item.id || "<no-id>";

    if(kind === "brand") {
        if(!_is_non_empty_string(item.logo)) {
            warnings.push(
                `toolbar item '${id}' (type:brand) requires 'logo' (image URL)`
            );
        }
        if(!_is_non_empty_string(item.wordmark)) {
            warnings.push(
                `toolbar item '${id}' (type:brand) requires 'wordmark' (text)`
            );
        }
    }

    /*  avatar has no required fields beyond the optional action — the
     *  initials come from a runtime provider, not the JSON. */

    /*  Validate the action shape, including the new "dropdown" type. */
    if(item.action !== undefined && item.action !== null) {
        let nested = validate_action(item.action, id);
        for(let w of nested) {
            warnings.push(w);
        }
    }

    /*  Optional secondary (right-click) action — same shape as action,
     *  used e.g. by a connection indicator to open a dev panel. */
    if(item.context_action !== undefined && item.context_action !== null) {
        let nested = validate_action(item.context_action, id);
        for(let w of nested) {
            warnings.push(w);
        }
    }

    return { ok: warnings.length === 0, warnings: warnings };
}


/************************************************************
 *  Validate an `action` object (toolbar item action or
 *  dropdown sub-item action).  Returns an array of warnings.
 ************************************************************/
export function validate_action(action, owner_id)
{
    let warnings = [];
    if(!action || typeof action !== "object") {
        return warnings;
    }
    let id = owner_id || "<no-id>";

    if(action.type === undefined || action.type === null) {
        /*  No type at all is also caught at runtime by handle_toolbar_action,
         *  but flag it here so config errors surface during validate_config. */
        warnings.push(`toolbar item '${id}' has action without 'type'`);
        return warnings;
    }
    if(TOOLBAR_ACTION_TYPES.indexOf(action.type) < 0) {
        warnings.push(
            `toolbar item '${id}' has unknown action.type '${action.type}'; ` +
            `valid: ${TOOLBAR_ACTION_TYPES.join(", ")}`
        );
        return warnings;
    }

    if(action.type === "navigate" && !_is_non_empty_string(action.route)) {
        warnings.push(
            `toolbar item '${id}' (action.type:navigate) requires 'route'`
        );
    }
    if(action.type === "event" && !_is_non_empty_string(action.event)) {
        warnings.push(
            `toolbar item '${id}' (action.type:event) requires 'event' name`
        );
    }
    if(action.type === "dropdown") {
        let nested = validate_dropdown_action(action, id);
        for(let w of nested) {
            warnings.push(w);
        }
    }

    return warnings;
}


/************************************************************
 *  Validate the items of a dropdown action.
 *
 *  Each entry is either:
 *      { type: "divider" [, show_on] }
 *      { id, name?, icon?, action, show_on? }
 *
 *  Nested dropdowns are NOT supported: an "action.type": "dropdown"
 *  inside a dropdown sub-item is flagged.
 ************************************************************/
export function validate_dropdown_action(action, owner_id)
{
    let warnings = [];
    let id = owner_id || "<no-id>";

    if(!Array.isArray(action.items)) {
        warnings.push(
            `toolbar item '${id}' (action.type:dropdown) requires items[]`
        );
        return warnings;
    }
    if(action.items.length === 0) {
        warnings.push(
            `toolbar item '${id}' (action.type:dropdown) has empty items[]`
        );
    }

    for(let i = 0; i < action.items.length; i++) {
        let it = action.items[i];
        let where = `${id}[${i}]`;
        if(!it || typeof it !== "object") {
            warnings.push(`dropdown item ${where} must be an object`);
            continue;
        }
        if(it.type === "divider") {
            continue;
        }
        if(!it.action || typeof it.action !== "object") {
            warnings.push(
                `dropdown item ${where} requires an action object`
            );
            continue;
        }
        if(it.action.type === "dropdown") {
            warnings.push(
                `dropdown item ${where} has action.type:dropdown — ` +
                `nested dropdowns are not supported`
            );
            continue;
        }
        let nested = validate_action(it.action, where);
        for(let w of nested) {
            warnings.push(w);
        }
    }

    return warnings;
}


/************************************************************
 *  Internal — non-empty trimmed string check.
 ************************************************************/
function _is_non_empty_string(v)
{
    return typeof v === "string" && v.trim().length > 0;
}
