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

### Commands

| Command | Description |
|---------|-------------|
| `queue_mark_pending` | Mark a queued message as pending. |
| `queue_mark_notpending` | Mark a queued message as not pending. |

---

(gclass-c-mqiogate)=
## C_MQIOGATE

Multiple-queue I/O gate — distributes messages to child C_QIOGATE
instances based on key hashing.

| Property | Value |
|----------|-------|
| **States** | `ST_STOPPED`, `ST_IDLE` |
| **Input events** | same as C_IOGATE |
| **Output events** | same as C_IOGATE |

### Key attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `method` | `string` | Routing method. |
| `digits` | `integer` | Hash digits for distribution. |
| `key` | `string` | JSON field name used as routing key. |

### Commands

| Command | Description |
|---------|-------------|
| `view-channels` | List child queue gates and their state. |
