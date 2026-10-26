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

**Source code:** [`src/c_yui_shell.js`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js)

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
## [`yui_shell_navigate(shell_gobj, route, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L4835)

Goes to a route.

| Option | Effect |
|---|---|
| (none) | **Push.** It writes an entry in the history, so the Back button returns. It is the default, and it is the safe one: a human chose to go there. |
| `opts.replace: true` | **Replace.** It changes the URL and writes no entry. Use it when the **code** decided the move: a redirect, a normalization, the default child of a parent menu, or a restore after a reload of the page. |
| `opts.push: true` | The default, written out. It stays valid, so a call site documents its intent. |

(js_yui_shell_last_route_under)=
## [`yui_shell_last_route_under(shell_gobj, route)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L4595)

Tells where the reader last was under a route. It returns the most recent route
that the shell went to and that is `route` or is below it. When there is no
such route, it returns `route`.

Use it for a control that goes back to a view as the reader left it. A treedb
graph keeps its focused topic in the URL (`<graph>/<topic>`), so a control that
goes to the bare route of the graph removes the focus. Ask this function first,
then give its answer to `yui_shell_navigate()`.

The memory is a copy of the URL for the life of the page. The shell keeps the
last 64 routes, does not store them, and does not apply them. Only a control
that asks gets the answer.

(js_yui_shell_last_route_outside)=
## [`yui_shell_last_route_outside(shell_gobj, route)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L4626)

Tells where the reader last was outside a route. It returns the most recent
route that the shell went to and that is not `route` and is not below it. When
there is no such route, it returns `""`: the page started on that view and did
not leave it.

Use it to find the last view that the reader saw before a subtree. It skips
every route below `route`. For a "←" button use `yui_shell_previous_route()`
instead: that button must go where the browser Back button goes, and Back does
not skip the routes of a subtree.

```javascript
const before_details = yui_shell_last_route_outside(shell, "/devices/detail");
```

This function reads the same memory as `yui_shell_last_route_under()`.

(js_yui_shell_previous_route)=
## [`yui_shell_previous_route(shell_gobj)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L4658)

Tells which route the reader was on before the current one. It returns the most
recent route that the shell went to and that is not the current route. When
there is no such route, it returns `""`: the page started on this route and did
not move.

Use it for the "←" button of a view. The rule is that this button goes to the
same place as the browser Back button. A button that goes to one fixed route
agrees with Back only when the reader came from that route.

When the answer is `""`, use a fixed route of your own:

```javascript
import {yui_shell_of, yui_shell_navigate, yui_shell_previous_route}
    from "@yuneta/gobj-ui/src/c_yui_shell.js";

function ac_go_back(gobj, event, kw, src)
{
    const shell = yui_shell_of(gobj);
    const target = yui_shell_previous_route(shell) || "/devices/list";
    yui_shell_navigate(shell, target, {push: true});
    return 0;
}
```

A "←" that goes UP one level, and names the level it goes to, is a different
control. Give it a fixed route.

(js_yui_shell_set_sub_routes)=
## [`yui_shell_set_sub_routes(shell_gobj, base_route, nodes)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L4888)

Declares the deep routes that a view owns, for the site map. `nodes` is an
ordered array of `{route, label, icon?, children?}` with full routes.

Give an empty value to clear them, and do it when the view stops. A map that
holds the children of a view that went away is a map that lies.

(js_yui_shell_register_event_handler)=
## [`yui_shell_register_event_handler(shell_gobj, event, gclass)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L4908)

Declares that a gclass handles an action of the toolbar or of the account menu,
so the site map shows where the action lives. Call it one time, next to the
subscription. More than one gclass can handle the same event.

(js_yui_shell_unpark_route)=
## [`yui_shell_unpark_route(route)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L5030)

Takes the URL off an action route that the application parked it on. Call it
from the path that closes the overlay.

The function has a guard, and the guard is the point of it. When the close comes
from the drain of the overlays, because the user went to another route while the
overlay was open, the URL moved already. A `history.back()` there lands on the
entries of the action, fires the action again, opens the overlay again, and
takes the navigation that the user asked for. The function goes back only while
the URL still sits on the route.

