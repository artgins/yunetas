---
title: 'gobj-ui: Component gclasses'
description: >-
  The registration function of each component of the library, and what
  each component draws.
---

# Component gclasses

Each component is a gclass, and each one needs its registration before an
application creates an instance of it. Register them next to the gclasses of
`@yuneta/gobj-js`, at start up.

```javascript
import { register_c_yui_shell, register_c_yui_nav } from "@yuneta/gobj-ui";

register_c_yui_shell();
register_c_yui_nav();
```

---

## The shell and the navigation

(js_register_c_yui_shell)=
### [`register_c_yui_shell()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_shell.js#L4480)

`C_YUI_SHELL` draws the frame of the application from one JSON file: the
toolbar, the menus and the zones. Its API is in [The shell](shell_api.md).

**A view that the shell shows again gets `EV_VIEW_SHOWN`** (gobj-ui 7.27.21).
The shell keeps a `keep_alive` view when you go to another route, and shows the
same view when you come back. The view keeps its state, but it also keeps the
data that it read when the shell created it. A view that reads its data only
then declares `EV_VIEW_SHOWN`, and reads the data again in that action. The
shell sends the event after `EV_ROUTE_CHANGED`, and only to a view that declares
it. A move inside the view (a subpath) is not a new show.

```js
// the FSM of the view: the same action as its Reload button
["ST_READY", [
    ["EV_VIEW_SHOWN",   ac_refresh,     null],
    ["EV_REFRESH",      ac_refresh,     null]
]]
// kw: {route, base, subpath}
```

`C_YUI_SERVICE_VIEW` gives the event to the view that it holds. A `C_YUI_NODE`
tree gives it to the content on the screen, and sends it when a node shows
again a content that it made before.

(js_register_c_yui_nav)=
### [`register_c_yui_nav()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_nav.js#L1235)

`C_YUI_NAV` draws the menu of the shell. Each zone takes its own layout, and the
layout changes with the width of the screen.

(js_register_c_yui_pager)=
### [`register_c_yui_pager()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_pager.js#L545)

`C_YUI_PAGER` holds a stack of pages inside one zone, for a movement that goes
in and returns.

(js_register_c_yui_wizard)=
### [`register_c_yui_wizard()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_wizard.js#L631)

`C_YUI_WIZARD` drives a sequence of steps with a way forward and a way back.

(js_register_c_yui_service_view)=
### [`register_c_yui_service_view()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_service_view.js#L386)

`C_YUI_SERVICE_VIEW` puts a view of a service in a zone of the shell.

(js_yui_mount_service_view)=
### [`yui_mount_service_view(host, spec)`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_service_view.js#L153)

Puts a view of a service in a host, from a description of it.

(js_expose_view_container)=
### [`expose_view_container(host, view)`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_service_view.js#L216)

Gives the container of a view to the host that holds it.

---

## Windows

(js_register_c_yui_window)=
### [`register_c_yui_window()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_window.js#L1847)

`C_YUI_WINDOW` draws a window that floats above the application.

A window is a dialog (since gobj-ui 7.26.43):

- It has `role="dialog"`. Its title gives its name: `aria-labelledby` points
  to the two halves of the title, `title_prefix` and `title`, so the name
  changes language with the title.
- When the window starts (`gobj_start`) because the reader pressed or typed
  something, it takes the focus. The attribute `focus` selects the element: an
  element of the window, a CSS selector, or a number (the n-th control of the
  body, from 0). The default is the window itself. A window that the
  application opens without an action of the reader (for example a window it
  restores at start up) does not take the focus. A modal window always takes
  it. A window that opens under an overlay that holds the focus (a modal
  window, a dialog, the drawer, a menu) does not take it either (since gobj-ui
  7.26.45): it opens under the overlay, and the reader finds it when the
  overlay closes. When the window closes, the focus goes back to the element
  that had it before, if the focus was in the window.
- Escape closes the window, as its ✕ does (`EV_CLOSE_WINDOW`). A subscriber
  of `EV_WINDOW_TO_CLOSE` can still stop the close or ask a question. Set
  `keyboard: false` to keep Escape. Escape does not close the window when the
  element that got the key used it: a handler that called `preventDefault()`,
  a cell editor of Tabulator, a search box with text, a `textarea` or a
  `contenteditable`. When an overlay of the shell opened after the window (a
  menu of the toolbar) is open, Escape closes that overlay first (since
  gobj-ui 7.26.45, see `yui_shell_escape_order()`).
