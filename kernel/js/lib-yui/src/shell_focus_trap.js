/***********************************************************************
 *          shell_focus_trap.js
 *
 *      Generic focus-trap helper used by C_YUI_SHELL for drawers,
 *      modals and any other overlay that must capture keyboard
 *      navigation while open.
 *
 *      Usage:
 *          let release = activate_focus_trap_on($panel);
 *          // ... when the overlay closes:
 *          release();
 *
 *      The trap:
 *          - captures Tab / Shift+Tab so focus cycles inside $panel,
 *          - moves focus to the first focusable child on activate,
 *          - restores focus to whatever element had it before
 *            activation when release() runs.
 *
 *      Pure module — no gobj / Yuneta dependencies.  The optional
 *      second argument lets callers inject a mock document object
 *      for unit tests.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

/*  Selector matching the elements typically considered focusable.
 *  Kept conservative: items that are explicitly removed from the tab
 *  order (`tabindex="-1"`) are excluded; everything else is in. */
export const FOCUSABLE_SELECTOR =
    "a[href], button:not([disabled]), input:not([disabled])," +
    " select:not([disabled]), textarea:not([disabled])," +
    " [tabindex]:not([tabindex=\"-1\"])";


/***************************************************************
 *  Activate the focus-trap on $panel.
 *
 *  Returns a `release` function that must be called to tear the
 *  trap down when the overlay closes.  Calling release() multiple
 *  times is safe (becomes a no-op after the first call).
 *
 *  `doc` defaults to globalThis.document; pass a stub for tests.
 ***************************************************************/
export function activate_focus_trap_on($panel, doc)
{
    if(!doc) {
        doc = globalThis.document;
    }
    if(!$panel || !doc) {
        return function noop() {};
    }

    let saved = doc.activeElement || null;
    let released = false;

    let trap = function(ev) {
        if(ev.key !== "Tab" && ev.keyCode !== 9) {
            return;
        }
        let nodes = $panel.querySelectorAll(FOCUSABLE_SELECTOR);
        if(nodes.length === 0) {
            return;
        }
        let first = nodes[0];
        let last  = nodes[nodes.length - 1];
        let inside = $panel.contains(doc.activeElement);

        if(ev.shiftKey) {
            if(!inside || doc.activeElement === first) {
                last.focus();
                ev.preventDefault();
            }
        } else {
            if(!inside || doc.activeElement === last) {
                first.focus();
                ev.preventDefault();
            }
        }
    };
    doc.addEventListener("keydown", trap, true);

    /*  Move focus to the first focusable child of the panel. */
    let firstFocusable = $panel.querySelector(FOCUSABLE_SELECTOR);
    if(firstFocusable && typeof firstFocusable.focus === "function") {
        firstFocusable.focus();
    }

    return function release_focus_trap() {
        if(released) {
            return;
        }
        released = true;
        doc.removeEventListener("keydown", trap, true);
        if(saved && typeof saved.focus === "function" &&
           (!doc.body || !doc.body.contains || doc.body.contains(saved)))
        {
            saved.focus();
        }
    };
}