When the close comes from a shell that stops, with the overlay still up (a
logout), the function does not go back. It writes the resting route under the
overlay over the URL in place, with `history.replaceState()`. A `back()` is
asynchronous, and the stopped shell never hears where it lands. Up to gobj-ui
7.26.47 that `back()` landed on the entry of the action, because the marker of
the overlay stays in place when the shell stops. The URL stayed on the route,
and a shell built in the same turn read it and opened the overlay again.

```js
yui_shell_show_modal(shell, $content, {
    dialog: true,
    title: "preferences",
    on_close: function() {
        yui_shell_unpark_route("/prefs");   // the route of `redirect: "stay"`
    }
});
```

---

## Drawers

The three functions open and close the off-canvas navigation from outside, such
as from a button in the toolbar. `menu_id` is optional.

(js_yui_shell_open_drawer)=
### [`yui_shell_open_drawer(shell_gobj, menu_id)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L4922)

Opens the drawer.

(js_yui_shell_close_drawer)=
### [`yui_shell_close_drawer(shell_gobj, menu_id)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L4927)

Closes the drawer.

(js_yui_shell_toggle_drawer)=
### [`yui_shell_toggle_drawer(shell_gobj, menu_id)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L4932)

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
### [`yui_shell_push_escape(shell_gobj, layer, handler, teardown)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L4955)

Puts a handler on the chain. `layer` is a free tag, such as `"modal"`,
`"popup"` or `"overlay"`. Today it is information only: the order of the stack
decides the priority, and that order matches the layers of the z-index that most
applications use.

`teardown` (optional, gobj-ui 7.26.46) is the close of the overlay that never
refuses. The shell calls it when it stops while the overlay is still open (a
logout, a session cut). It closes such overlays from the top down, before its
views stop. Give it when `handler` can refuse to close (a `before_close` that
asks). Without it, the overlay and its focus trap stay after the shell is gone.

A shell that stops does not move the browser history (since gobj-ui 7.26.47).
In 7.26.46 the stop removed the history entries of its overlays with
`history.back()`. That call is asynchronous, and a stopped shell does not see
where it lands: after a logout and a new login in the same turn, it removed the
entry of the first window of the new shell, and the next Back left the
application.

The next shell removes those entries (since gobj-ui 7.26.49). The entries of
the overlays that the stop closed, and the entry of a `stay` action route under
one of them, are dead: a Back onto one of them closes nothing. The stopped shell
gives them to the next shell. When the next shell starts with `use_hash` (the
default), it
removes them with one `history.go(-n)`, and it waits for the landing as for a
`back()` of its own: an overlay that it opens in the meantime gets its entry
after the landing. In 7.26.47 and 7.26.48 the entries stayed, and each logout
with an overlay open added dead Backs. The shell finds the entries by their key
in the Navigation API. A reload keeps the keys in `sessionStorage` for the
first shell of the new page.

The shell does not go past the first entry of the current page (since gobj-ui
7.26.51). A `location.replace()` to another page keeps the key of the entry
that it replaces. Thus a later load of the application can be on the key of a
dead entry, above the entries of the earlier load. In 7.26.49 and 7.26.50 the
shell went back into those entries, and the browser loaded the earlier page
again. After a reload, all the entries belong to the new page
(`sameDocument`), and the shell removes the dead ones.

Dead entries below an entry of the reader are removed when a Back of the
reader lands on them (since gobj-ui 7.26.51). This occurs when the reader
moves while the login page is shown. The shell continues the Back with one
`history.go(-n)` to the first live entry below them. A Forward onto them
continues to the first live entry above them; when there is none, the shell
does not move. The shell gets the direction from the `currententrychange`
event of the Navigation API. Thus the shell can move the history by itself:
at its start, and after a Back or a Forward of the reader. An application with
its own `popstate` or `hashchange` listener receives that traversal.

```javascript
// session history: /windows, /history, [dead marker], [dead marker], /history/pos-9
// a Back of the reader from /history/pos-9 lands on a dead marker:
//   the shell continues with history.go(-2) and lands on /history
// a Forward from /history lands on a dead marker:
//   the shell continues with history.go(2) and lands on /history/pos-9
```

