# Gateway GClasses

Message routing, multiplexing, and persistent queuing.

**Source:** `kernel/c/root-linux/src/c_channel.c`, `c_iogate.c`,
`c_qiogate.c`, `c_mqiogate.c`

---

(gclass-c-channel)=
## C_CHANNEL

Base communication channel — wraps a transport + protocol stack into a
single open/closed abstraction with message-rate statistics.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_CLOSED`, `ST_OPENED` |
| **Input events** | `EV_ON_MESSAGE`, `EV_SEND_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE`, `EV_DROP` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `opened` | `bool` | Channel state (read-only). |
| `subscriber` | `pointer` | Gobj receiving output events. |
| `txMsgsec` / `rxMsgsec` | `integer` | Current message rate per second (stats). |
| `maxtxMsgsec` / `maxrxMsgsec` | `integer` | Peak message rates (stats). |

### Message rates

The rates of `C_CHANNEL` and `C_IOGATE` are computed when their stats are
read: the messages since the previous computation, divided by the time
elapsed in milliseconds. A read less than a second after the previous one
answers the last rates and does not move the window. So the rate is exact
over any interval of a second or more, and two readers share one window
(each sees the rate since the other's read). Up to 7.25.21 the time was taken
in whole seconds (a read after 1.9 s divided by 1: +90 %), and every read
moved the window, so the messages of a read under a second went into no
rate.

What the design does not do, by choice (there is no timer: a gate nobody
reads costs nothing):

- The window is as wide as the reader's interval: a reader every 60 s gets
  the average of those 60 s, and a burst of one second inside them is
  diluted.
- The maxima (`maxtxMsgsec` / `maxrxMsgsec`) move only when a window closes,
  so they are maxima of the readers' windows, not of any second.
- The messages before the first read go into no rate: the first read opens
  the window.

A rate per second whatever the readers do needs a tick of its own, as the
control center keeps (its `timeout` attr).

A `C_CHANNEL` read by its parent (a gate) answers its BARE counters, which
the gate merges into its own stats.

---

(gclass-c-iogate)=
## C_IOGATE

I/O gate — routes messages between channels and upper-layer destinations.
Supports one-at-a-time (rotated) or broadcast delivery.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |
| **Input events** | `EV_ON_MESSAGE`, `EV_SEND_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE`, `EV_DROP`, `EV_STOPPED` |
| **Output events** | `EV_ON_MESSAGE`, `EV_ON_OPEN`, `EV_ON_CLOSE` |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `persistent_channels` | `bool` | Keep channels alive across reconnections. |
| `send_type` | `integer` | `0` = rotated one-at-a-time, `1` = broadcast to all. A kw can choose for itself with `__send_type__`. |

### Send to all

With `send_type` 1 every open channel gets the message. A message in a
gbuffer is read out by the channel that sends it (`C_PROT_TCP4H` appends it
to its frame), so every open channel but the last gets a COPY of what is
left to read -- with its secret flag, label and address -- and the last one
takes the original gbuffer. So a sender that kept a reference to that
gbuffer finds it read out after a send to all, as after a send to one. A
kw without a gbuffer is shared (`kw_incref()`). Up to 7.25.20 the gbuffer was
shared: the first channel sent the message, and the others an empty frame,
which a `C_PROT_TCP4H` peer drops the connection on (*"frame_length cannot be
0"*).

```C
gbuffer_t *gbuf = gbuffer_create(len, len);
gbuffer_append(gbuf, data, len);
gobj_send_event(gate, EV_SEND_MESSAGE,
    json_pack("{s:I, s:i}",
        "gbuffer", (json_int_t)(uintptr_t)gbuf,     // the kw owns it
        "__send_type__", 1                          // to every open channel
    ),
    gobj
);
```

`tests/c/c_tcp` (`test8`).

### Commands

| Command | Description |
|---------|-------------|
| `view-channels` | List managed channels and their state. |
| `enable-channel` / `disable-channel` | Enable or disable a channel. |
| `trace-on-channel` / `trace-off-channel` | Toggle tracing on a channel. |
| `reset-stats-channel` | Reset channel statistics. |

### Stats

`stats-yuno` on a `C_IOGATE` service answers the usual envelope, with the
gate's counters and rates under `data`. Up to 7.25.21 its `mt_stats` returned
the bare counters and the command answered `-1` with no comment (the same
for `C_QIOGATE`).

```bash
ycommand -c 'stats-yuno id=<id> service=__input_side__'
# {"result": 0, "data": {"txMsgs": 1519, "rxMsgs": 1520, "txMsgsec": 12, "rxMsgsec": 12, ...}}
```

From C, a parent reads it the same way:

```C
json_t *jn_stats = gobj_stats(gobj_gate, NULL, 0, gobj);
json_t *jn_data = kw_get_dict(gobj, jn_stats, "data", 0, 0);   // the counters
json_int_t txMsgs = kw_get_int(gobj, jn_data, "txMsgs", 0, 0);
JSON_DECREF(jn_stats)
```

---

(gclass-c-qiogate)=
## C_QIOGATE

Queued I/O gate — extends C_IOGATE with **persistent message queuing**
backed by timeranger. Provides ack-based delivery with automatic retry.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |
| **Input events** | same as C_IOGATE + `EV_TIMEOUT` |
| **Output events** | same as C_IOGATE |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `tranger_path` | `string` | Path for the timeranger queue storage. |
| `tranger_database` | `string` | Database name. |
| `topic_name` | `string` | Queue topic name. |
| `max_pending_acks` | `integer` | Maximum messages in flight without ack. |
| `timeout_ack` | `integer` | Ack timeout in seconds. |
| `timeout_poll` | `integer` | Queue poll interval in seconds. |
| `backup_queue_size` | `integer` | Backup retention size. |
| `alert_queue_size` | `integer` | Queue size threshold for alerts. |
| `msgs_in_queue` | `integer` (stats) | Messages in the queue, not acked yet: a gauge, read live. |
| `pending_acks` | `integer` (stats) | Messages sent and waiting for their ack: a gauge, read live. |

`msgs_in_queue` and `pending_acks` are stats attributes, so they are in the
stats of the gate itself and in those of a `C_MQIOGATE` above it, one entry
per child (up to 7.25.21 only the gate's own `mt_stats` said them, which a
`C_MQIOGATE` never asks: the size of a persistent link's queue could not be
read through it).

```bash
ycommand -c 'stats-yuno id=<id> service=__output_side__'
# {"output-qiogate-aire": {"msgs_in_queue": 12, "pending_acks": 3, ...}, ...}
```

### Commands

| Command | Description |
|---------|-------------|
| `queue_mark_pending` | Mark a queued message as pending. |
| `queue_mark_notpending` | Mark a queued message as not pending. |

---

(gclass-c-mqiogate)=
## C_MQIOGATE

Multiple-queue I/O gate. It sends each message to one of its `C_QIOGATE`
children, chosen from a key of the message, or to all of them. Each child is
a persistent queue towards one destination, so an outage of one destination
does not stop the others. This is the entry gateway that spreads the keys of
a treedb over several nodes (see [the key](#philosophy-the-key)).

Its children must all be `C_QIOGATE`.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |
| **Input events** | same as C_IOGATE |
| **Output events** | same as C_IOGATE |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `method` | `string` | `lastdigits` (default): send to ONE child. `broadcast`: send to every child. |
| `digits` | `integer` | How many characters at the end of the key value choose the child. Default `1`. |
| `key` | `string` | Field of the message kw that holds the key. Default `id`. It must be a **string**. |

### How `lastdigits` chooses the child

1. Take the last `digits` characters of `kw[key]`.
2. Read them as a decimal number if they are all digits. Otherwise read
   them as hexadecimal.
3. The child is that number modulo the number of children (child 0 is the
   first).

With two children and `digits: 1`, `id: "dev-0042"` goes to `output-0`
(2 % 2 = 0) and `id: "dev-0043"` goes to `output-1`. With `id: "a1b2c3"` the
last character `3` is used.

The same key always goes to the same child while the number of children
stays the same. **If you add or remove a child, keys move to other
children.** Plan the number of children, or move the data of the keys with
them.

A key that is not a string, or is empty, is logged as an error and goes to
the first child.

### Example

Two destinations, one queue each (the queue kw is the `C_QIOGATE` one,
shortened here):

```json
{
    "name": "__output_side__",
    "gclass": "C_MQIOGATE",
    "kw": {
        "method": "lastdigits",
        "digits": 1,
        "key": "id"
    },
    "children": [
        {
            "name": "output-0",
            "gclass": "C_QIOGATE",
            "kw": {"topic_name": "gate_events", "tkey": "tm"},
            "children": [
                {"name": "output-0", "gclass": "C_IOGATE", "children": [
                    {"name": "output-0", "gclass": "C_CHANNEL", "children": [
                        {"name": "output-0", "gclass": "C_PROT_TCP4H", "children": [
                            {"name": "output-0", "gclass": "C_TCP",
                             "kw": {"url": "tcp://node-0:2000"}}
                        ]}
                    ]}
                ]}
            ]
        },
        {
            "name": "output-1",
            "gclass": "C_QIOGATE",
            "kw": {"topic_name": "gate_events", "tkey": "tm"},
            "children": [
                {"name": "output-1", "gclass": "C_IOGATE", "children": [
                    {"name": "output-1", "gclass": "C_CHANNEL", "children": [
                        {"name": "output-1", "gclass": "C_PROT_TCP4H", "children": [
                            {"name": "output-1", "gclass": "C_TCP",
                             "kw": {"url": "tcp://node-1:2000"}}
                        ]}
                    ]}
                ]}
            ]
        }
    ]
}
```

### Commands

| Command | Description |
|---------|-------------|
| `view-channels` | List child queue gates and their state. |
