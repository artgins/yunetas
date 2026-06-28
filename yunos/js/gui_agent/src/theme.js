/***********************************************************************
 *          theme.js
 *
 *          Light / dark theme via Bulma's `data-theme` on <html>,
 *          persisted in localStorage. Bulma 1.x ships the dark palette,
 *          so toggling the attribute is all that is needed.
 *
 *          Copyright (c) 2026, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    kw_get_local_storage_value,
    kw_set_local_storage_value,
} from "@yuneta/gobj-js";


const STORAGE_KEY = "theme";


/***************************************************************
 *  Current theme ("light" | "dark").
 ***************************************************************/
function current_theme()
{
    let t = kw_get_local_storage_value(STORAGE_KEY, "light", true);
    return (t === "dark") ? "dark" : "light";
}

/***************************************************************
 *  Apply a theme to <html> (and remember it).
 ***************************************************************/
function apply_theme(theme)
{
    let t = (theme === "dark") ? "dark" : "light";
    document.documentElement.setAttribute("data-theme", t);
    kw_set_local_storage_value(STORAGE_KEY, t);
    return t;
}

/***************************************************************
 *  Flip light <-> dark. Returns the new theme.
 ***************************************************************/
function toggle_theme()
{
    return apply_theme(current_theme() === "dark" ? "light" : "dark");
}

export {current_theme, apply_theme, toggle_theme};
