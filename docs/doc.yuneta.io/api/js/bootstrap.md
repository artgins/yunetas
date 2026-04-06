# Framework Bootstrap

Before any GClass is registered or any GObject is created, the
framework must be started with `gobj_start_up()`. This hooks up the
global persistence callbacks, command parser, and stats parser.

```javascript
gobj_start_up(
    jn_global_settings,         // JSON object or null
    load_persistent_attrs_fn,   // fn(gobj, keys)
    save_persistent_attrs_fn,   // fn(gobj, keys)
    remove_persistent_attrs_fn, // fn(gobj, keys)
    list_persistent_attrs_fn,   // fn(gobj, keys)
    global_command_parser_fn,   // fn or null
    global_stats_parser_fn      // fn or null
)
```

For a browser app using `localStorage` as the persistence backend, a
typical bootstrap looks like:

```javascript
import {
    gobj_start_up,
    db_load_persistent_attrs,
    db_save_persistent_attrs,
    db_remove_persistent_attrs,
    db_list_persistent_attrs,
} from "@yuneta/gobj-js";

gobj_start_up(
    null,
    db_load_persistent_attrs,
    db_save_persistent_attrs,
    db_remove_persistent_attrs,
    db_list_persistent_attrs,
    null,
    null
);
```

After bootstrap you typically register your built-in GClasses
(`register_c_yuno`, `register_c_timer`, `register_c_ievent_cli`, …) and
then create the Yuno root:

```javascript
import {
    register_c_yuno,
    register_c_timer,
    gobj_create_yuno,
} from "@yuneta/gobj-js";

register_c_yuno();
register_c_timer();

const yuno = gobj_create_yuno("my_app", "C_YUNO", {
    yuno_role: "frontend",
    yuno_name: "main",
});
```
