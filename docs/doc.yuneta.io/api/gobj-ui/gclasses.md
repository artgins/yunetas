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
### [`register_c_yui_shell()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_shell.js#L3149)

`C_YUI_SHELL` draws the frame of the application from one JSON file: the
toolbar, the menus and the zones. Its API is in [The shell](shell_api.md).

(js_register_c_yui_nav)=
### [`register_c_yui_nav()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_nav.js#L1030)

`C_YUI_NAV` draws the menu of the shell. Each zone takes its own layout, and the
layout changes with the width of the screen.

(js_register_c_yui_pager)=
### [`register_c_yui_pager()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_pager.js#L540)

`C_YUI_PAGER` holds a stack of pages inside one zone, for a movement that goes
in and returns.

(js_register_c_yui_wizard)=
### [`register_c_yui_wizard()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_wizard.js#L609)

`C_YUI_WIZARD` drives a sequence of steps with a way forward and a way back.

(js_register_c_yui_service_view)=
### [`register_c_yui_service_view()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_service_view.js#L353)

`C_YUI_SERVICE_VIEW` puts a view of a service in a zone of the shell.

(js_yui_mount_service_view)=
### [`yui_mount_service_view(host, spec)`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_service_view.js#L147)

Puts a view of a service in a host, from a description of it.

(js_expose_view_container)=
### [`expose_view_container(host, view)`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_service_view.js#L210)

Gives the container of a view to the host that holds it.

---

## Windows

(js_register_c_yui_window)=
### [`register_c_yui_window()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_window.js#L1266)

`C_YUI_WINDOW` draws a window that floats above the application.

(js_register_c_yui_window_manager)=
### [`register_c_yui_window_manager()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_window_manager.js#L655)

`C_YUI_WINDOW_MANAGER` holds the windows, and it draws the dock of them.

---

## Data

(js_register_c_yui_form)=
### [`register_c_yui_form()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_form.js#L3495)

`C_YUI_FORM` is the one engine of forms of the library. The attribute
`render_mode` chooses between the form that runs a command and the form that
edits a record.

(js_register_c_yui_json)=
### [`register_c_yui_json()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_json.js#L1490)

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

(js_register_c_yui_json_pad)=
### [`register_c_yui_json_pad()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_json_pad.js#L899)

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
### [`register_c_yui_json_graph()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_json_graph.js#L2542)

`C_YUI_JSON_GRAPH` draws a JSON value as a graph.

(js_register_c_yui_period)=
### [`register_c_yui_period()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_period.js#L1462)

`C_YUI_PERIOD` chooses a range of time. Its calendar takes the language of the
application. The algebra behind it is in [Time and periods](time.md).

---

## TreeDB

(js_register_c_yui_treedb_topics)=
### [`register_c_yui_treedb_topics()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_treedb_topics.js#L3229)

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
### [`register_c_yui_treedb_graph()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_treedb_graph.js#L3737)

`C_YUI_TREEDB_GRAPH` draws a treedb as the graph that it is: the topics are the
nodes, and the links between hook and foreign key are the edges.

(js_register_c_yui_treedb_schema)=
### [`register_c_yui_treedb_schema()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_treedb_schema.js#L976)

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
### [`register_c_yui_schema_editor()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_schema_editor.js#L4648)

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
### [`register_c_yui_treedb_topic_with_form()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_treedb_topic_with_form.js#L5478)

`C_YUI_TREEDB_TOPIC_WITH_FORM` draws one topic with the form of its records.

(js_register_c_g6_nodes_tree)=
### [`register_c_g6_nodes_tree()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_g6_nodes_tree.js#L12325)

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

(js_register_c_yui_gobj_tree_js)=
### [`register_c_yui_gobj_tree_js()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_gobj_tree_js.js#L2992)

`C_YUI_GOBJ_TREE_JS` draws the tree of the gobjs of the application, for a
development panel.

(js_register_c_yui_gclass)=
### [`register_c_yui_gclass()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_gclass.js#L1535)

`C_YUI_GCLASS` shows what a gclass IS: its attributes, its commands, its
events and its states, read from the descriptor the runtime holds. It draws
the descriptor and not a document written beside it, so what it shows cannot
go stale.

(js_register_c_yui_fsm_graph)=
### [`register_c_yui_fsm_graph()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_fsm_graph.js#L1004)

`C_YUI_FSM_GRAPH` draws the state machine of a gclass as a graph: one node per
state and one edge per event that moves between two of them. It is what
`C_YUI_GCLASS` opens for the states of a gclass.

---

## Charts and maps

(js_register_c_yui_uplot)=
### [`register_c_yui_uplot()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_uplot.js#L603)

`C_YUI_UPLOT` draws a chart of a series of time with the library uPlot.

(js_register_c_yui_map)=
### [`register_c_yui_map()`](https://github.com/artgins/gobj-ui.js/blob/7.25.20/src/c_yui_map.js#L1102)

`C_YUI_MAP` draws a map with the library maplibre. The controls are in
[Map controls](maplibre.md).

:::{note}
maplibre 6 is ESM only. An application that draws a map emits the worker as a
`.js` file and gives it to `setWorkerUrl`. A file with the name `.mjs` arrives
with a type of content that the browser refuses.
:::
