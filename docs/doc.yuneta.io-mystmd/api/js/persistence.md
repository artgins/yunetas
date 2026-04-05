# Persistence

Attributes declared with the `SDF_PERSIST` flag are automatically
saved and loaded by the framework via the callbacks registered at
[bootstrap](bootstrap.md).

The reference backend is `localStorage` (browser) via `dbsimple.js`.

## Low-level dbsimple API

```javascript
db_load_persistent_attrs(gobj, keys)
db_save_persistent_attrs(gobj, keys)
db_remove_persistent_attrs(gobj, keys)
db_list_persistent_attrs(gobj, keys)
```

These read/write directly from `localStorage`. Use them as the
callbacks passed to `gobj_start_up`.

## Via gobj (delegates to registered persistence fns)

```javascript
gobj_load_persistent_attrs(gobj, keys)
gobj_save_persistent_attrs(gobj, keys)
gobj_remove_persistent_attrs(gobj, keys)
gobj_list_persistent_attrs(gobj, keys)
```

Application code normally calls these, not the low-level `db_*`
functions. They dispatch to whichever backend was registered at
`gobj_start_up` time.

## Local storage helpers (no "store" parameter)

Also exposed for ad-hoc use outside the attribute persistence system:

```javascript
kw_get_local_storage_value(key, default_value, create)   // create=false by default
kw_set_local_storage_value(key, value)
kw_remove_local_storage_value(key)
```

## Plugging in a different backend

Pass your own callbacks to `gobj_start_up` — for example, an
`IndexedDB`-backed store, an HTTP REST backend, or an in-memory mock for
tests. The only contract is that each callback takes `(gobj, keys)`
and either loads/saves/removes/lists the attributes whose names appear
in the `keys` array.
