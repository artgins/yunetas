---
title: 'JS: Bootstrap'
description: >-
  Starting the framework in a browser: gobj_start_up, the registration of
  the gclasses and the creation of the yuno.
---

# Bootstrap

An application starts the framework one time, registers its gclasses, builds the
yuno and plays it.

**Source code:** [`src/gobj.js`](https://github.com/artgins/gobj-js/blob/7.16.2/src/gobj.js)

---

(js_gobj_start_up)=
## [`gobj_start_up()`](https://github.com/artgins/gobj-js/blob/7.16.2/src/gobj.js#L762)

Starts the framework. Call it one time, before every other call.

```javascript
gobj_start_up(
    jn_global_settings,
    load_persistent_attrs_fn,
    save_persistent_attrs_fn,
    remove_persistent_attrs_fn,
    list_persistent_attrs_fn,
    global_command_parser_fn,
    global_stats_parser_fn
)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `jn_global_settings` | `object` | The settings of the application. A gclass reads its own section by its name. |
| `load_persistent_attrs_fn` | `function` | Loads the persistent attributes. |
| `save_persistent_attrs_fn` | `function` | Saves the persistent attributes. |
| `remove_persistent_attrs_fn` | `function` | Deletes the persistent attributes. |
| `list_persistent_attrs_fn` | `function` | Lists the persistent attributes. |
| `global_command_parser_fn` | `function` | The parser of the commands, for a gclass that gives no table. |
| `global_stats_parser_fn` | `function` | The parser of the statistics. |

**Returns**

Returns `0`.

**Notes**

The function declares the global events of the framework, and
`EV_STATE_CHANGED` is one of them.

Give the four functions of [`dbsimple`](persistence.md) to keep the persistent
attributes in the local storage of the browser.

---

## A complete start

```javascript
import {
    gobj_start_up, gobj_create_yuno, gobj_start_tree, gobj_play,
    register_c_yuno, register_c_timer, register_c_ievent_cli,
    db_load_persistent_attrs, db_save_persistent_attrs,
    db_remove_persistent_attrs, db_list_persistent_attrs,
    command_parser, stats_parser
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

register_c_yuno();
register_c_timer();
register_c_ievent_cli();
register_c_my_app();

const yuno = gobj_create_yuno("my_app", "C_MY_APP", {});
gobj_start_tree(yuno);
gobj_play(yuno);
```

The order matters. Every gclass that a creation names must have its registration
first.

---

(js_YUNETA_VERSION)=
## [`YUNETA_VERSION`](https://github.com/artgins/gobj-js/blob/7.16.2/src/gobj.js#L57)

The version of the framework, as a string.

:::{note}
The package `@yuneta/gobj-js` tracks the version of the SDK, and the two move on
their own line between two releases. Read the version of the package from its
`package.json`, and read this constant for the version that the runtime reports.
:::
