/*********************************************************
 *      lib_icons.js
 *
 *      Two icon systems are managed here:
 *
 *      1. FA-style CSS icons (yui_icons.css)
 *         Usage: <i class="yi-save"></i>
 *         Color: inherits currentColor from parent via CSS mask.
 *
 *      2. SVG symbol icons for G6 toolbar (and any other component
 *         that uses <use xlink:href="#id">).
 *         Symbols are injected once into the DOM.
 *         All symbol ids are prefixed with "g6-icon-" to avoid
 *         conflicts with other DOM elements.
 *         Icon paths come from:
 *           fontawesome-free-7.1.0-web/sprites/regular.svg
 *
 *         G6 toolbar items are managed via CSS class names (className
 *         field in getItems()) using YuiToolbar in c_g6_nodes_tree.js.
 *         This lets lib_graph.js state functions (set_submit_state,
 *         disableElements, …) work on toolbar icons exactly like on
 *         regular Bulma/FA buttons — no icon-specific helpers needed.
 *
 *      Copyright (c) 2025-2026, ArtGins.
 *      All Rights Reserved.
 *********************************************************/

const CUSTOM_SVG_ICONS_ID = 'g6-custom-svgicons';

const ICONS = `<svg xmlns="http://www.w3.org/2000/svg">
    <!-- FA7 regular floppy-disk -->
    <symbol id="g6-icon-save" viewBox="0 0 448 512">
        <path fill="currentColor" d="M64 80c-8.8 0-16 7.2-16 16l0 320c0 8.8 7.2 16 16 16l320 0c8.8 0 16-7.2 16-16l0-242.7c0-4.2-1.7-8.3-4.7-11.3L320 86.6 320 176c0 17.7-14.3 32-32 32l-160 0c-17.7 0-32-14.3-32-32l0-96-32 0zm80 0l0 80 128 0 0-80-128 0zM0 96C0 60.7 28.7 32 64 32l242.7 0c17 0 33.3 6.7 45.3 18.7L429.3 128c12 12 18.7 28.3 18.7 45.3L448 416c0 35.3-28.7 64-64 64L64 480c-35.3 0-64-28.7-64-64L0 96zM160 320a64 64 0 1 1 128 0 64 64 0 1 1 -128 0z"></path>
    </symbol>
</svg>`;

/*
 *  Inject all custom SVG symbols into the DOM once.
 *  Subsequent calls are no-ops (guarded by element id).
 */
export function inject_svg_icons()
{
    if(document.getElementById(CUSTOM_SVG_ICONS_ID)) {
        return;
    }
    const div = document.createElement('div');
    div.id = CUSTOM_SVG_ICONS_ID;
    div.style.display = 'none';
    div.innerHTML = ICONS;
    document.body.appendChild(div);
}
