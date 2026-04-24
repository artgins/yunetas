/***********************************************************************
 *          shell_show_on.js
 *
 *      Pure breakpoint helpers used by C_YUI_SHELL to hide/show zones
 *      via Bulma visibility classes.  Split out of c_yui_shell.js so
 *      it can be unit-tested without a DOM.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/

/*  Bulma breakpoint order (low → high).  Used by show_on parser. */
export const BULMA_BP_ORDER = ["mobile", "tablet", "desktop", "widescreen", "fullhd"];


/************************************************************
 *  Parse a `show_on` expression into the set of breakpoints
 *  at which the element should be visible.
 *
 *      ""  |  "*"                always visible
 *      "mobile"                  only at mobile
 *      "mobile|tablet"           at mobile or tablet (OR)
 *      ">=desktop"               from desktop up
 *      "<desktop"                strictly below desktop
 *      "<=tablet"                tablet and below
 *      ">mobile"                 strictly above mobile
 *
 *  Returns { mobile:bool, tablet:bool, desktop:bool,
 *            widescreen:bool, fullhd:bool }.
 ************************************************************/
export function breakpoints_from_expr(expr)
{
    let out = { mobile:false, tablet:false, desktop:false, widescreen:false, fullhd:false };
    expr = String(expr == null ? "" : expr).trim();
    if(expr === "*" || expr === "") {
        for(let bp of BULMA_BP_ORDER) { out[bp] = true; }
        return out;
    }
    let parts = expr.split("|").map(s => s.trim()).filter(s => s.length);
    for(let p of parts) {
        let m;
        if((m = /^>=(\w+)$/.exec(p))) {
            let idx = BULMA_BP_ORDER.indexOf(m[1]);
            if(idx >= 0) {
                for(let i=idx;i<BULMA_BP_ORDER.length;i++) { out[BULMA_BP_ORDER[i]] = true; }
            }
        } else if((m = /^<(\w+)$/.exec(p))) {
            let idx = BULMA_BP_ORDER.indexOf(m[1]);
            if(idx > 0) {
                for(let i=0;i<idx;i++) { out[BULMA_BP_ORDER[i]] = true; }
            }
        } else if((m = /^<=(\w+)$/.exec(p))) {
            let idx = BULMA_BP_ORDER.indexOf(m[1]);
            if(idx >= 0) {
                for(let i=0;i<=idx;i++) { out[BULMA_BP_ORDER[i]] = true; }
            }
        } else if((m = /^>(\w+)$/.exec(p))) {
            let idx = BULMA_BP_ORDER.indexOf(m[1]);
            if(idx >= 0) {
                for(let i=idx+1;i<BULMA_BP_ORDER.length;i++) { out[BULMA_BP_ORDER[i]] = true; }
            }
        } else if(Object.prototype.hasOwnProperty.call(out, p)) {
            out[p] = true;
        }
    }
    return out;
}


/*  Map each Bulma breakpoint to the helper class that hides it. */
export function bulma_hidden_class(bp)
{
    switch(bp) {
        case "mobile":     return "yui-hidden-mobile";          /*  <769       */
        case "tablet":     return "yui-hidden-tablet-only";     /*  769-1023   */
        case "desktop":    return "yui-hidden-desktop-only";    /*  1024-1215  */
        case "widescreen": return "yui-hidden-widescreen-only"; /*  1216-1407  */
        case "fullhd":     return "yui-hidden-fullhd";          /*  ≥1408      */
    }
    return "";
}
