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
| `send_type` | `integer` | `0` = rotated one-at-a-time, `1` = broadcast to all. |

### Commands

| Command | Description |
|---------|-------------|
| `view-channels` | List managed channels and their state. |
| `enable-channel` / `disable-channel` | Enable or disable a channel. |
| `trace-on-channel` / `trace-off-channel` | Toggle tracing on a channel. |
| `reset-stats-channel` | Reset channel statistics. |

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
| `repeated_key` | `string` | Field with the key of a message (e.g. `id`) to ignore repeated messages. Empty (default): nothing is ignored. |
| `repeated_field` | `string` | Field, or path with `` ` ``, compared per key. Default `seq`. |
| `repeated_mode` | `string` | `not_newer` (default): ignore a value not above the last of its key. `equal`: ignore only the same value. |
| `repeated_preload` | `integer` | Latest records of the queue read at start to rebuild the last value of each key. Default `100000`. |

### Commands

| Command | Description |
|---------|-------------|
| `queue_mark_pending` | Mark a queued message as pending. |
| `queue_mark_notpending` | Mark a queued message as not pending. |

### Ignoring repeated messages

A queue link delivers **at least once**. When an ack is lost, the sender sends
the message again. A gate that forwards what it receives through a
`C_QIOGATE` then puts the same message in its queue twice.

Set `repeated_key` to stop that. The gate compares the `repeated_field` of each
message with the last value it passed for the same key. A value that is not
above it (`not_newer`), or that is equal to it (`equal`), is **not queued**:
the gate gives it back as queued, so its sender acks it upstream as usual, and
counts it in the stats.

```json
{
    "name": "output-0",
    "gclass": "C_QIOGATE",
    "kw": {
        "topic_name": "gate_events",
        "tkey": "tm",
        "repeated_key": "id",
        "repeated_field": "seq"
    }
}
```

- The field must **grow per key**: a sequence number, or a time in
  milliseconds. A time in seconds drops the second message of a second.
- A path reaches into the message: `` "repeated_field": "data`tm" ``.
- `not_newer` also drops a message that arrives **out of order**. For a sender
  that does not keep order, use `equal`, which drops only a repeat of the last
  value.
- `not_newer` drops every message of a key whose field goes **back on
  purpose** (a device replaced, a counter restarted) until the field passes
  the old value again. Only `repeated_msgs` shows it. Make the sequence
  survive such a reset before you turn the filter on, or use `equal`.
- The last value of each key lives in memory. At start, the gate rebuilds it
  from the latest `repeated_preload` records of its queue, because a restart is
  exactly when the senders resend. It logs *"Repeated messages: last value of
  each key rebuilt from the queue"* with the number of records and keys.
- A message without the key or the field is queued as always.
- Behind a `C_MQIOGATE` with `method: lastdigits`, a key always goes to the
  same child, so each child sees every repeat of its keys.

With `repeated_key` set, the stats add three counters:

| Stat | Meaning |
|------|---------|
| `repeated_msgs` | Messages not queued because they were repeats. |
| `repeated_unchecked` | Messages queued without a check: no key or no field. |
| `repeated_keys` | Keys with a last value in memory. |

With `repeated_key` empty (the default) the gate does nothing new: every
message is queued, and the stats are the same as before.

The filter cannot see a repeat that the gate itself sends. When the ack of
the **next** hop is lost, the gate sends the same queue row again, and it is
the receiver of that hop that gets it twice.

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
