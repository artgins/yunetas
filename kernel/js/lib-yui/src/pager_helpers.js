/***********************************************************************
 *          pager_helpers.js
 *
 *      Pure (DOM-free) navigation-stack logic for C_YUI_PAGER.
 *      Kept apart so it is unit-testable with `node --test`
 *      (same split as shell_toolbar_helpers.js).
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

/***************************************************************
 *  A stack entry is a plain object:
 *      { id, title, discardable }
 *  The DOM node bound to it is tracked by the gclass, never here.
 ***************************************************************/

/***************************************************************
 *  Push an entry, returning a NEW stack (never mutates).
 ***************************************************************/
function pager_push(stack, entry)
{
    return stack.concat([entry]);
}

/***************************************************************
 *  Pop the top entry.
 *  Returns { stack, popped } or null when the stack is empty.
 ***************************************************************/
function pager_pop(stack)
{
    if(stack.length === 0) {
        return null;
    }
    return {
        stack:  stack.slice(0, -1),
        popped: stack[stack.length - 1],
    };
}

/***************************************************************
 *  Replace the top entry (same depth).
 *  Returns { stack, replaced }; on an empty stack it just pushes.
 ***************************************************************/
function pager_replace(stack, entry)
{
    if(stack.length === 0) {
        return { stack: [entry], replaced: null };
    }
    return {
        stack:    stack.slice(0, -1).concat([entry]),
        replaced: stack[stack.length - 1],
    };
}

/***************************************************************
 *  Top entry or null.
 ***************************************************************/
function pager_top(stack)
{
    if(stack.length === 0) {
        return null;
    }
    return stack[stack.length - 1];
}

/***************************************************************
 *  What the header chrome must show for the current stack.
 *
 *      opts = { root_title, back_on_root, with_discard }
 *
 *  Returns { title, show_back, back_kind, show_discard, depth }.
 *  back_kind:
 *    "back"  — a deeper page: the affordance pops (icon: arrow)
 *    "close" — the root page with back_on_root: it exits/closes
 *              (icon: a cross, shown INSIDE the popup)
 *    "none"  — root page, back_on_root false: no affordance
 ***************************************************************/
function pager_header_model(stack, opts)
{
    opts = opts || {};
    let depth = stack.length;
    let top   = pager_top(stack);

    let title;
    if(top && typeof top.title === "string" && top.title.length > 0) {
        title = top.title;
    } else {
        title = opts.root_title || "";
    }

    let back_kind;
    if(depth > 1) {
        back_kind = "back";
    } else if(opts.back_on_root) {
        back_kind = "close";
    } else {
        back_kind = "none";
    }

    let show_discard = !!opts.with_discard && !!(top && top.discardable);

    return {
        title:        title,
        show_back:    back_kind !== "none",
        back_kind:    back_kind,
        show_discard: show_discard,
        depth:        depth,
    };
}

/***************************************************************
 *  Resolve what the "back" affordance must do.
 *
 *  Returns one of:
 *      { type: "pop" }   — pop the top page
 *      { type: "exit" }  — leave the pager (host should close)
 *      { type: "noop" }  — nothing (root, back_on_root === false)
 ***************************************************************/
function pager_back_action(stack, back_on_root)
{
    if(stack.length > 1) {
        return { type: "pop" };
    }
    if(back_on_root) {
        return { type: "exit" };
    }
    return { type: "noop" };
}

export {
    pager_push,
    pager_pop,
    pager_replace,
    pager_top,
    pager_header_model,
    pager_back_action,
};
