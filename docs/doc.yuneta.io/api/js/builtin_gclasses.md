# Built-in GClasses

`@yuneta/gobj-js` ships three built-in GClasses that every application
uses. They must be registered explicitly before any GObject of that
type can be created.

## `C_YUNO`

The application root. Always the first GClass registered.

```javascript
import { register_c_yuno } from "@yuneta/gobj-js";
register_c_yuno();
```

### Key attributes

`yuno_name`, `yuno_role`, `yuno_id`, `yuno_version`, `yuno_release`,
`yuneta_version`, `required_services`, `tracing`, `start_date`,
`node_uuid`, `__username__`.

## `C_TIMER`

Manages timeouts and periodic timers. Delivers `EV_TIMEOUT` (one-shot)
or `EV_TIMEOUT_PERIODIC` to its subscriber.

```javascript
import {
    register_c_timer,
    set_timeout,
    set_timeout_periodic,
    clear_timeout,
} from "@yuneta/gobj-js";

register_c_timer();

// One-shot timeout (ms)
set_timeout(timer_gobj, 5000);

// Periodic timeout
set_timeout_periodic(timer_gobj, 1000);

// Cancel
clear_timeout(timer_gobj);
```

**Events published:** `EV_TIMEOUT`, `EV_TIMEOUT_PERIODIC`

**Attributes:** `subscriber`, `periodic`, `msec`

## `C_IEVENT_CLI`

Inter-event client — proxies a remote Yuneta service over WebSocket so
it looks like a local GObject. Used to communicate with backend yunos.

```javascript
import { register_c_ievent_cli } from "@yuneta/gobj-js";
register_c_ievent_cli();

const remote = gobj_create_service("backend", "C_IEVENT_CLI", {
    url:                "ws://localhost:1991",
    wanted_yuno_role:   "agent",
    wanted_yuno_service: "agent",
    jwt:                "...",
}, gobj_yuno());
```

### Key attributes

`url`, `jwt`, `wanted_yuno_role`, `wanted_yuno_name`,
`wanted_yuno_service`, `remote_yuno_role`, `remote_yuno_name`,
`remote_yuno_service`.

Once connected, `C_IEVENT_CLI` republishes every event that arrives
from the remote side as a local event on itself — subscribe to it the
same way you would to any local GObject.
