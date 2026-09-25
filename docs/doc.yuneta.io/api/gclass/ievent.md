# Inter-Event GClasses

RPC-like communication between yunos over the network. A local gobj
subscribes to a remote service as if it were local.

**Source:** `kernel/c/root-linux/src/c_ievent_cli.c`, `c_ievent_srv.c`

---

(gclass-c-ievent-cli)=
## C_IEVENT_CLI

Inter-event client — connects to a remote yuno and simulates its service
as a local gobj. Handles identity-card exchange, authentication, and
subscription management.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_WAIT_CONNECTED`, `ST_WAIT_IDENTITY_CARD`, `ST_SESSION`, `ST_SUBSCRIBED` |
| **Input events** | `EV_SEND_MESSAGE`, `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE`, `EV_TIMEOUT`, `EV_DROP`, `EV_STOPPED` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `wanted_yuno_role` | `string` | Role of the remote yuno to connect to. |
| `wanted_yuno_name` | `string` | Name of the remote yuno. |
| `wanted_yuno_service` | `string` | Service name on the remote yuno. |
| `url` | `string` | Connection URL. |
| `jwt` | `string` | JSON Web Token for authentication. |
| `timeout_idack` | `integer` | Identity-card ack timeout in seconds. |

### Stopping with remote subscriptions open

`gobj_stop()` closes the transport, but the FSM stays in `ST_SESSION` until
the close arrives, so `EV_ON_CLOSE` is still published. A subscription added
or withdrawn in that window is not sent to the peer. The peer's
`C_IEVENT_SRV` drops every subscription of the channel when the channel
closes, and an added subscription is sent at the next open. Up to 7.25.4 the
`__unsubscribing__` frame went down to the stopping transport, and
`C_WEBSOCKET` or `C_TCP` logged *"Event NOT DEFINED in state"*. Example:

```c
gobj_stop_tree(priv->gobj_remote);                          // the transport starts to close
gobj_unsubscribe_event(priv->gobj_remote, EV_X, 0, gobj);   // not sent, and not an error
```

---

(gclass-c-ievent-srv)=
## C_IEVENT_SRV

Inter-event server — entry gate for authenticated service access.
Manages incoming connections, routes inter-event messages to local
services, and handles WebSocket upgrade.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_DISCONNECTED`, `ST_WAIT_IDENTITY_CARD`, `ST_SESSION` |
| **Input events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE`, `EV_TIMEOUT`, `EV_DROP`, `EV_STOPPED` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `__username__` | `string` | Authenticated username (read-only). |
| `__session_id__` | `string` | Session identifier (read-only). |
| `jwt_payload` | `json` | Decoded JWT payload. |
| `client_yuno_role` | `string` | Role of the connected client yuno. |
| `client_yuno_name` | `string` | Name of the connected client yuno. |
| `this_service` | `string` | Local service name this gate serves. |
| `authenticated` | `bool` | Whether the connection is authenticated. |
| `max_subscriptions` | `integer` | Subscriptions a peer may hold on the channel (default `5000`, `0` no limit). See *What a peer may hold*. |
| `max_subscription_size` | `integer` | Bytes, as compact json, of the `__filter__` and of the `__global__` of a peer's subscription (default `16384`, `0` no limit). |

### Lifecycle of a channel

The tree of a channel (`C_CHANNEL` → `C_IEVENT_SRV` → `C_WEBSOCKET` or
`C_PROT_TCP4H` → `C_TCP`) belongs to its gate, not to one connection.
`gobj_start_tree()` of the gate starts it. When the peer leaves, only its
`C_TCP` stops: the protocol gobj stays running and serves the next connection
that the channel accepts.

When the gate stops, all of the tree stops: each layer stops the one below
it, and `C_IEVENT_SRV` stops its protocol gobj since 7.25.5. Up to 7.25.4 a
gate stopped with a plain `gobj_stop()` (the way the yuno stops an
autostart service) left each `C_WEBSOCKET` or `C_PROT_TCP4H` running, and the
yuno exited with *"Destroying a RUNNING gobj"*. Only an owner that called
`gobj_stop_tree()` on the gate stopped it all.

A channel that is disabled and enabled again (the `C_IOGATE` commands
`disable-channel` and `enable-channel`) comes back whole: `C_CHANNEL`
restarts its tree, and `C_IEVENT_SRV` starts its protocol gobj in its start,
the mirror of its stop. The transport `C_TCP` is manual start: `C_TCP_S`
starts it for each connection it accepts.

```bash
ycommand -c 'command-yuno id=<id> service=__input_side__ command=disable-channel channel_name=^input-1$'
ycommand -c 'command-yuno id=<id> service=__input_side__ command=enable-channel channel_name=^input-1$'
```

`channel_name` is a regular expression. It selects the channels whose name
it matches: `^input-1$` on a gate with `input-1` and `input-2` selects only
`input-1`. One that matches no channel selects nothing, and the command
answers with the header of the view only. (Up to 7.25.4 each of the six
commands looped for ever on the first channel that did not match, and
blocked the yuno.) A text that is not a valid regular expression is refused with -1,
for example `channel_name=input-[` answers *"\<role^name>: channel_name is not
a valid regular expression: 'input-['"* (up to 7.25.4: *"regcomp() failed"*).

Start and stop the gate as a pair. Either declare it `"autostart": true` in
the config and let the yuno start and stop it, or do both in the owner:

```c
PRIVATE int mt_play(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    priv->gobj_input_side = gobj_find_service("__input_side__", TRUE);
    gobj_subscribe_event(priv->gobj_input_side, 0, 0, gobj);
    gobj_start_tree(priv->gobj_input_side);
    return 0;
}

PRIVATE int mt_pause(hgobj gobj)
{
    PRIVATE_DATA *priv = gobj_priv_data(gobj);

    gobj_stop_tree(priv->gobj_input_side);
    return 0;
}
```

### Subscription authz

A peer subscribes to an event of a local service with a `__subscribing__`
message. `C_IEVENT_SRV` accepts it only for a **public** output event of a
service the channel may reach. Since 7.25.5, when the yuno sets
`enable_subscription_authz` (off by default) and the event is flagged
`EVF_AUTHZ_SUBSCRIBE`, the channel's user also needs the publisher's
permission aliased `__subscribe_event__` — `read` for the `EV_TREEDB_NODE_*`
feed of a treedb and for the `EV_TRANGER_RECORD_ADDED` feed of a
`C_TRANGER` — or the global `__subscribe_event__`. A refused subscription is
logged (*"No permission to subscribe event"*) and not made; the channel stays
open. The refusal is logged once per service and event on a channel: a peer
that repeats it does not write a log line per frame. When the peer withdraws a
refused subscription later, that is logged at info level (*"its subscription
was refused"*). A withdrawal that matches no subscription of the channel, and
was not refused, is a warning (*"UNSUBSCRIBING event matches no subscription
of this channel"*), with the frame cut to 256 bytes. Both are the peer's to
repeat, so each is written at most once per 10 s per channel, with the count
of the ones not written (`suppressed`). Up to 7.25.4 each frame wrote its
line, and the no-match warning the whole frame, as big as the peer made it.

Example: a yuno that enforces the treedb feed permission, in its config:

```json
{
    "yuno": {
        "enable_subscription_authz": true
    }
}
```

Details, and how a gclass declares a guarded event:
[`YUNO_AUTH.md`](../../../../yunos/c/yuno_agent/YUNO_AUTH.md) §4.6.

### What a peer may put in a subscription

The kw of a `__subscribing__` message can carry `__config__`, `__global__`
and `__filter__`, as a local `gobj_subscribe_event()` does. `C_IEVENT_SRV`
builds the subscription from those three, and keeps of them only what a peer
may set (since 7.25.5):

| Key | What is kept |
|-----|--------------|
| `__filter__` | All of it. It decides only the peer's own deliveries. |
| `__config__` | The keys in the list below. |
| `__global__` | The keys of the peer's own: not one that starts with `_` (the framework's: `__md_iev__`, `__md_yuno__`, `__service__`, ...), and not `gbuffer`, the binary field of every kw. They come back to the peer in every event of this subscription, and to nobody else. |
| `__local__` | Nothing. The one `__local__` of a remote subscription is the reference to the channel, set by `C_IEVENT_SRV`. |
| any other key | Nothing. |

What is dropped is logged as a warning, *"SUBSCRIBING keys a peer may not
set, ignored"*, at most once per 10 s per channel. A `__filter__` or a
`__global__` bigger than `max_subscription_size` refuses the subscription
(*"SUBSCRIBING refused, bigger than max_subscription_size"*, same pace).

Up to 7.25.4 only `__config__` was filtered, and the publish shared ONE kw
with every subscriber, so a peer's subscription changed the event of every
subscriber after it: its `__global__` forged keys of the event (a
`topic_name`, a `node`), its `__local__` removed them, the filters of the
later subscribers were evaluated on the forged kw, and the back-metadata of
the gate (`__md_iev__`, with the peer's user name and channel) reached a
local subscriber. A `gbuffer` in the peer's `__global__` was taken for a
pointer when the event was serialized back to it, and crashed the yuno. Now
`gobj_publish_event()` gives a subscription with a `__local__` or a
`__global__` a twin of the kw of its own
([`gobj_publish_event()`](#gobj_publish_event)), and `C_IEVENT_SRV` changes a
kw that somebody else holds only on a copy.

Of `__config__` the list of keys a peer may set has one key:

| Key | Meaning |
|-----|---------|
| `__first_shot__` | Read by the publisher in its `mt_subscription_added()`: `false` asks it not to send its current state when the subscription is made. |

Any other key is removed before the subscription is made, and logged as a
warning (*"SUBSCRIBING __config__ keys a peer may not set, ignored"*). Three
of those keys change how the framework delivers the event, and a peer must
never have them:

- `__hard_subscription__` makes the subscription survive the close of the
  channel. The channel is static, so it went to the next user of it, who got
  the feed without asking and past the subscription authz.
- `__own_event__` stops the publish loop when the delivery to this
  subscription fails. With the channel closed every delivery fails, and every
  subscriber after it lost the event.
- `__rename_event_name__` delivers the event to `C_IEVENT_SRV` under another
  name, one of its own inputs among them.

Up to 7.25.4 the peer's `__config__` went through whole. Also since 7.25.5,
the close of a channel removes every subscription it made with force, hard
or not.

Example: a SPA subscribes without the first shot, and that key arrives:

```js
gobj_subscribe_event(gobj_remote, "EV_REALTIME_TRACK", {
    __config__: {__first_shot__: false},
    __filter__: {id: device_id}
}, gobj);
```

A key that a publisher reads from a peer is added to the list in
`c_ievent_srv.c` (`peer_subscription_config_keys`), and documented here.

A peer withdraws a subscription with an `__unsubscribing__` message that
repeats what it subscribed. The kw is filtered the same way, and compared
with what the peer SENT: the `__global__` that `C_IEVENT_SRV` stores carries
its own back-metadata too, which the peer never repeats. Up to 7.25.4 it was
compared whole, and a subscription with a `__global__` could not be withdrawn
until the channel closed.

Example: a C client tags the events of its subscription, and withdraws it:

```C
json_t *kw = json_pack("{s:{s:s}, s:{s:s}}",
    "__filter__", "topic_name", "devices",
    "__global__", "tag", "devices_view"     // comes back in each event
);
gobj_subscribe_event(gobj_remote, EV_REALTIME_TRACK, json_incref(kw), gobj);
gobj_unsubscribe_event(gobj_remote, EV_REALTIME_TRACK, kw, gobj);   // the same kw
```

### What a peer may hold

Every subscription costs a scan of the publisher's subscriptions when it is
made, and one more on every publish of its event. A peer could make them
without end: 20000 subscriptions of one peer blocked the event
loop for 80 s, and every later publish took 13 ms. So a channel holds at most
`max_subscriptions` of its peer. Beyond it a subscription is refused, logged
once (*"SUBSCRIBING refused, the peer holds max_subscriptions"*), and not
again until the peer is under the cap. A subscription that repeats one the
peer holds replaces it, and takes no room.

A gate whose peers subscribe per device (two subscriptions per device, in the
SPAs of hidraulia) raises the cap in the `kw` of the `C_IEVENT_SRV` of its
channel tree:

```json
{
    "name": "input-(^^__range__^^)",
    "gclass": "C_IEVENT_SRV",
    "kw": {
        "max_subscriptions": 20000
    }
}
```

### What the gate stamps

The gate writes who sent a message into the kw it hands on, and overwrites
whatever the peer put there under the same key. In a command, a stats
request and an event, `__username__` is the channel's authenticated user
(set by `C_AUTHZ`), whatever the peer sent. In the routing stack of an event
(`__md_iev__`), `__username__`, `input_channel` and `input_service` are the
gate's. The command parser's authz check reads that `__username__`.

Example: a peer sends `list-yunos` with a `__username__` of its own in the kw
of the command:

```json
{"__username__": "admin"}
```

The service gets `"__username__": "bob"` when the channel's user is `bob`.
Up to 7.25.4 [`kw_set_dict_value()`](#kw_set_dict_value) kept a key that
already existed, so the peer's `admin` reached the service.

`tests/c/c_ievent_srv_peer_subs`.
