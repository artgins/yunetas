# Hierarchy & Navigation

GObjects form a parent-child tree. The root is the **Yuno**. Services
live directly under the Yuno. Each GObject has exactly one parent
(except the Yuno itself).

```
Yuno
 ├── Service "auth"     (C_IEVENT_CLI)
 ├── Service "main"     (C_MY_SERVICE)
 │    └── Child "sub"   (C_SOME_GCLASS)
 └── Service "timer"    (C_TIMER)
```

## Moving around the tree

```javascript
gobj_parent(gobj)
gobj_yuno()                         // → the Yuno root
gobj_default_service()              // → the default service under the Yuno
```

## Names

```javascript
gobj_name(gobj)
gobj_short_name(gobj)
gobj_full_name(gobj)
gobj_gclass_name(gobj)

gobj_yuno_name(gobj)
gobj_yuno_role(gobj)
gobj_yuno_id(gobj)
```

## Finding gobjs

```javascript
gobj_find_child(gobj, kw_filter)
gobj_find_service(name, verbose)

gobj_find_gobj(gobj, path)          // resolve a path string
gobj_search_path(gobj, path)
```

## Walking the tree

```javascript
gobj_walk_gobj_children(gobj, walk_type, cb_walking, user_data, user_data2)
gobj_walk_gobj_children_tree(gobj, walk_type, cb_walking, user_data, user_data2)
```

`walk_type` controls the traversal order (pre-order, post-order, etc.).
The callback `cb_walking(gobj, user_data, user_data2)` is invoked for
each visited node; return a non-zero value to stop walking early.
