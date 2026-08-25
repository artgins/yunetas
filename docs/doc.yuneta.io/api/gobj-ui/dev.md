---
title: 'gobj-ui: Development panel'
description: >-
  The panel of traces and traffic, the window of the tree of gobjs, and
  the traces that stay between two visits.
---

# Development panel

The panel shows the traffic between events, the traces of the framework and the
statistics. It is the window that a developer keeps open next to the
application.

**Source code:** [`src/yui_dev.js`](https://github.com/artgins/gobj-ui.js/blob/7.23.15/src/yui_dev.js),
[`src/yui_frontend_view.js`](https://github.com/artgins/gobj-ui.js/blob/7.23.15/src/yui_frontend_view.js)

---

(js_build_dev_panel)=
## [`build_dev_panel()`](https://github.com/artgins/gobj-ui.js/blob/7.23.15/src/yui_dev.js#L1589)

Builds the panel: a bar of controls, a log and the statistics. It follows the
theme that is active.

**Returns** an object with two keys.

| Key | Description |
|---|---|
| `$el` | The element of the panel. |
| `dispose` | Stops the trace of the traffic. Call it from the close of the modal that holds the panel. |

(js_setup_dev)=
## [`setup_dev(self, show)`](https://github.com/artgins/gobj-ui.js/blob/7.23.15/src/yui_dev.js#L1525)

Opens or closes the window of the developer, which holds the same panel inside a
`C_YUI_WINDOW` with a title bar, a button of maximize and a button of close.

It keeps the state of the window in the local storage, so a reload of the page
finds it again.

(js_dev_window_was_open)=
## [`dev_window_was_open()`](https://github.com/artgins/gobj-ui.js/blob/7.23.15/src/yui_dev.js#L1468)

Tells if the window of the developer was open in the last visit. The host reads
it at start up, and opens the window again, so the traffic and the traces that
the developer turned on keep their collection.

(js_apply_dev_traces)=
## [`apply_dev_traces()`](https://github.com/artgins/gobj-ui.js/blob/7.23.15/src/yui_dev.js#L1479)

Writes every trace of the developer that the local storage holds to the running
yuno: the traffic, the state machine, the creation, the start and the stop, the
subscriptions and the i18n.

It does not depend on the window. Call it one time at the start of the
application, so a reload keeps what the developer turned on.

(js_info_traffic)=
## [`info_traffic(title, msg, direction, size)`](https://github.com/artgins/gobj-ui.js/blob/7.23.15/src/yui_dev.js#L898)

Adds one message between events to the log of the traffic. The buffer has a
limit, so a change of the view or of the filter draws again from the memory.

| `direction` | Meaning |
|---|---|
| `1` | It goes out. |
| `2` | It comes in. |
| `3` | An error. |

With no logger in place the function writes to the console.

---

(js_setup_frontend_view)=
## [`setup_frontend_view(self)`](https://github.com/artgins/gobj-ui.js/blob/7.23.15/src/yui_frontend_view.js#L42)

Opens the tree of the gobjs of the application, in a window that is not modal.
It gives the window back, or `null` when the window exists already.

The tree is a pure child of the window, so every path that takes the window down
takes the tree with it.
