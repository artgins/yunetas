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

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js)

---

## The root and the services

(js_gobj_yuno)=
### [`gobj_yuno()`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3305)

Gives the yuno, which is the root of the tree.

(js___yuno__)=
### [`__yuno__`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L81)

The variable that holds the yuno. Read it with
[`gobj_yuno()`](#js_gobj_yuno), which is the public form.

(js_gobj_services)=
### [`gobj_services()`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L2152)

Gives the names of the registered services, as a list.

(js_gobj_default_service)=
### [`gobj_default_service()`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L2166)

Gives the default service.

(js_gobj_find_service)=
### [`gobj_find_service(service_name, verbose)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L2177)

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
### [`gobj_name(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3352)

Gives the name of a gobj.

(js_gobj_gclass_name)=
### [`gobj_gclass_name(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3365)

Gives the name of the gclass of a gobj.

(js_gobj_short_name)=
### [`gobj_short_name(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3378)

Gives the short name, which is the gclass and the name. A log message uses it.

(js_gobj_full_name)=
### [`gobj_full_name(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3391)

Gives the full name, which carries the path from the yuno.

(js_gobj_yuno_name)=
### [`gobj_yuno_name()`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3316)

Gives the name of the yuno.

(js_gobj_yuno_role)=
### [`gobj_yuno_role()`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3328)

Gives the role of the yuno.

(js_gobj_yuno_id)=
### [`gobj_yuno_id()`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3340)

Gives the identifier of the yuno.

---

## Parents and children

(js_gobj_parent)=
### [`gobj_parent(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3411)

Gives the parent of a gobj.

(js_gobj_change_parent)=
### [`gobj_change_parent(gobj, parent)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L4524)

Moves a gobj to another parent, and gives `0`. A walk of the tree that is in
progress gives the gobj in its new place, not in its old place (gobj-js
7.26.28; see [`gobj_walk_gobj_children()`](#js_gobj_walk_gobj_children)).

It gives `-1` with an ERROR, and moves nothing, when the gobj or the new parent
is destroyed, when the new parent is not a gobj, or when the new parent is the
gobj itself or one of its descendants (that makes a cycle; gobj-js 7.26.29).

```javascript
gobj_change_parent(a, a_child);   // -1: a gobj cannot be its own ancestor
```

```javascript
// c1, c2 and c3 are children of root; c1 has the child g1, c2 the child g2.
gobj_walk_gobj_children_tree(root, walk_type_t.WALK_TOP2BOTTOM, (child) => {
    if(child === c1) {
        gobj_change_parent(c2, c3);
    }
    return 0;
}, 0, 0);
// The callback gets: c1, g1, c3, c2, g2 (7.26.27 gave c1, g1, c2, g2, c3, c2, g2)
```

(js_gobj_bottom_gobj)=
### [`gobj_bottom_gobj(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3471)

Gives the bottom gobj, which is the gobj that carries the work of the layer
below. A gclass of a protocol keeps its transport there.

(js_gobj_set_bottom_gobj)=
### [`gobj_set_bottom_gobj(gobj, bottom_gobj)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3483)

Writes the bottom gobj. The function writes a log warning when the gobj holds
one already.

---

## Search

(js_gobj_find_child)=
### [`gobj_find_child(gobj, jn_filter)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3882)

Gives the first child that matches a filter of attributes.

(js_gobj_match_children)=
### [`gobj_match_children(gobj, jn_filter)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3903)

Gives every child that matches a filter, as a list.

(js_gobj_match_children_tree)=
### [`gobj_match_children_tree(gobj, jn_filter)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3927)

Gives every gobj below this one that matches a filter, at any depth.

(js_gobj_match_gobj)=
### [`gobj_match_gobj(gobj, jn_filter)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3820)

Tells if one gobj matches a filter. The three functions above use it.

(js_gobj_find_gobj)=
### [`gobj_find_gobj(gobj, path)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3798)

Finds a gobj from a path.

(js_gobj_search_path)=
### [`gobj_search_path(gobj, path)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3952)

Finds a gobj from a path, and begins the search at `gobj`.

---

## Walk the tree

(js_walk_type_t)=
### [`walk_type_t`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L330)

The order the two walks take. One of `WALK_TOP2BOTTOM`, `WALK_BOTTOM2TOP` or
`WALK_BYLEVEL` must be given; with `WALK_BYLEVEL`, add `WALK_FIRST2LAST` or
`WALK_LAST2FIRST`. Same names and same values as the C `walk_type_t`.

(js_gobj_walk_gobj_children)=
### [`gobj_walk_gobj_children(gobj, walk_type, cb_walking, user_data, user_data2)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3519)

Calls `cb_walking(gobj, user_data, user_data2)` for each child. `walk_type`
chooses the order. A callback that gives a negative value stops the walk, and
the walk gives that value. `0` or a positive value goes on to the next child.

The callback can destroy any gobj, also the current one. The walk does not give
a destroyed gobj to the callback (7.26.27).

The callback can also move a gobj with
[`gobj_change_parent()`](#js_gobj_change_parent). The walk gives each gobj to
the callback one time at most (gobj-js 7.26.28):

- A gobj that left the list before the walk got to it is not given there. The
  walk gives it under its new parent, if the walk gets to that parent later.
  If the walk has already passed the new parent, the walk does not give the
  gobj or its children at all.
- A gobj that was given, and then moved to a parent that the walk gets to
  later, is not given again there. Its children are not given again either.

Before 7.26.28, the walk gave a moved gobj two times: in its old place and in
its new place.

(js_gobj_walk_gobj_children_tree)=
### [`gobj_walk_gobj_children_tree(gobj, walk_type, cb_walking, user_data, user_data2)`](https://github.com/artgins/gobj-js/blob/7.29.1/src/gobj.js#L3537)

The same walk, and it goes down to every depth. With `WALK_TOP2BOTTOM`, a
positive value from the callback skips the children of that gobj. The walk does
not go into the children of a gobj that the callback destroyed. A gobj that the
callback moves is given one time at most, as above.

```javascript
gobj_walk_gobj_children_tree(gobj, walk_type_t.WALK_TOP2BOTTOM, (child) => {
    if(gobj_is_disabled(child)) {
        return 1;           // skip its children
    }
    log_info(gobj_short_name(child));
    return 0;               // go on, and down into its children
}, 0, 0);
```
