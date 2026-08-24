---
title: 'gobj-ui: Theme'
description: >-
  Reading the theme that is active, and following a change of it from a
  gclass.
---

# Theme

**Source code:** [`src/yui_theme.js`](https://github.com/artgins/gobj-ui.js/blob/7.23.5/src/yui_theme.js)

The theme has three states, and only two of them are written down. An explicit
choice of the user writes `data-theme` on the root element. The default setting
writes nothing, and the preference of the operating system decides.

---

(js_yui_theme_now)=
## [`yui_theme_now()`](https://github.com/artgins/gobj-ui.js/blob/7.23.5/src/yui_theme.js#L37)

Gives the theme that is active, as `"dark"` or `"light"`. It reads
`data-theme` of the root element, and it reads the preference of the operating
system when the attribute is absent. Without a DOM it gives `"light"`.

(js_yui_is_dark)=
## [`yui_is_dark()`](https://github.com/artgins/gobj-ui.js/blob/7.23.5/src/yui_theme.js#L56)

Tells if the application draws dark.

(js_yui_watch_theme)=
## [`yui_watch_theme(gobj, event)`](https://github.com/artgins/gobj-ui.js/blob/7.23.5/src/yui_theme.js#L85)

Follows the theme, and sends `event` to `gobj` on every change, with the payload
`{theme}`. The default event is `EV_THEME`.

It watches **both** sources: the attribute `data-theme`, with a
`MutationObserver`, and the preference of the operating system, which is the
live source while the attribute is absent.

**Returns** a handle with `disconnect()`, or `null` where there is no DOM.

```javascript
priv.theme_observer = yui_watch_theme(gobj);
…
if(priv.theme_observer) {
    priv.theme_observer.disconnect();
    priv.theme_observer = null;
}
```

The gclass must declare the event in its state machine, and it must disconnect
the observer in `mt_destroy`.

:::{important}
The observer only translates the notification of the browser into an event. The
gclass draws again **in its action**, and not inside the callback. It is the
same rule as every other notification of the operating system.
:::

:::{note}
Draw the colors again. Do not build the component again. A canvas that the code
destroys and builds again loses the zoom and the position that the user chose.
:::
