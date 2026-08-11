# Built-in GClasses

`@yuneta/gobj-js` ships three built-in GClasses that every application
uses. They must be registered explicitly before any GObject of that
type can be created.

(js_register_c_yuno)=
## [`C_YUNO`](https://github.com/artgins/gobj-js/blob/7.10.0/src/c_yuno.js#L286)

The application root. Always the first GClass registered.
`register_c_yuno()` registers it.

```javascript
import { register_c_yuno } from "@yuneta/gobj-js";
register_c_yuno();
```

### Key attributes

`yuno_name`, `yuno_role`, `yuno_id`, `yuno_version`, `yuno_release`,
`yuneta_version`, `required_services`, `tracing`, `start_date`,
`node_uuid`, `__username__`.

(js_register_c_timer)=
## [`C_TIMER`](https://github.com/artgins/gobj-js/blob/7.10.0/src/c_timer.js#L323)

Manages timeouts and periodic timers. Delivers `EV_TIMEOUT` (one-shot)
or `EV_TIMEOUT_PERIODIC` to its subscriber. `register_c_timer()` registers it.

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

(js_set_timeout)=
### [`set_timeout(gobj, msec)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/c_timer.js#L341)

Arms a timer of one shot. It publishes `EV_TIMEOUT` when the time passes.

(js_set_timeout_periodic)=
### [`set_timeout_periodic(gobj, msec)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/c_timer.js#L359)

Arms a periodic timer. It publishes `EV_TIMEOUT_PERIODIC` at each period.

(js_clear_timeout)=
### [`clear_timeout(gobj)`](https://github.com/artgins/gobj-js/blob/7.10.0/src/c_timer.js#L377)

Stops the timer.

:::{important}
A timer measures a **time**: a schedule, a window of inactivity, or a backoff.
It is not the way to run something later. To leave the stack that you stand on,
use [`gobj_post_event()`](events.md#js_gobj_post_event). A timer of one
millisecond costs a child gobj and the name of the event, because every
continuation arrives as `EV_TIMEOUT`, and the `machine` trace stops saying what
occurred.

A timer that asks the same question again is polling, and Yuneta does not poll.
Refresh from an action of the user, or subscribe to an event of the producer.
:::

(js_register_c_ievent_cli)=
## [`C_IEVENT_CLI`](https://github.com/artgins/gobj-js/blob/7.10.0/src/c_ievent_cli.js#L1480)

Inter-event client. It carries a remote Yuneta service over a websocket, so it
looks like a local gobj. It is the way to speak to a backend yuno.
`register_c_ievent_cli()` registers it.

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
same way you will to any local GObject.
