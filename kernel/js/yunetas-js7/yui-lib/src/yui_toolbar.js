/*
 *  A toolbar with scroll (hidden) horizontal
 *
 *  Using Bulma Framework (https://bulma.io)
 *
 *  attrs: attributes to <div> of yui-horizontal-toolbar
 *  items: [[createElement2() or parameters of createElement2]]
 */
/* global ResizeObserver, window, document */

import {
    createElement2, debounce
} from "yunetas";

import "./yui_toolbar.css"; // Must be in index.js ?

function yui_toolbar(attrs={}, items = [])
{
    const left_button = `
        <span class="has-text-primary is-flex"><svg viewBox="0 0 320 512" style="fill:var(--bulma-link)"><path class="fa-secondary" opacity="1" d="M41.4 278.6c-12.5-12.5-12.5-32.8 0-45.3l160-160c12.5-12.5 32.8-12.5 45.3 0s12.5 32.8 0 45.3L109.3 256 246.6 393.4c12.5 12.5 12.5 32.8 0 45.3s-32.8 12.5-45.3 0l-160-160z"/><path class="fa-primary" d=""/></svg></span>
    `;
    const right_button = `
        <span class="has-text-primary is-flex"><svg viewBox="0 0 320 512" style="fill:var(--bulma-link)"><path class="fa-secondary" opacity="1" d="M278.6 233.4c12.5 12.5 12.5 32.8 0 45.3l-160 160c-12.5 12.5-32.8 12.5-45.3 0s-12.5-32.8 0-45.3L210.7 256 73.4 118.6c-12.5-12.5-12.5-32.8 0-45.3s32.8-12.5 45.3 0l160 160z"/><path class="fa-primary" d=""/></svg></span>
    `;

    // Create the toolbar container
    attrs.class = attrs.class?`yui-horizontal-toolbar ${attrs.class}`:'yui-horizontal-toolbar';
    let $toolbar = createElement2(['div', attrs, [
        ['button', { class: 'yui-horizontal-toolbar-scroll-btn left' },
            left_button, //'<',
            {
                click: cb_left_arrow
            }
        ],
        ['div', { class: 'yui-horizontal-toolbar-container' }, items],
        ['button', { class: 'yui-horizontal-toolbar-scroll-btn right' },
            right_button, //'>',
            {
                click: cb_right_arrow
            }
        ]
    ]]);

    let $container =$toolbar.querySelector('.yui-horizontal-toolbar-container');
    let $leftButton =$toolbar.querySelector('.yui-horizontal-toolbar-scroll-btn.left');
    let $rightButton =$toolbar.querySelector('.yui-horizontal-toolbar-scroll-btn.right');

    function cb_left_arrow(evt) {
        evt.stopPropagation();
        $container.scrollBy({ left: -20, behavior: 'smooth' });
    }
    function cb_right_arrow(evt) {
        evt.stopPropagation();
        $container.scrollBy({ left: 20, behavior: 'smooth' });
    }

    function updateScrollButtons(evt) {
        // window.console.log(evt);
        const isScrollable = $container.scrollWidth > $container.clientWidth;
        const atStart = $container.scrollLeft <= 0;
        const atEnd = $container.scrollLeft >= $container.scrollWidth - $container.clientWidth;
        $leftButton.style.display = isScrollable && !atStart ? 'block' : 'none';
        $rightButton.style.display = isScrollable && !atEnd ? 'block' : 'none';
    }

    $container.addEventListener('scroll', updateScrollButtons);

    const debouncedResize = debounce(updateScrollButtons, 300);
    const observer = new ResizeObserver(() => {
        debouncedResize();
    });
    observer.observe(document.body);

    updateScrollButtons();


    // window.addEventListener('resize', updateScrollButtons);
    // window.addEventListener('pageshow', updateScrollButtons);
    //
    // updateScrollButtons();

    return $toolbar;
}

export {yui_toolbar};