Two limits stay. A page with no next shell keeps the dead entries: with a
question over a panel, two Backs change nothing while the login page is shown.
The next shell removes them. A browser without the Navigation API keeps the
dead entries, as in 7.26.48: each logout with a question over a panel adds two
entries and two dead Backs. Each logout with the preferences open on their
action route (`/prefs`, `stay`) keeps the action entry below their marker,
because `yui_shell_unpark_route()` changes only the current entry. A Back onto
that entry opens the preferences again, one time for each such logout.

```javascript
// a logout with a dialog open, then a new login
gobj_stop_tree(old_shell);
gobj_destroy(old_shell);
let shell = gobj_create_pure_child("shell", "C_YUI_SHELL", {config: config, subscriber: app}, app);
gobj_start_tree(shell);     // removes the dead entries: one Back closes, the next leaves
```

A question that is open at the stop gets the answer
of Escape: `false` for a yes/no or danger question, `"cancel"` for
yes/no/cancel. A `yui_shell_confirm_ok()` resolves as it does on every close:
code that waits for it runs after the stop, and must not use the old shell.

```javascript
let ask_then_close = () => my_modal.request_close();   // can refuse
let close_now      = () => my_modal.close();           // never refuses
yui_shell_push_escape(shell, "modal", ask_then_close, close_now);
```

(js_yui_shell_pop_escape)=
### [`yui_shell_pop_escape(shell_gobj, handler)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L4959)

Takes a handler off the chain. Call it on **every** path that closes the
overlay, and not only on the Escape key.

(js_yui_shell_escape_order)=
### [`yui_shell_escape_order(shell_gobj)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L4976)

Returns the order of the top entry of the chain, or 0 when the chain is empty.
Every entry gets an order when it is pushed, and the orders only grow. An
overlay that hears the Escape key itself keeps the order when it opens. When
the order is higher at its Escape, an overlay was opened after it: it lets the
key go on to the shell, which closes that overlay first. A `C_YUI_WINDOW` does
this since gobj-ui 7.26.45.

```javascript
priv.escape_order = yui_shell_escape_order(shell);     // when it opens
// … in its own Escape handler:
if(yui_shell_escape_order(shell) > priv.escape_order) {
    return;     // a menu opened after it: the shell closes it first
}
```

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
### [`yui_shell_register_overlay(shell_gobj, close_fn, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L4999)

Declares an overlay. `close_fn` is what the Back button calls to take the
overlay down.

With `opts.keep_on_navigate` set to `true` the overlay is a panel of navigation,
and it stays when the route changes. The default is off, because an overlay that
outlives the view below it is the exception, and the exception asks for itself.

**Returns** the overlay, or `null` when the integration with the history is off.
A caller that receives `null` skips the call below.

(js_yui_shell_overlay_dismissed)=
### [`yui_shell_overlay_dismissed(shell_gobj, overlay)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L5003)

Tells the shell that the overlay went away by a path that is not the Back
button.

---

## Avatars

(js_yui_shell_set_avatar_provider)=
### [`yui_shell_set_avatar_provider(shell_gobj, provider)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L5073)

Gives the function that the shell asks for the image of a user.

(js_yui_shell_refresh_avatars)=
### [`yui_shell_refresh_avatars(shell_gobj)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L5083)

Asks the provider again, and draws the avatars again.

---

## Language

(js_yui_shell_set_translator)=
### [`yui_shell_set_translator(shell_gobj, t)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L5098)

Gives the translation function of the application to the shell.

:::{important}
After the application changes its own language, it calls
`yui_shell_language_changed()`, which publishes `EV_LANGUAGE_CHANGED`. A view
that draws through a widget subscribes to that event and draws again **in the
action**. Never listen to `languageChanged` of i18next.
:::

After the user icons change (see [User icons](dom.md)), the code that fed the
registry calls `yui_shell_icons_changed(shell)`, which publishes
`EV_ICONS_CHANGED`. A view that decided, when it drew, whether a name is an icon
subscribes to that event and draws again. `C_YUI_TREEDB_TOPIC_WITH_FORM` does
this for its `icon` cells.

```js
yui_icons_set_user(nodes);
yui_shell_icons_changed(shell);
```

---

## The toolbar

(js_yui_shell_set_connection_state)=
### [`yui_shell_set_connection_state(shell_gobj, connected)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L5187)

