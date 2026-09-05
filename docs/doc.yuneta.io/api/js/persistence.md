---
title: 'JS: Persistence'
description: >-
  Saving and loading the attributes that carry SDF_PERSIST, and the
  local-storage back end that gobj-js ships.
---

# Persistence

An attribute with the flag `SDF_PERSIST` stays between two visits of the same
browser. The framework holds no store of its own. It calls the four functions
that [`gobj_start_up()`](bootstrap.md#js_gobj_start_up) receives, and the
package ships a back end on the local storage.

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js),
[`src/dbsimple.js`](https://github.com/artgins/gobj-js/blob/7.16.4/src/dbsimple.js)

:::{important}
Always name the attributes that you save:
`gobj_save_persistent_attrs(gobj, "attr_name")`. The call with no name saves
every attribute that carries `SDF_PERSIST`, which is wasteful, and it can write
over an attribute that the caller did not touch. The C side has the same rule.
:::

---

## The API of the framework

Each function accepts a string, a list of names or an object of names in `keys`.

(js_gobj_save_persistent_attrs)=
### [`gobj_save_persistent_attrs(gobj, keys)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L1662)

Saves the attributes that `keys` names.

(js_gobj_load_persistent_attrs)=
### [`gobj_load_persistent_attrs(gobj, keys)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L1645)

Loads the attributes that `keys` names.

(js_gobj_remove_persistent_attrs)=
### [`gobj_remove_persistent_attrs(gobj, keys)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L1679)

Deletes the attributes that `keys` names from the store.

(js_gobj_list_persistent_attrs)=
### [`gobj_list_persistent_attrs(gobj, keys)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/gobj.js#L1697)

Gives the attributes that the store holds.

---

## The back end on the local storage

Give these four functions to
[`gobj_start_up()`](bootstrap.md#js_gobj_start_up), and the API above works on
the local storage of the browser.

```javascript
import {
    gobj_start_up,
    db_load_persistent_attrs,
    db_save_persistent_attrs,
    db_remove_persistent_attrs,
    db_list_persistent_attrs
} from "@yuneta/gobj-js";

gobj_start_up(
    settings,
    db_load_persistent_attrs,
    db_save_persistent_attrs,
    db_remove_persistent_attrs,
    db_list_persistent_attrs,
    command_parser,
    stats_parser
);
```

(js_db_save_persistent_attrs)=
### [`db_save_persistent_attrs(gobj, keys)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/dbsimple.js#L69)

Writes the attributes to the local storage.

(js_db_load_persistent_attrs)=
### [`db_load_persistent_attrs(gobj, keys)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/dbsimple.js#L38)

Reads the attributes from the local storage.

(js_db_remove_persistent_attrs)=
### [`db_remove_persistent_attrs(gobj, keys)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/dbsimple.js#L96)

Deletes the attributes from the local storage.

(js_db_list_persistent_attrs)=
### [`db_list_persistent_attrs(gobj, keys)`](https://github.com/artgins/gobj-js/blob/7.16.4/src/dbsimple.js#L123)

Gives the attributes that the local storage holds.

---

The three helpers below the back end are in
[`kw` helpers](helpers_kw.md#js_kw_get_local_storage_value). A write that fails
gives `-1`, so a caller sees the failure. The local storage of a browser has a
limit of size, and a private window can refuse a write.
