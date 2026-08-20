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
### [`register_c_yui_shell()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_shell.js#L3020)

`C_YUI_SHELL` draws the frame of the application from one JSON file: the
toolbar, the menus and the zones. Its API is in [The shell](shell.md).

(js_register_c_yui_nav)=
### [`register_c_yui_nav()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_nav.js#L1027)

`C_YUI_NAV` draws the menu of the shell. Each zone takes its own layout, and the
layout changes with the width of the screen.

(js_register_c_yui_pager)=
### [`register_c_yui_pager()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_pager.js#L540)

`C_YUI_PAGER` holds a stack of pages inside one zone, for a movement that goes
in and returns.

(js_register_c_yui_wizard)=
### [`register_c_yui_wizard()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_wizard.js#L607)

`C_YUI_WIZARD` drives a sequence of steps with a way forward and a way back.

(js_register_c_yui_service_view)=
### [`register_c_yui_service_view()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_service_view.js#L353)

`C_YUI_SERVICE_VIEW` puts a view of a service in a zone of the shell.

(js_yui_mount_service_view)=
### [`yui_mount_service_view(host, spec)`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_service_view.js#L147)

Puts a view of a service in a host, from a description of it.

(js_expose_view_container)=
### [`expose_view_container(host, view)`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_service_view.js#L210)

Gives the container of a view to the host that holds it.

---

## Windows

(js_register_c_yui_window)=
### [`register_c_yui_window()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_window.js#L1266)

`C_YUI_WINDOW` draws a window that floats above the application.

(js_register_c_yui_window_manager)=
### [`register_c_yui_window_manager()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_window_manager.js#L645)

`C_YUI_WINDOW_MANAGER` holds the windows, and it draws the dock of them.

---

## Data

(js_register_c_yui_form)=
### [`register_c_yui_form()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_form.js#L3021)

`C_YUI_FORM` is the one engine of forms of the library. The attribute
`render_mode` chooses between the form that runs a command and the form that
edits a record.

(js_register_c_yui_json)=
### [`register_c_yui_json()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_json.js#L938)

`C_YUI_JSON` draws a JSON value, and it draws a big one without the cost of the
whole tree.

(js_register_c_yui_json_graph)=
### [`register_c_yui_json_graph()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_json_graph.js#L1110)

`C_YUI_JSON_GRAPH` draws a JSON value as a graph.

(js_register_c_yui_period)=
### [`register_c_yui_period()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_period.js#L1436)

`C_YUI_PERIOD` chooses a range of time. Its calendar takes the language of the
application. The algebra behind it is in [Time and periods](time.md).

---

## TreeDB

(js_register_c_yui_treedb_topics)=
### [`register_c_yui_treedb_topics()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_treedb_topics.js#L2364)

`C_YUI_TREEDB_TOPICS` draws the topics of a treedb as cards, with a panel of
information.

(js_register_c_yui_treedb_graph)=
### [`register_c_yui_treedb_graph()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_treedb_graph.js#L2301)

`C_YUI_TREEDB_GRAPH` draws a treedb as the graph that it is: the topics are the
nodes, and the links between hook and foreign key are the edges.

(js_register_c_yui_treedb_schema)=
### [`register_c_yui_treedb_schema()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_treedb_schema.js#L820)

`C_YUI_TREEDB_SCHEMA` draws the schema of a treedb. It draws it as the schema
literal in C draws it: one card for each topic, and the fields of the topic in
the sequence of the schema. The marks on a field are the marks of the literal
(`{}`, `[]`, `(↖)`, `*`, `#`).

An edge is a hook. It goes between the field that declares the hook and the
foreign-key field of the child that the hook names. The arrowhead is on the
hook, because the reference belongs to the child and points to its parent. This
is the direction of the `↖` in a foreign-key mark.

The diagram keeps the scale that it is drawn at. It does not zoom to the
container when it appears.

(js_register_c_yui_schema_editor)=
### [`register_c_yui_schema_editor()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_schema_editor.js#L3300)

`C_YUI_SCHEMA_EDITOR` edits the schemas that a yuno keeps in its
`treedb_system_schema`. The store keeps a schema in three flat topics:
`treedbs`, `topics` and `cols`. This gclass shows them as one schema: a treedb,
its topics, and the columns of a topic in their sequence.

The gclass writes the two versions that make a change public. Thus the operator
does not set them:

- `topic_version` makes public a change of the columns of a topic.
- `schema_version` makes public the schema.

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

(js_register_c_yui_treedb_topic_with_form)=
### [`register_c_yui_treedb_topic_with_form()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_treedb_topic_with_form.js#L2860)

`C_YUI_TREEDB_TOPIC_WITH_FORM` draws one topic with the form of its records.

(js_register_c_g6_nodes_tree)=
### [`register_c_g6_nodes_tree()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_g6_nodes_tree.js#L6373)

`C_G6_NODES_TREE` draws a tree of nodes with the library G6.

(js_register_c_yui_gobj_tree_js)=
### [`register_c_yui_gobj_tree_js()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_gobj_tree_js.js#L2123)

`C_YUI_GOBJ_TREE_JS` draws the tree of the gobjs of the application, for a
development panel.

---

## Charts and maps

(js_register_c_yui_uplot)=
### [`register_c_yui_uplot()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_uplot.js#L507)

`C_YUI_UPLOT` draws a chart of a series of time with the library uPlot.

(js_register_c_yui_map)=
### [`register_c_yui_map()`](https://github.com/artgins/gobj-ui.js/blob/7.0.1/src/c_yui_map.js#L904)

`C_YUI_MAP` draws a map with the library maplibre. The controls are in
[Map controls](maplibre.md).

:::{note}
maplibre 6 is ESM only. An application that draws a map emits the worker as a
`.js` file and gives it to `setWorkerUrl`. A file with the name `.mjs` arrives
with a type of content that the browser refuses.
:::