Draws the state of the connection in the toolbar. Since gobj-ui 7.25.5 it
also publishes `EV_CONNECTION_STATE {connected}` from the shell on each
change of state (not on a repeated call with the same state). A
`C_YUI_TREEDB_TOPICS` view listens to it, with no extra code in the host:

- When the backend goes down, the view answers each form write that is in
  flight as refused.
- When the backend is up again, the view reads each open table one time
  (since gobj-ui 7.25.14). While the connection was down, the view did not
  get the node events of other writers.

The shell sends the event only to subscribers that
declare it. An app that subscribes to all the events of its shell with
the `subscriber` attr does not get it.

```js
// the app, in the handlers of its transport
function ac_on_open(gobj, event, kw, src)
{
    yui_shell_set_connection_state(gobj.priv.shell, true);
    return 0;
}
function ac_on_close(gobj, event, kw, src)
{
    yui_shell_set_connection_state(gobj.priv.shell, false);    // EV_CONNECTION_STATE {connected: false}
    return 0;
}
```

(js_yui_shell_set_toolbar_item_icon)=
### [`yui_shell_set_toolbar_item_icon(shell_gobj, item_id, icon_class)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L5223)

Changes the icon of an item of the toolbar.

(js_yui_shell_set_toolbar_item_badge)=
### [`yui_shell_set_toolbar_item_badge(shell_gobj, item_id, value, tone)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L5263)

Writes a badge on an item of the toolbar, such as a count of messages. `0`,
`""` and `null` hide it, over 99 reads `99+`. The optional `tone` (since
gobj-ui 7.26.30) is one of `danger` (the default look), `warning`, `info`,
`success`, `primary` or `link`; an unknown one is logged and drawn with the
default look.

```js
yui_shell_set_toolbar_item_badge(shell, "alarms", 3);
yui_shell_set_toolbar_item_badge(shell, "alarms", 3, "warning");
yui_shell_set_toolbar_item_badge(shell, "alarms", 0);     // hidden
```

(js_yui_shell_set_nav_item_badge)=
### [`yui_shell_set_nav_item_badge(shell_gobj, item_id, value, tone)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L5323)

The twin of the toolbar's, for an entry of a menu: of `app_config.json`, of a
submenu, or one given to `yui_shell_set_submenu()`. It reaches every nav that
draws the entry (a rail on a desktop and an icon bar on a phone draw it twice)
and is kept across their rebuilds; from then on it wins over the item's own
`badge`, which only seeds the first paint. An id no menu declares is a silent
no-op. Since gobj-ui 7.26.30.

```json
{"id": "alarms", "name": "alarms", "icon": "yi-triangle-exclamation",
 "route": "/alarms", "badge": 0}
```

```js
yui_shell_set_nav_item_badge(shell, "alarms", 3, "danger");
```

A screen reader hears each change of a badge, toolbar or menu, in the shell's
live region, as the name of the item and its count: *"Alarms, 3"*. All the
changes of one turn of the loop are said in one message, one line each (since
gobj-ui 7.26.42: before, the second change of an action replaced the first). A
badge that moves twice in the turn says its last count. A count that goes away
in the turn it came is not said.

The name is a key, translated, in every place that draws the item (since
gobj-ui 7.26.42). An entry of a menu is named by its `name`, else its
`aria_label`, else its `id`. An item of the toolbar is named by its
`aria_label`, else its `name`, else its `id`: a toolbar `name` is often a
symbol, such as `"ES/EN"`. The `validate-locales` of every app asks for the key
that names each item.

```json
{"id": "alarms", "aria_label": "alarms", "icon": "yi-triangle-exclamation", "route": "/alarms"}
```

(js_yui_shell_close_dropdown)=
### [`yui_shell_close_dropdown(shell_gobj)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/c_yui_shell.js#L5370)

Closes the dropdown of the account.

---

## The site map

(js_yui_shell_show_route_map)=
### [`yui_shell_show_route_map(shell, opts)`](https://github.com/artgins/gobj-ui.js/blob/7.28.3/src/shell_route_map.js#L488)

Opens the site map, which shows the whole surface of the navigation as a tree:
the toolbar, each menu, the sub-routes that each view declared, and the routes
that only the table of routes holds. It marks the position of the reader.
