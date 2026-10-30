# Built-in GClasses

`@yuneta/gobj-js` ships three built-in GClasses that every application
uses. They must be registered explicitly before any GObject of that
type can be created.

(js_register_c_yuno)=
## [`C_YUNO`](https://github.com/artgins/gobj-js/blob/7.29.4/src/c_yuno.js#L556)

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
## [`C_TIMER`](https://github.com/artgins/gobj-js/blob/7.29.4/src/c_timer.js#L326)

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

**Who hears it:** the `subscriber` attr, else, for a pure child, its parent,
which the timer subscribes in `mt_create`. That is the C timer's rule, and the
timeout is always PUBLISHED (gobj-js 7.26.22). Up to 7.26.21 a pure child SENT
it to its parent, also when it had a `subscriber`. This is a BREAKING change
for a host that ends its timer, or itself, in the timer's `EV_TIMEOUT`: it now
destroys the timer under its publication, and `gobj_destroy()` logs the ERROR
*"Destroying a gobj while its tree is publishing"*. Post an event to itself
instead (see [`gobj_destroy()`](lifecycle.md#js_gobj_destroy)). To re-arm or
clear the timer in the action is correct.

```javascript
// The host is the parent: it declares EV_TIMEOUT in its FSM.
let timer = gobj_create_pure_child("deadline", "C_TIMER", {}, gobj);
set_timeout(timer, 15000);
```

(js_set_timeout)=
### [`set_timeout(gobj, msec)`](https://github.com/artgins/gobj-js/blob/7.29.4/src/c_timer.js#L355)

Arms a timer of one shot. It publishes `EV_TIMEOUT` when the time passes.

(js_set_timeout_periodic)=
### [`set_timeout_periodic(gobj, msec)`](https://github.com/artgins/gobj-js/blob/7.29.4/src/c_timer.js#L373)

Arms a periodic timer. It publishes `EV_TIMEOUT_PERIODIC` at each period.

(js_clear_timeout)=
### [`clear_timeout(gobj)`](https://github.com/artgins/gobj-js/blob/7.29.4/src/c_timer.js#L391)

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
## [`C_IEVENT_CLI`](https://github.com/artgins/gobj-js/blob/7.29.4/src/c_ievent_cli.js#L1785)

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

A message that names a service goes to that service, if the service takes the
event as public. A message that names no service is published only if a
registered gclass declares its event PUBLIC
([`gobj_find_event_type()`](state_machine.md#js_gobj_find_event_type) with
`EVF_PUBLIC_EVENT`; 7.26.24 took any declaration, 7.26.25 requires it public).
The transport drops any other message with a WARNING, and masks the name of the
event. The events that the transport says itself (`EV_ON_OPEN`, `EV_ON_CLOSE`,
`EV_ON_OPEN_ERROR`, `EV_ON_ID_NAK`, `EV_TIMEOUT`, `EV_ON_MESSAGE`, and
`EV_IDENTITY_CARD_ACK` in the session) are never taken from the server
(7.26.25). An error of the server is a WARNING, not an ERROR, and these errors
also close the channel, as in the C transport:

- Before the session, an event that is not `EV_IDENTITY_CARD_ACK`.
- A `dst_role` that is not the role of this yuno, or a `dst_yuno` that is not
  its name. An empty `dst_role` or `dst_yuno` is legal.

The transport closes such a channel with the code **4002**, not 1000, so
`EV_ON_CLOSE` tells that the session was cut (7.26.25). A browser can close a
websocket only with 1000 or 3000-4999.

An event that the named service does not take as public is dropped, and the
session stays open. So are `EV_MT_STATS` and `EV_MT_COMMAND` from the server:
the C client runs them and answers, a browser client does not run what its
server asks. Each one gives a WARNING that names what was asked, masked
(7.26.26).

### Reconnection

A dropped link tries again after `timeout_retry` (5 s). The delay doubles up to
`timeout_retry_max` (60 s), with ±20% jitter. The delay starts again from
`timeout_retry` when a session is reached, and on `mt_start`. A session that the
transport closes with 4002 does not count, so a server that breaks each session
at once is not tried again at the first delay forever (7.26.25). But a session
that lived longer than `timeout_retry_max` counts: its fault starts the delay
again from `timeout_retry` (7.26.26).

The base of the first delay is at least 1000 ms (7.26.27). The ±20% jitter
puts the first delay itself between 800 and 1200 ms. A lower `timeout_retry` is
raised to 1000, with one WARNING. Before 7.26.27, a `timeout_retry` of 0 stayed
0, because 0 doubled is 0. Then a server that closed the link before the ack
got hundreds of connections a second.

```javascript
gobj_create_service("backend", "C_IEVENT_CLI", {
    url:                "wss://backend.example.com:1991",
    timeout_retry:      2000,       // first delay, ms
    timeout_retry_max:  30000,      // longest delay, ms
}, gobj_yuno());
```

A refused identity card (an ack with `result < 0`) publishes `EV_ON_ID_NAK`.
Then the channel closes with 1000. No session was reached, so the close also
gives `EV_ON_OPEN_ERROR` (one WARNING), and the link tries again, as in the C
transport. If a subscriber shows the refusal on `EV_ON_ID_NAK`, it must not
show a second error on the `EV_ON_OPEN_ERROR` that follows.

### The end of a session

`EV_ON_CLOSE` and `EV_ON_OPEN_ERROR` carry `code`, `reason` and `was_clean`
(7.26.24). The `reason` is masked and capped. The transport logs nothing when a
session ends cleanly: a stop, a logout, or a close of the server with 1000 or
1001. A session that is CUT (not clean, or any other code, such as 1006, 1008
or 1011) gives one WARNING with the code and the masked reason. A socket that
closes before it opens is the `EV_ON_OPEN_ERROR` WARNING.

```javascript
function ac_on_close(gobj, event, kw, src)
{
    if(!kw.was_clean || (kw.code !== 1000 && kw.code !== 1001)) {
        show_banner(gobj, kw.code);     // the transport logged the WARNING
    }
    return 0;
}
```

A pure child `C_IEVENT_CLI` with no `subscriber` publishes to its parent
(7.26.22). This is a BREAKING change for such a host: its FSM must declare
`EV_ON_CLOSE` and `EV_ON_OPEN_ERROR`, or each one is *"Event NOT DEFINED in
state"*.