- `modal: true` blocks the rest of the application: a backdrop in the dialog
  layer of the shell, `aria-modal="true"`, and Tab stays in the window. A modal
  window does not go to the dock.

```js
const win = gobj_create_service("keys", "C_YUI_WINDOW", {
    title_prefix: topic,            // data, not translated
    title:        "keys",           // an i18n key
    body:         $box,
    focus:        ".KEYS_FILTER",   // this box gets the focus
    modal:        true,             // the reader answers it first
}, gobj);
gobj_start(win);                    // opened by the reader: the focus goes in
```

(js_register_c_yui_window_manager)=
### [`register_c_yui_window_manager()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_window_manager.js#L988)

`C_YUI_WINDOW_MANAGER` holds the windows, and it draws the dock of them.

A floating dock keeps a band at the top or at the bottom of the viewport. It
writes the height of the band in two CSS variables on the root element:
`--yui-dock-inset-top` and `--yui-dock-inset-bottom` (since gobj-ui 7.26.43).
A window that fills the screen (the sheet of a phone, a maximized window) does
not go into the band. An inline dock and an empty dock keep no band. An
element of the application that fills the screen can use the same variables:

```css
.MY_FULL_PANEL {
    position: fixed;
    top:    var(--yui-dock-inset-top, 0px);
    bottom: var(--yui-dock-inset-bottom, 0px);
    left: 0;
    right: 0;
}
```

(js_register_c_yui_update_watch)=
### [`register_c_yui_update_watch()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_update_watch.js#L743)

`C_YUI_UPDATE_WATCH` tells the reader that a newer build of the SPA is
deployed (since gobj-ui 7.27.29). A tab that was opened before a deploy runs
the old bundle until it is reloaded, because no route of the SPA reloads the
page. The service compares the build stamp of the page with the build stamp of
the page that the server sends now. When they are different, it shows a bar
with the new version, a **Reload** button and a ✕. It does not reload the page
itself.

The build stamp is the `<meta name="yuneta-build">` that `yunetaHtmlPlugin`
writes into `index.html` at build time. The service asks the server at start,
when the tab or the window comes back to the front, when the browser is online
again, when a lazy chunk does not load, every hour, and when it gets
`EV_CHECK_BUILD`. Create it one time, outside the shell, and send it
`EV_CHECK_BUILD` when the backend connection opens:

```js
register_c_yui_update_watch();
gobj_start(gobj_create_service("__update_watch__", "C_YUI_UPDATE_WATCH", {}, yuno));

// in the EV_ON_OPEN action of the app
let update_watch = gobj_find_service("__update_watch__", true);
if(update_watch) {
    gobj_send_event(update_watch, "EV_CHECK_BUILD", {}, gobj);
}
```

(js_yui_update_watch_language_changed)=
### [`yui_update_watch_language_changed()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_update_watch.js#L759)

The bar of `C_YUI_UPDATE_WATCH` hangs off the body. With a shell, it changes
language with the shell's `EV_LANGUAGE_CHANGED`. The login screen has no
shell, so its language button must tell the watch itself (since gobj-ui
7.27.30). The function sends `EV_LANGUAGE_CHANGED` to every live watch. The
optional argument is the gobj that sends it:

```js
import {yui_update_watch_language_changed} from "@yuneta/gobj-ui/src/c_yui_update_watch.js";

// the language button of the login, after i18next has switched
switch_locale(locale);
yui_update_watch_language_changed();
```

---

## Data

(js_register_c_yui_form)=
### [`register_c_yui_form()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_form.js#L4562)

`C_YUI_FORM` is the one engine of forms of the library. The attribute
`render_mode` chooses between the form that runs a command and the form that
edits a record.

Two attributes tell what the form holds. `changes` is true when the form is
not what is stored: the reader changed it, or the record has no value for a
field that has a default, and the form shows the default. The save button
lights when `changes` is true. `edited` is true only when the reader changed
the form (typed in it, cleared it, or pasted into it) after the record was
loaded, saved or undone (since gobj-ui 7.26.39). A window or a sheet asks
*"all changes will be lost"* before it closes only when `edited` is true:
the form answers `EV_WINDOW_TO_CLOSE` with `abort_close` and `warning`.

