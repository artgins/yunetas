---
title: 'gobj-ui: The shell API'
description: >-
  The functions that drive C_YUI_SHELL: navigation, sub-routes, drawers,
  the escape chain, the overlays, the avatars and the toolbar.
---

# The shell API

`C_YUI_SHELL` draws the frame of the application from one JSON file: the
toolbar, the menus and the zones where the views go. These functions drive it
from outside.

**Source code:** [`src/c_yui_shell.js`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js)

The design is in [The declarative shell](../../../../kernel/js/gobj-ui/SHELL.md)
and [Routing](../../../../kernel/js/gobj-ui/ROUTING.md). This page is the
reference of the functions.

:::{note}
Find the shell of a gobj with `yui_shell_of()`, and not with
[`gobj_parent()`](../js/hierarchy.md#js_gobj_parent). Under `C_YUI_NODE` the
parent is the node, and it is not the shell.
:::

---

## Navigation

(js_yui_shell_navigate)=
## [`yui_shell_navigate(shell_gobj, route, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3160)

Goes to a route.

| Option | Effect |
|---|---|
| (none) | **Push.** It writes an entry in the history, so the Back button returns. It is the default, and it is the safe one: a human chose to go there. |
| `opts.replace: true` | **Replace.** It changes the URL and writes no entry. Use it when the **code** decided the move: a redirect, a normalization, the default child of a parent menu, or a restore after a reload of the page. |
| `opts.push: true` | The default, written out. It stays valid, so a call site documents its intent. |

(js_yui_shell_set_sub_routes)=
## [`yui_shell_set_sub_routes(shell_gobj, base_route, nodes)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3203)

Declares the deep routes that a view owns, for the site map. `nodes` is an
ordered array of `{route, label, icon?, children?}` with full routes.

Give an empty value to clear them, and do it when the view stops. A map that
holds the children of a view that went away is a map that lies.

(js_yui_shell_register_event_handler)=
## [`yui_shell_register_event_handler(shell_gobj, event, gclass)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3223)

Declares that a gclass handles an action of the toolbar or of the account menu,
so the site map shows where the action lives. Call it one time, next to the
subscription. More than one gclass can handle the same event.

(js_yui_shell_unpark_route)=
## [`yui_shell_unpark_route(route)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3299)

Takes the URL off an action route that the application parked it on. Call it
from the path that closes the overlay.

The function has a guard, and the guard is the point of it. When the close comes
from the drain of the overlays, because the user went to another route while the
overlay was open, the URL moved already. A `history.back()` there lands on the
entries of the action, fires the action again, opens the overlay again, and
takes the navigation that the user asked for. The function goes back only while
the URL still sits on the route.

---

## Drawers

The three functions open and close the off-canvas navigation from outside, such
as from a button in the toolbar. `menu_id` is optional.

(js_yui_shell_open_drawer)=
### [`yui_shell_open_drawer(shell_gobj, menu_id)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3237)

Opens the drawer.

(js_yui_shell_close_drawer)=
### [`yui_shell_close_drawer(shell_gobj, menu_id)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3238)

Closes the drawer.

(js_yui_shell_toggle_drawer)=
### [`yui_shell_toggle_drawer(shell_gobj, menu_id)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3239)

Opens the drawer when it is closed, and closes it when it is open.

---

## The escape chain

An overlay that the shell does not own declares itself here, so the Escape key
reaches the top one first. The drawer is built in.

```javascript
let close_fn = () => my_modal.close();
yui_shell_push_escape(shell, "modal", close_fn);
// … when the modal closes by any path:
yui_shell_pop_escape(shell, close_fn);
```

(js_yui_shell_push_escape)=
### [`yui_shell_push_escape(shell_gobj, layer, handler)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3254)

Puts a handler on the chain. `layer` is a free tag, such as `"modal"`,
`"popup"` or `"overlay"`. Today it is information only: the order of the stack
decides the priority, and that order matches the layers of the z-index that most
applications use.

(js_yui_shell_pop_escape)=
### [`yui_shell_pop_escape(shell_gobj, handler)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3258)

Takes a handler off the chain. Call it on **every** path that closes the
overlay, and not only on the Escape key.

---

## Overlays and the Back button

An overlay is transient. It declares itself here so the Back button closes it
before it moves the application.

```javascript
let overlay = yui_shell_register_overlay(shell, close_fn);
// … when the overlay closes by any path that is not Back:
yui_shell_overlay_dismissed(shell, overlay);
```

(js_yui_shell_register_overlay)=
### [`yui_shell_register_overlay(shell_gobj, close_fn, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3277)

Declares an overlay. `close_fn` is what the Back button calls to take the
overlay down.

With `opts.keep_on_navigate` set to `true` the overlay is a panel of navigation,
and it stays when the route changes. The default is off, because an overlay that
outlives the view below it is the exception, and the exception asks for itself.

**Returns** the overlay, or `null` when the integration with the history is off.
A caller that receives `null` skips the call below.

(js_yui_shell_overlay_dismissed)=
### [`yui_shell_overlay_dismissed(shell_gobj, overlay)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3281)

Tells the shell that the overlay went away by a path that is not the Back
button.

---

## Avatars

(js_yui_shell_set_avatar_provider)=
### [`yui_shell_set_avatar_provider(shell_gobj, provider)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3316)

Gives the function that the shell asks for the image of a user.

(js_yui_shell_refresh_avatars)=
### [`yui_shell_refresh_avatars(shell_gobj)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3326)

Asks the provider again, and draws the avatars again.

---

## Language

(js_yui_shell_set_translator)=
### [`yui_shell_set_translator(shell_gobj, t)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3341)

Gives the translation function of the application to the shell.

:::{important}
After the application changes its own language, it calls
`yui_shell_language_changed()`, which publishes `EV_LANGUAGE_CHANGED`. A view
that draws through a widget subscribes to that event and draws again **in the
action**. Never listen to `languageChanged` of i18next.
:::

---

## The toolbar

(js_yui_shell_set_connection_state)=
### [`yui_shell_set_connection_state(shell_gobj, connected)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3412)

Draws the state of the connection in the toolbar.

(js_yui_shell_set_toolbar_item_icon)=
### [`yui_shell_set_toolbar_item_icon(shell_gobj, item_id, icon_class)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3431)

Changes the icon of an item of the toolbar.

(js_yui_shell_set_toolbar_item_badge)=
### [`yui_shell_set_toolbar_item_badge(shell_gobj, item_id, value)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3464)

Writes a badge on an item of the toolbar, such as a count of messages.

(js_yui_shell_close_dropdown)=
### [`yui_shell_close_dropdown(shell_gobj)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/c_yui_shell.js#L3501)

Closes the dropdown of the account.

---

## The site map

(js_yui_shell_show_route_map)=
### [`yui_shell_show_route_map(shell, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.8.1/src/shell_route_map.js#L437)

Opens the site map, which shows the whole surface of the navigation as a tree:
the toolbar, each menu, the sub-routes that each view declared, and the routes
that only the table of routes holds. It marks the position of the reader.
