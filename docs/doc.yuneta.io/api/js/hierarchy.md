---
title: 'JS: Hierarchy and Navigation'
description: >-
  The tree of gobjs, the names of a gobj, the search functions and the
  walk of the children.
---

# Hierarchy and Navigation

The gobjs build a tree of parents and children. The root is the **yuno**. The
services are under the yuno. Each gobj has one parent, and the yuno has none.

```
Yuno
 ├── Service "auth"     (C_IEVENT_CLI)
 ├── Service "main"     (C_MY_SERVICE)
 │    └── Child "sub"   (C_SOME_GCLASS)
 └── Service "timer"    (C_TIMER)
```

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js)

---

## The root and the services

(js_gobj_yuno)=
### [`gobj_yuno()`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L2619)

Gives the yuno, which is the root of the tree.

(js___yuno__)=
### [`__yuno__`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L80)

The variable that holds the yuno. Read it with
[`gobj_yuno()`](#js_gobj_yuno), which is the public form.

(js_gobj_services)=
### [`gobj_services()`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L1590)

Gives the names of the registered services, as a list.

(js_gobj_default_service)=
### [`gobj_default_service()`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L1604)

Gives the default service.

(js_gobj_find_service)=
### [`gobj_find_service(service_name, verbose)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L1615)

Finds a service by its name. With `verbose` set to `true` the function writes a
log error when the service does not exist.

:::{note}
A gobj that
[`gobj_create_default_service()`](lifecycle.md#js_gobj_create_default_service)
built is not in this register, and this function gives `null` for it.
:::

---

## Names

(js_gobj_name)=
### [`gobj_name(gobj)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L2666)

Gives the name of a gobj.

(js_gobj_gclass_name)=
### [`gobj_gclass_name(gobj)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L2679)

Gives the name of the gclass of a gobj.

(js_gobj_short_name)=
### [`gobj_short_name(gobj)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L2692)

Gives the short name, which is the gclass and the name. A log message uses it.

(js_gobj_full_name)=
### [`gobj_full_name(gobj)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L2705)

Gives the full name, which carries the path from the yuno.

(js_gobj_yuno_name)=
### [`gobj_yuno_name()`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L2630)

Gives the name of the yuno.

(js_gobj_yuno_role)=
### [`gobj_yuno_role()`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L2642)

Gives the role of the yuno.

(js_gobj_yuno_id)=
### [`gobj_yuno_id()`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L2654)

Gives the identifier of the yuno.

---

## Parents and children

(js_gobj_parent)=
### [`gobj_parent(gobj)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L2725)

Gives the parent of a gobj.

(js_gobj_change_parent)=
### [`gobj_change_parent(gobj, parent)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L3764)

Moves a gobj to another parent.

(js_gobj_bottom_gobj)=
### [`gobj_bottom_gobj(gobj)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L2785)

Gives the bottom gobj, which is the gobj that carries the work of the layer
below. A gclass of a protocol keeps its transport there.

(js_gobj_set_bottom_gobj)=
### [`gobj_set_bottom_gobj(gobj, bottom_gobj)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L2797)

Writes the bottom gobj. The function writes a log warning when the gobj holds
one already.

---

## Search

(js_gobj_find_child)=
### [`gobj_find_child(gobj, jn_filter)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L3160)

Gives the first child that matches a filter of attributes.

(js_gobj_match_children)=
### [`gobj_match_children(gobj, jn_filter)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L3181)

Gives every child that matches a filter, as a list.

(js_gobj_match_children_tree)=
### [`gobj_match_children_tree(gobj, jn_filter)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L3205)

Gives every gobj below this one that matches a filter, at any depth.

(js_gobj_match_gobj)=
### [`gobj_match_gobj(gobj, jn_filter)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L3098)

Tells if one gobj matches a filter. The three functions above use it.

(js_gobj_find_gobj)=
### [`gobj_find_gobj(gobj, path)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L3076)

Finds a gobj from a path.

(js_gobj_search_path)=
### [`gobj_search_path(gobj, path)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L3230)

Finds a gobj from a path, and begins the search at `gobj`.

---

## Walk the tree

(js_walk_type_t)=
### [`walk_type_t`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L306)

The order the two walks take. One of `WALK_TOP2BOTTOM`, `WALK_BOTTOM2TOP` or
`WALK_BYLEVEL` must be given; with `WALK_BYLEVEL`, add `WALK_FIRST2LAST` or
`WALK_LAST2FIRST`. Same names and same values as the C `walk_type_t`.

(js_gobj_walk_gobj_children)=
### [`gobj_walk_gobj_children(gobj, walk_type, cb_walking, user_data, user_data2)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L2831)

Calls `cb_walking(gobj, user_data, user_data2)` for each child. `walk_type`
chooses the order. A callback that gives a value that is not `0` stops the walk.

(js_gobj_walk_gobj_children_tree)=
### [`gobj_walk_gobj_children_tree(gobj, walk_type, cb_walking, user_data, user_data2)`](https://github.com/artgins/gobj-js/blob/7.25.9/src/gobj.js#L2849)

The same walk, and it goes down to every depth.