What the reader does to a TABLE of the form is `edited` like anything typed:
a cell edited, a row added or deleted, in a nested table too, whatever the
record the form opened with (since gobj-ui 7.26.41: in 7.26.39 and 7.26.40 a
form that opened changed did not hear its tables, and closing it lost a table
edit without a question). The rows the form loads itself are not. The titles
of the tables' columns change language with the shell's
`EV_LANGUAGE_CHANGED`, like the rest of the form.

Undo shows the rows that the form loaded (since gobj-ui 7.26.43). Tabulator
keeps the objects that it gets, and a cell edit writes into them, so each
table gets its own copy of the rows of the record. Before 7.26.43 the table
got the objects of the record: an edit wrote into the record, Undo loaded the
edited record again and cleared `edited`, and the close did not ask. What a
save publishes is a copy too.

The nested tables of a row live as long as the row. A reload, Undo, Clear, a
row that the reader deletes and the destroy of the form destroy them. A row
that Tabulator draws again keeps its nested tables and their rows. A table
loads all its rows in one call and draws them one time: 300 rows load in
0.14 s (20 s before 7.26.43). A nested table is built with its rows (since
gobj-ui 7.26.45): 300 rows with a nested table each load in about 2.4 s
(3.2 s in 7.26.43 and 7.26.44, 40 s before 7.26.43), in Chromium with
Tabulator 6.6.0.

A reload, Undo, Clear or the close of the form with a cell editor open closes
the editor first, and drops what is typed in it (since gobj-ui 7.26.45).
Tabulator 6.6.0 throws when it wipes its rows with an editor open in any row
but the first. What the reader types in a cell editor sets `edited` at once,
before the editor commits (since gobj-ui 7.26.46), as a field of the form does.
Thus a host that reads `edited` before it reloads or closes the form can ask
first.

```js
if(gobj_read_bool_attr(form, "edited")) {
    // the reader typed, maybe in a cell not committed yet: ask before the reload
}
```

An edit that the reader cancels (Escape, or a box left as it was) gives back
the `edited` and `changes` that the form had when that editor opened (since
gobj-ui 7.26.47). Before, the typed text still counted, and the close asked
about a change that was gone. If something else changed the form while the
editor was open, the cancel gives nothing back.

Enter in a box of the form does not submit the form (since gobj-ui 7.26.47).
Before, the "add" button of a table was the first submit button of the form:
Enter in any field added an empty row, and in a form with one text box Enter
reloaded the page.

A `time` or `now` column of a table is edited in a `datetime-local` box, to the
second, with the same conversion as the time field of the form: an epoch in
seconds, or ISO text for a string column (since gobj-ui 7.26.46). Before, the
editor of Tabulator needed luxon, which gobj-ui does not supply: it opened
empty and failed when the reader confirmed the value.

