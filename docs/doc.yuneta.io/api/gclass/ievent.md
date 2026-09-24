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
feed of a treedb — or the global `__subscribe_event__`. A refused
subscription is logged (*"No permission to subscribe event"*) and not made;
the channel stays open.

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
