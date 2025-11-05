/***********************************************************************
 *          lib_graph.js
 *
 *          Utilities for graph, using Bulma
 *
 *          Copyright (c) 2025, ArtGins.
 *          All Rights Reserved.
 ***********************************************************************/
import {
    gobj_read_attr,
} from "yunetas";

// import {t} from "i18next";
import "./lib_graph.css";

/************************************************************
 *  Add class to elements selected
 ************************************************************/
function addClasses($container, selector, ...classNames)
{
    $container.querySelectorAll(selector).forEach(el =>
        el.classList.add(...classNames)
    );
}

/************************************************************
 *  Remove class from elements selected
 ************************************************************/
function removeClasses($container, selector, ...classNames)
{
    $container.querySelectorAll(selector).forEach(el =>
        el.classList.remove(...classNames)
    );
}

/************************************************************
 *  Toggle class to elements selected
 ************************************************************/
function toggleClasses($container, selector, ...classNames)
{
    $container.querySelectorAll(selector).forEach(el =>
        classNames.forEach(cls => el.classList.toggle(cls))
    );
}

/************************************************************
 *  Remove child elements
 ************************************************************/
function removeChildElements($element)
{
    $element.innerHTML = '';  // Remove all children
}

/************************************************************
 *  Disable/Enable elements
 ************************************************************/
function disableElements($container, selector)
{
    $container.querySelectorAll(selector).forEach(el => {
        el.setAttribute('disabled', '');
    });
}

function enableElements($container, selector)
{
    $container.querySelectorAll(selector).forEach(el => {
        if(el.hasAttribute('disabled')) {
            el.removeAttribute('disabled');
        }
    });
}

/************************************************************
 *  Set or reset the color of 'enable submit action' state
 *  Color green
 ************************************************************/
function set_submit_state($container, selector, set)
{
    if(set) {
        addClasses($container, selector, "color_submit_state");
    } else {
        removeClasses($container, selector, "color_submit_state");
    }
}

/************************************************************
 *  Set or reset the color of 'enable cancel action' state
 *  Color red
 ************************************************************/
function set_cancel_state($container, selector, set)
{
    if(set) {
        addClasses($container, selector, "color_cancel_state");
    } else {
        removeClasses($container, selector, "color_cancel_state");
    }
}

/************************************************************
 *  Set or reset the color of 'enable actived' state
 *  Color orange
 ************************************************************/
function set_active_state($container, selector, set)
{
    if(set) {
        addClasses($container, selector, "color_active_state");
    } else {
        removeClasses($container, selector, "color_active_state");
    }
}

/**
 * Returns a smart stroke color in `rgba()` format based on:
 * - The given fill color (hex or rgb[a])
 * - The current theme ('light' or 'dark')
 * - The adjustment factor (default 0.2)
 *
 * @param {string} fillColor - Fill color string
 * @param {string} theme - 'light' or 'dark'
 * @param {number} factor - Lighten/darken factor (default 0.2)
 * @returns {string} rgba(r, g, b, a)
 */
function getStrokeColor(fillColor, theme = 'light', factor = 0.2) {
    let r, g, b, a = 1;

    if(fillColor.startsWith('#')) {
        let hex = fillColor.slice(1);
        if(hex.length === 3 || hex.length === 4) {
            hex = hex.split('').map(c => c + c).join('');
        }

        if(hex.length === 6) {
            r = parseInt(hex.slice(0, 2), 16);
            g = parseInt(hex.slice(2, 4), 16);
            b = parseInt(hex.slice(4, 6), 16);
        } else if(hex.length === 8) {
            r = parseInt(hex.slice(0, 2), 16);
            g = parseInt(hex.slice(2, 4), 16);
            b = parseInt(hex.slice(4, 6), 16);
            a = parseInt(hex.slice(6, 8), 16) / 255;
        } else {
            throw new Error("Unsupported hex format");
        }
    } else if(fillColor.startsWith('rgb')) {
        const match = fillColor.match(/rgba?\(\s*(\d+),\s*(\d+),\s*(\d+)(?:,\s*([\d.]+))?\s*\)/);
        if(!match) {
            throw new Error("Invalid rgb()/rgba() format.");
        }
        r = parseInt(match[1], 10);
        g = parseInt(match[2], 10);
        b = parseInt(match[3], 10);
        if(match[4] !== undefined) {
            a = parseFloat(match[4]);
        }
    } else {
        throw new Error("Unsupported color format.");
    }

    const luminance = (0.299 * r + 0.587 * g + 0.114 * b) / 255;

    let lighten;
    if(theme === 'dark') {
        // In dark mode, prefer lighter strokes
        lighten = luminance < 0.7;
    } else {
        // In light mode, prefer darker strokes
        lighten = luminance < 0.3;
    }

    const adjust = (v) => lighten
        ? Math.min(255, Math.floor(v + (255 - v) * factor))
        : Math.max(0, Math.floor(v * (1 - factor)));

    return `rgba(${adjust(r)}, ${adjust(g)}, ${adjust(b)}, ${a.toFixed(3)})`;
}

//=======================================================================
//      Expose the class via the global object
//=======================================================================
export {
    addClasses,
    removeClasses,
    toggleClasses,
    removeChildElements,
    disableElements,
    enableElements,
    set_submit_state,
    set_cancel_state,
    set_active_state,
    getStrokeColor,
};