The cell shows the time as text in the language of the application, with a
clock of 24 hours (since gobj-ui 7.26.52). It is the same text that the cells
of `C_YUI_TREEDB_TOPIC_WITH_FORM` show, and it comes from
[`fmt_date_time()`](time.md#js_fmt_date_time):

```text
es    14 mar 2026, 09:26:53
en    Mar 14, 2026, 09:26:53
```

When the language changes, the form writes the time cells of each table again,
cell by cell. It does not draw the rows again, because a row that is drawn again
builds its nested tables again, empty. A cell with an open editor is not
changed; when the editor closes, the cell shows the new language. From gobj-ui
7.26.47 to 7.26.51 the cell showed `2026-03-14 09:26:53`; before, the raw
epoch.

A date with one part cleared is not a value (since gobj-ui 7.26.47). Enter keeps
the editor open, with a red outline and `aria-invalid`, until the date is
complete or the reader presses Escape. When the reader leaves the cell, the
edit is cancelled and the cell keeps its time. Before, one Backspace and Enter
erased the stored time. To clear a time, clear all the parts of the box: an
empty box writes `null`. When the last part is clear, the box is valid again
and `aria-invalid` goes (since gobj-ui 7.26.49). In the box, the Left and Right
arrows move between the parts. Tab belongs to the table: it leaves the cell and
opens the editor of the next cell.

Enter on a time that the reader did not change cancels the edit, also on the
hour (since gobj-ui 7.26.49). The browser writes `2026-10-25T02:30:00` back as
`2026-10-25T02:30`. Before, the text did not match, and in the hour that occurs
two times in autumn the form wrote the earlier hour: -3600 s.

The time field of the form reads and writes a column as the cell editor does
(since gobj-ui 7.26.47). A column with no time shows an empty box; before, the
field showed January 1970 and a save wrote `0`. A `string` column is read and
written as ISO text; before, the field read it as a number and wrote an epoch.
A template field with no real type is an epoch (since gobj-ui 7.26.49). In
7.26.47 and 7.26.48 the field showed no time and a save wrote `null`:

```js
const template = {
    appointment: "time.writable",           // an epoch in seconds
    since:       "string.time.writable",    // an ISO text
};
```

A time field that the reader did not change is saved with the value of the
record (since gobj-ui 7.26.51), as the cell is. The form keeps the value that
filled the box and the text that the box gives back. When the box still shows
that text, the save uses that value. Before, the save converted the box again.
In the hour that occurs two times in autumn, a time of the second pass went
back -3600 s, also when the reader changed only another field:

```js
// Europe/Madrid: 01:30Z on 2026-10-25 is 02:30 of the second pass
record:          {when: 1792891800}
box:             2026-10-25T02:30
save, no change: {when: 1792891800}      // 7.26.50: 1792888200
```

A wall time that the reader types in that hour is the first pass. That limit
stays: the box holds no offset.

A field that Save refuses (an empty required box, a date with a part cleared)
has a red outline and `aria-invalid="true"`, and the line under the box is its
`aria-describedby` (since gobj-ui 7.26.49). Before, only the outline showed the
error, and a screen reader does not see an outline.

```js
const kw = {abort_close: false};
gobj_send_event(form, "EV_WINDOW_TO_CLOSE", kw, gobj);
if(kw.abort_close) {
    // the reader changed the form: ask kw.warning ("all changes will be lost")
}
```

The tables of a form select no rows. To select rows with a click, set
`selectableRows` in `tabulator_settings`; the attribute `selectable` (`true`
by default) then says whether a click selects. Before gobj-ui 7.26.45 a click
selected nothing with `selectableRows: true`, and selected a row with
`selectable: false`.

```js
gobj_create("form", "C_YUI_FORM", {
    template:           template,
    tabulator_settings: {selectableRows: 1},    // a click selects one row
}, parent);
```

The checkbox column (`with_checkbox`) also obeys `selectable`. When the form is
not selectable, the column is hidden (gobj-ui 7.26.46): the row checkbox of
Tabulator shows a tick even when the selection is refused, and the header
checkbox selects all rows without a check. The column follows each write of
`selectable`, in every table (since gobj-ui 7.26.47; in 7.26.46 the value at
the build of the table stayed). When `selectable` becomes `false`, the selected
rows are deselected (since gobj-ui 7.26.49). Before, they stayed selected, and
the reader had no checkbox and no click to remove the selection. The header checkbox is named "select all
rows" and a row checkbox "select row"; both change language with the form. A
click in a nested table selects only in that table.

```js
gobj_create("form", "C_YUI_FORM", {
    template:      template,
    with_checkbox: true,       // a checkbox column ...
    selectable:    false,      // ... is hidden: nothing can be selected
}, parent);

gobj_write_bool_attr(form, "selectable", true);   // the column is shown
```

(js_register_c_yui_json)=
### [`register_c_yui_json()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_json.js#L1526)

`C_YUI_JSON` draws a JSON value, and it draws a big one without the cost of the
whole tree. It draws it three ways, and each one answers a different question:

- **tree** — where is this value, and what is around it. Only this view can
  expand one branch at a time.
- **text** — what does the document say, exactly. Use it to read the document
  as it is written, to select part of it, or to search it with the browser.
- **graph** — what shape does the document have. This view uses a
  `C_YUI_JSON_GRAPH` child.

The `view_mode` attribute selects the first view. The switch of the toolbar
changes it.

Next to a number under a time key (`t`, `tm`, `__t__`, `from_t`, `fr_t`, and
the other keys of `TIME_FIELDS`), the tree writes the time in the language of
the application, with a clock of 24 hours. The number can be in seconds or in
milliseconds:

```text
"t": 1785312000      29 jul 2026, 10:00:00        (es, Europe/Madrid)
"t": 1785312000      Jul 29, 2026, 10:00:00       (en)
```

The viewer subscribes to `EV_LANGUAGE_CHANGED` of the shell, and when the
language changes it draws the tree again (since gobj-ui 7.26.52). The text view
and the graph are not drawn again. Before, the time was the text of
`toLocaleString()`, in the language of the browser, and it did not change with
the language of the application.

(js_register_c_yui_json_pad)=
### [`register_c_yui_json_pad()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_json_pad.js#L900)

`C_YUI_JSON_PAD` is a pad for JSON from outside the application. Paste the
JSON, and a `C_YUI_JSON` child shows it. When the text is not JSON, the last
document stays on the pad and a line under the text gives the reason.

The pad has two panes. The second pane opens with the *second json* button.
The *compare* button shows the differences of the two documents in place of
the two viewers: one row for each id of the flat form (`json2flat`), with the
kind (added, removed or changed) and the two values. The pad keeps the two
texts and its layout in `localStorage`, under the key in its `storage_key`
attribute. An empty `storage_key` keeps nothing.

(js_register_c_yui_json_graph)=
### [`register_c_yui_json_graph()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_json_graph.js#L2585)

`C_YUI_JSON_GRAPH` draws a JSON value as a graph.

(js_register_c_yui_period)=
### [`register_c_yui_period()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_period.js#L1479)

`C_YUI_PERIOD` chooses a range of time. Its calendar takes the language of the
application. The algebra behind it is in [Time and periods](time.md).

---

## TreeDB

(js_register_c_yui_treedb_topics)=
### [`register_c_yui_treedb_topics()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_treedb_topics.js#L3674)

`C_YUI_TREEDB_TOPICS` draws the topics of a treedb as cards, with a panel of
information.

**When the connection drops** (gobj-ui 7.25.13, 7.25.14). The view answers
each form write that is in flight as refused. The form stays open on the
typed values. While the session is down, the view does not get the node
events of other writers. Thus, when the session is up again, the view reads
each open table one time. An "up" event that comes before the transport of
the view is in session does not read. For example:

```js
// Session up, the tables `users` and `roles` are open.
// The session drops         -> the writes in flight are answered as refused
// EV_TRANSPORT_STATE {connected: true}, and the transport is in session
//                           -> one `nodes` read of `users` and one of `roles`
```

(js_register_c_yui_treedb_graph)=
### [`register_c_yui_treedb_graph()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_treedb_graph.js#L3899)

`C_YUI_TREEDB_GRAPH` draws a treedb as the graph that it is: the topics are the
nodes, and the links between hook and foreign key are the edges.

(js_register_c_yui_treedb_schema)=
### [`register_c_yui_treedb_schema()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_treedb_schema.js#L986)

`C_YUI_TREEDB_SCHEMA` draws the schema of a treedb. It draws it as the schema
literal in C draws it: one card for each topic, and the fields of the topic in
the sequence of the schema. The marks on a field are the marks of the literal
(`{}`, `[]`, `(↖)`, `*`, `#`), plus `(2)` on a secondary key (`pkey2s`) and
`(t)` on the time key (`tkey`). For example, the topic `binaries` of the agent
declares `'pkey2s': 'version'`, so its card draws the row `version (2)`, in
bold like the pkey.

An edge is a hook. It goes between the field that declares the hook and the
foreign-key field of the child that the hook names. The arrowhead is on the
hook, because the reference belongs to the child and points to its parent. This
is the direction of the `↖` in a foreign-key mark.

The diagram keeps the scale that it is drawn at. It does not zoom to the
container when it appears.

(js_register_c_yui_schema_editor)=
### [`register_c_yui_schema_editor()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_schema_editor.js#L4993)

`C_YUI_SCHEMA_EDITOR` edits the schemas that a yuno keeps in its
`treedb_system_schema`. The store keeps a schema in three flat topics:
`treedbs`, `topics` and `cols`. This gclass shows them as one schema: a treedb,
its topics, and the columns of a topic in their sequence.

**An edit is a draft.** A write of this gclass changes no version, and the
treedb does not use the change yet. The host makes the change public in two
steps:

1. It saves the draft with the command `save-schema` of `C_TREEDB`. This
   command increments the `topic_version` of each topic that changed, and the
   `schema_version` of the treedb, one time.
2. It puts the saved schema in use with `apply-schema`, and restarts the yuno
   that owns the treedb.

The gclass writes `topic_version` only when the operator types it in the form
of a topic. The topic list and the column screen mark the topics that hold a
draft. The host tells the gclass which topics hold a draft that is not saved,
with `EV_DRAFTS`. Each `EV_DRAFTS` replaces the previous one.

The gclass also does these operations:

- It moves a column to a different position. You drag the row.
- It shows what each flag of a column does.
- It draws the schema that you edit.
- It shows the errors that the treedb refuses. Do this before you restart the
  yuno.
- It writes the schema as its C literal. Then you can put the schema in the
  source code.
- It reads a schema and shows a plan. The plan shows each write before the
  gclass does it.

This example mounts the editor and gives it the events that it needs from the
host:

```js
// A named SERVICE: it is the `src` of the commands that it sends to the
// backend. Its parent gets EV_POSITION_CHANGED, EV_RECORD_WRITTEN and
// EV_SCHEMA_CHECKED.
let editor = gobj_create_service("schemas", "C_YUI_SCHEMA_EDITOR", {
    gobj_remote_yuno: transport,            // the treedb service, or an adapter
    treedb_name:      "treedb_system_schema",
    base_route:       "/schemas"
}, gobj);
gobj_start(editor);

// the session of the transport goes up or down
gobj_send_event(editor, "EV_TRANSPORT_STATE", {connected: true}, gobj);
// the url changes under the route of the view
gobj_send_event(editor, "EV_SHOW", {subpath: "treedb_authzs/users"}, gobj);
// the answer of saved-schema says which topics hold a draft
gobj_send_event(editor, "EV_DRAFTS", {drafts: {treedb_authzs: ["users"]}}, gobj);
```

**When the connection drops.** A drop stops the load or the write that is in
flight. The gclass reads the schemas again when the session is up again. A
write that the drop stopped shows `the connection dropped during the write`.

**When the schemas are read again.** While the schemas load, the gclass shows
a loading screen. A dialog that is open stays, but its Save shows `the schemas
are loading: wait for them`. When the load ends, the gclass closes each dialog
that shows the old schemas, and shows `the schemas were read again: open the
dialog again`. A load that fails keeps the schemas on the screen. The next
action of the operator then reads them again.

**The Save of a form while a reload is owed** (gobj-ui 7.25.16). After a load
that failed, the Save of an open form reads the schemas again first, and shows
`the schemas shown may be out of date: they are read again, and the form opens
again on them with your changes`. When the load ends, the gclass closes the
form and opens it again on the new schemas. It puts back only the fields that
the operator changed. Thus a field that the operator did not change shows the
value that the store has now, and the next Save does not write an old value.
If the column or the topic of the form is not in the new schemas, the form
does not open again, and the gclass shows `the schemas were read again and
what the form was editing is not there any more`. For example:

```js
// a load failed: a reload is owed. The column form of db.users.id is open,
// and the operator typed "Identifier" in its header.
form.querySelector(".SCHEMA_COL_FORM_SAVE").click();   // the schemas load again
// ... the load ends: the form opens again, header "Identifier",
//     the other fields as the store has them now
```

**`EV_REFRESH` while a write is in flight** (gobj-ui 7.25.16). The gclass
does the refresh when the writes end. Before, the refresh started at once, and
the writes that were still in the queue were not sent:

```js
gobj_send_event(editor, "EV_CONFIRMED", {what: "topic", topic: "users",
    model_gen: n}, editor);                             // 2 deletes in the queue
gobj_send_event(editor, "EV_REFRESH", {}, gobj);        // it waits
// ... the 2 deletes are done: the schemas load again
```

**When the host moves the view** (gobj-ui 7.25.15). The shell keeps a dialog
open when only the subpath of the url changes. If `EV_SHOW` moves the view to
a different position, the gclass closes the form, the import or the orphans,
and shows `the view moved: open the dialog again`. The export and the check
stay open. A confirmation that the operator answers on a different position
does nothing, and shows the same text. For example:

```js
gobj_send_event(editor, "EV_EDIT_COLUMN", {col: "id"}, gobj);    // on db/users
gobj_send_event(editor, "EV_SHOW", {subpath: "db"}, gobj);       // the form closes
```

The texts in `code` above are i18n keys. The locales of the application must
have them.

(js_register_c_yui_treedb_topic_with_form)=
### [`register_c_yui_treedb_topic_with_form()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_treedb_topic_with_form.js#L5729)

`C_YUI_TREEDB_TOPIC_WITH_FORM` draws one topic with the form of its records.

A cell of a `time` or `now` column shows the time in the language of the
application, with a clock of 24 hours (since gobj-ui 7.26.52), as the time
cells of `C_YUI_FORM`. When the language changes, the table rebuilds its
columns and the cells show the new language:

```js
{id: "last_seen", header: "Last seen", type: "integer",
 flag: ["persistent", "writable", "time"]}
// 1791983109 in Europe/Madrid:  es "14 oct 2026, 15:05:09"  en "Oct 14, 2026, 15:05:09"
```

Before, the cell used the language of the browser and a clock of 12 hours
(`14 oct 2026, 03:05:09 p. m.`), and the language button of the application did
not change it.

**A table under a hidden tab keeps its place** (gobj-ui 7.27.19 and 7.27.20).
`C_YUI_TREEDB_TOPICS` hides the tables of the other topics with
`display:none`. When you go back to a tab, the table shows the same rows at the
same scroll position. The table watches its own size (`EV_TABLE_RESIZED`). It
does not draw while it is hidden. It draws again when you see it, and it puts
back the scroll position. Thus the option `autoResize` of Tabulator is always
off, and the table ignores a value in `tabulator_settings`:

```js
gobj_create_pure_child(name, "C_YUI_TREEDB_TOPIC_WITH_FORM", {
    tabulator_settings: {autoResize: true},     // ignored: the table watches its size
}, parent);
```

Before, each visit to another tab moved the table approximately 12 rows up.
In 7.27.19 the table came back empty under a `C_YUI_NODE` tree. A route change
removed the table from the document and added it again, and the browser set the
scroll to 0. Since 7.27.20, `C_YUI_NODE` does not move a child that is on the
screen.

**Other changes in 7.27.19:**

- A click on a cell of an fkey to many parents (`'type': 'array'` with the
  flag `fkey`) opens the record, as a click on any other cell of data.
- In edition mode the rows keep their height. The buttons of the column
  **Operations** are as high as a line of text.
- When you stop edition mode, the table clears the selection. The checkbox
  column is hidden outside edition mode, so you cannot see or clear a selection
  there.

(js_register_c_g6_nodes_tree)=
### [`register_c_g6_nodes_tree()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_g6_nodes_tree.js#L13273)

`C_G6_NODES_TREE` draws a tree of nodes with the library G6.

**A Save that the backend does not accept** (gobj-ui 7.25.15). The Save of the
graph writes one record in `__graphs__` for each topic whose arrangement
changed. Nothing answers such a write when it is done. Thus the host must tell
the graph when the write is not done: the backend refuses it, the transport
refuses it, or there is no session. The host sends `EV_GRAPHS_WRITE_REFUSED`
with the topic. The graph then writes that topic again at the next Save.
`C_YUI_TREEDB_GRAPH` does this. A different host must do it too:

```js
// the answer to the update-node of __graphs__ is an error
gobj_send_event(engine, "EV_GRAPHS_WRITE_REFUSED", {topic: record.topic}, gobj);
```

From gobj-ui 7.25.16 the Save stays lit until a Save writes the topic. Before,
the next change of the history (or of the mode, or of the theme) turned the
Save off again, and a refusal in the mode `reading` did not show.

:::{note}
**The graphs on a touch screen.** From gobj-ui 7.23.9, the three graphs of the
library obey a finger:

- Two fingers make the zoom. G6 gives the zoom to the wheel only, and a
  telephone has no wheel.
- A long press opens the menu of the context. G6 makes that event from a
  press of the right button of the mouse. It does not read the event of the
  same name from the DOM. Thus the menu had no door on a touch screen.
- The mode `operation` moves the camera again. That mode gave the graph no
  pan and no zoom.
- The controls that a finger touches are 44 pixels. The library asks
  `(pointer: coarse)` for this. A mouse sees no change.
- The two toolbars fold behind one button. The container must be less than
  480 pixels wide. The toolbars are inside the canvas, one on each side, and
  on a telephone they hide the nodes.
- A finger selects many nodes. The two gestures use the key `shift`, and a
  telephone has no key. Thus the toolbar of edition has a button **selection
  mode** (gobj-ui 7.23.11). While the mode is on, a touch selects a card and
  a drag on the background draws the band.
- A finger moves a node (gobj-ui 7.23.14). G6 puts `touch-action: none` on
  its canvas, but it puts nothing on its nodes of HTML. Thus a drag that
  started on a card was a scroll of the page. The browser stopped the pointer
  after approximately 20 pixels.
- A press does only one thing. The library decides at the release of the
  finger: a movement is a drag, a quick release is the action of the element,
  and a long press is the menu of the context. A timer cannot decide, because
  the gesture is not complete when the timer operates.

A vibration of 15 milliseconds tells you that the press is long enough
(gobj-ui 7.23.15). The vibration is a signal only. If you then move the
finger, you get the drag. Some devices have no vibrator.
:::

**The canvas of a graph is a control.** It is in the Tab order (gobj-ui
7.26.28), so it has a name and it shows a focus ring (gobj-ui 7.26.42). The
name says which graph it is, and it is a key of the app: `json graph`, `gobj
tree graph`, `treedb graph`, `fsm graph` and `schema graph`. An app that mounts
one of these graphs needs its key in each locale, or its `prebuild` fails.

```js
// src/locales/en.js
"json graph":      "JSON graph",
"gobj tree graph": "Gobj tree graph",
"treedb graph":    "Treedb graph",
"fsm graph":       "State machine graph",
```

(js_register_c_yui_gobj_tree_js)=
### [`register_c_yui_gobj_tree_js()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_gobj_tree_js.js#L4113)

`C_YUI_GOBJ_TREE_JS` draws the tree of the gobjs of the application, for a
development panel.

(js_register_c_yui_gclass)=
### [`register_c_yui_gclass()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_gclass.js#L1904)

`C_YUI_GCLASS` shows what a gclass IS: its attributes, its commands, its
events and its states, read from the descriptor the runtime holds. It draws
the descriptor and not a document written beside it, so what it shows cannot
go stale.

(js_register_c_yui_fsm_graph)=
### [`register_c_yui_fsm_graph()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_fsm_graph.js#L1011)

`C_YUI_FSM_GRAPH` draws the state machine of a gclass as a graph: one node per
state and one edge per event that moves between two of them. It is what
`C_YUI_GCLASS` opens for the states of a gclass.

---

## Charts and maps

(js_register_c_yui_uplot)=
### [`register_c_yui_uplot()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_uplot.js#L602)

`C_YUI_UPLOT` draws a chart of a series of time with the library uPlot.

The axes and the grid take the ink of the theme, light or dark, and the chart
draws them again when the theme changes. uPlot always draws an X axis and a Y
axis; the chart inks both, also when the `axes` attribute gives only one (its
default `[{}]`). Before gobj-ui 7.26.40 the Y axis kept the black of uPlot, and
its values could not be read on the dark scheme. Since 7.28.1 every Y axis
the host did not size is as wide as its longest label
(`yui_uplot_axis_size()`): uPlot gives it a fixed 50px, which cut every value
past five digits (`150,000` drawn as `,0,000`). To keep a fixed width, give
the axis its `size` (`axes: [{}, {size: 60}]`). An application that makes a
uPlot of its own gets the same axes from `yui_uplot_axes()`
(`src/yui_uplot_ink.js`), and redraws the chart on a change of the theme
(`yui_watch_theme()` sends `EV_THEME`):

```js
import {yui_uplot_axes} from "@yuneta/gobj-ui/src/yui_uplot_ink.js";

const chart = new uPlot({
    width: 600,
    height: 300,
    series: [{}, {label: "temperature"}],
    axes: yui_uplot_axes([{}]),       // the X and the Y, with the ink of the theme
}, data, $el);
```

The texts of a time axis and of the legend are in the language of the
application, with a clock of 24 hours (since gobj-ui 7.26.52). uPlot writes them
from its own templates, in English and with 12 hours: `3pm` and `10/14` on the
axis, `2026-10-14 3:05pm` in the legend. The chart gives uPlot the option
`fmtDate` from `yui_uplot_fmt_date()` (`src/yui_uplot_time.js`): the axis shows
`15:00`, and `14/10` in Spanish; the legend shows the text of
[`fmt_date_time()`](time.md#js_fmt_date_time). When the language changes, the chart
draws the axes and the legend again. A uPlot of the application takes the same
option:

```js
import {yui_uplot_fmt_date} from "@yuneta/gobj-ui/src/yui_uplot_time.js";

const chart = new uPlot({
    width: 600,
    height: 300,
    series: [{}, {label: "temperature"}],
    fmtDate: yui_uplot_fmt_date(() => i18next.language),
}, data, $el);
// on EV_LANGUAGE_CHANGED:
chart.redraw(true, true);
chart.setLegend(null, false);
```

(js_register_c_yui_map)=
### [`register_c_yui_map()`](https://github.com/artgins/gobj-ui.js/blob/7.28.1/src/c_yui_map.js#L1166)

`C_YUI_MAP` draws a map with the library maplibre. The controls are in
[Map controls](maplibre.md).

:::{note}
maplibre 6 is ESM only. An application that draws a map emits the worker as a
`.js` file and gives it to `setWorkerUrl`. A file with the name `.mjs` arrives
with a type of content that the browser refuses.
:::
