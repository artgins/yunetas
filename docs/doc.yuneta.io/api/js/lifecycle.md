# GObject Lifecycle

## Creation

| Function | Purpose |
|---|---|
| `gobj_create(name, gclass_name, attrs, parent)` | Generic child |
| `gobj_create_yuno(name, gclass_name, attrs)` | Application root (only one per process) |
| `gobj_create_service(name, gclass_name, attrs, yuno)` | Named service under the Yuno |
| `gobj_create_default_service(name, gclass_name, attrs, yuno)` | The default service (receives `gobj_play`/`gobj_pause`) |
| `gobj_create_volatil(name, gclass_name, attrs, parent)` | Volatile child (auto-destroyed on parent destroy) |
| `gobj_create_pure_child(name, gclass_name, attrs, parent)` | Pure child (opaque to events outside parent) |

## Start and stop

```javascript
gobj_start(gobj)           // calls mt_start
gobj_stop(gobj)            // calls mt_stop
gobj_start_children(gobj)
gobj_stop_children(gobj)
gobj_start_tree(gobj)      // start this gobj and every descendant
gobj_stop_tree(gobj)
```

## Play and pause (default service)

```javascript
gobj_play(gobj)
gobj_pause(gobj)
```

These are applied to the default service via its lifecycle methods.

## Destroy

```javascript
gobj_destroy(gobj)
```

Destroy is recursive: all children are destroyed first. Subscriptions
are automatically cleaned up.

## Status queries

```javascript
gobj_is_running(gobj)      // → boolean
gobj_is_playing(gobj)      // → boolean
gobj_is_destroying(gobj)   // → boolean
gobj_is_volatil(gobj)      // → boolean
gobj_is_pure_child(gobj)   // → boolean
```
