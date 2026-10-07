(module-mqtt-tr2q)=
# tr2q (MQTT session queues)

The persistent queue that holds the messages of an MQTT session while they are
in flight or waiting their turn: the QoS 1 and 2 messages a client has not
acknowledged yet, and the ones the broker keeps for a session that is
disconnected. It is the MQTT twin of
[`tr_queue`](../api/timeranger2/tr_queue.md), on timeranger2 as well, with
the MQTT state of each message (QoS, retain, dup, direction, origin and the
step of the QoS handshake) kept in the `user_flag` of its record.

Source code:

- [`tr2q_mqtt.h`](https://github.com/artgins/yunetas/blob/7.26.6/modules/c/mqtt/src/tr2q_mqtt.h)
- [`tr2q_mqtt.c`](https://github.com/artgins/yunetas/blob/7.26.6/modules/c/mqtt/src/tr2q_mqtt.c)

## How a queue works

A queue is a timeranger2 topic with a `rowid` key, one per session and
direction: `<client_id>-IN` for the messages the session receives,
`<client_id>-OUT` for the ones it sends ([`build_queue_name()`](#build_queue_name)).
`C_PROT_MQTT2` opens both at the start of a session, and `C_MQTT_BROKER` opens
the `-OUT` one of a disconnected session for the time of an append.

- **On disk**, every message is a record of the topic. While it is not done
  with, its `user_flag` carries `TR2Q_MSG_PENDING`; finishing it
  ([`tr2q_unload_msg()`](#tr2q_unload_msg)) clears that bit on disk, so a
  restart loads only what is still pending ([`tr2q_load()`](#tr2q_load)).
- **In memory**, each pending message is a `q2_msg_t` in one of two lists:
  - **inflight** -- the messages being delivered, with their content in
    memory, at most `max_inflight_messages` of them (`0`: no limit);
  - **queued** -- the ones beyond that limit, with only their metadata (the
    content stays on disk, read when the message moves to inflight with
    [`tr2q_move_from_queued_to_inflight()`](#tr2q_move_from_queued_to_inflight)).
- **`first_rowid`** (in the topic's `topic_var.json`) is the rowid of the first
  pending message: a load starts there instead of reading the whole topic.
- **Backup**: once the topic holds `backup_queue_size` records, and nothing is
  in flight or queued, [`tr2q_check_backup()`](#tr2q_check_backup) moves it
  to a backup and re-creates it empty, so a queue does not grow for ever.

```C
typedef struct {            // a queue (fields to read, not to write)
    json_t *tranger;
    json_t *topic;
    char topic_name[256];
    size_t max_inflight_messages;
    dl_list_t dl_inflight;
    dl_list_t dl_queued;
    uint64_t first_rowid;
    BOOL load_failed;       // the last tr2q_load() did not read every pending message
    ...
} tr2_queue_t;

typedef struct {            // a message of the queue
    DL_ITEM_FIELDS
    tr2_queue_t *trq;
    md2_record_ex_t md_record;  // its timeranger2 metadata: __t__, rowid, user_flag
    json_int_t rowid;
    uint16_t mid;               // the MQTT packet identifier
    BOOL inflight;              // TRUE in dl_inflight, FALSE in dl_queued
    json_t *kw_record;          // its content, NULL while it is not loaded
} q2_msg_t;
```

### The `user_flag` of a message

| Bits | Mask | Values |
|---|---|---|
| 0 | `TR2Q_MSG_PENDING` | set while the message is not done with |
| 4-5 | `TR2Q_QOS_MASK` | `mosq_m_qos0`, `mosq_m_qos1`, `mosq_m_qos2` |
| 6 | `TR2Q_RETAIN_MASK` | `mosq_m_retain` |
| 7 | `TR2Q_DUP_MASK` | `mosq_m_dup` |
| 8-9 | `TR2Q_DIR_MASK` | `mosq_md_in`, `mosq_md_out` |
| 10-11 | `TR2Q_ORIG_MASK` | `mosq_mo_client`, `mosq_mo_broker` |
| 12-15 | `TR2Q_STATE_MASK` | `mosq_ms_publish_qos0` ... `mosq_ms_queued`, the step of the QoS handshake |

They are read and written in memory with `static inline` helpers, and written
to disk with [`tr2q_save_hard_mark()`](#tr2q_save_hard_mark):

| Helper | Does |
|---|---|
| `msg_flag_set_pending()` / `msg_flag_get_pending()` | the pending bit |
| `msg_flag_set_qos()` / `msg_flag_get_qos()` | the QoS bits as `mqtt_msg_qos_t` |
| `msg_flag_set_qos_level()` / `msg_flag_get_qos_level()` | the QoS as `0`, `1`, `2` |
| `msg_flag_set_retain()` / `msg_flag_get_retain()` | the retain bit |
| `msg_flag_set_dup()` / `msg_flag_get_dup()` | the dup bit |
| `msg_flag_set_direction()` / `msg_flag_get_direction()` | `mosq_md_in` / `mosq_md_out` |
| `msg_flag_set_origin()` / `msg_flag_get_origin()` | `mosq_mo_client` / `mosq_mo_broker` |
| `msg_flag_set_state()` / `msg_flag_get_state()` | the `mqtt_msg_state_t` |

And the lists are walked with `tr2q_first_inflight_msg()`,
`tr2q_first_queued_msg()`, `tr2q_next_msg()` (and their `last` / `prev`), or
the macros `Q2MSG_FOREACH_FORWARD_INFLIGHT(trq, msg)` /
`Q2MSG_FOREACH_FORWARD_QUEUED(trq, msg)` and their `_SAFE` forms, which allow
unloading the current message. `tr2q_inflight_size()` and
`tr2q_queued_size()` count them; `tr2q_msg_rowid()` and `tr2q2_msg_time()`
read a message's rowid and `__t__`.

---

(tr2q_open)=
## `tr2q_open()`

`tr2q_open()` opens a queue, creating its topic the first time (the create is
idempotent). The tranger must be started already (`tranger2_startup()`).
The queue is opened EMPTY: [`tr2q_load()`](#tr2q_load) brings in the pending
messages of a previous run.

```C
tr2_queue_t *tr2q_open(
    json_t *tranger,
    const char *topic_name,
    const char *tkey,
    system_flag2_t system_flag,
    size_t max_inflight_messages,
    size_t backup_queue_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `tranger` | `json_t *` | The tranger the queue lives in (not owned). |
| `topic_name` | `const char *` | The topic, one per queue (see [`build_queue_name()`](#build_queue_name)). |
| `tkey` | `const char *` | The field of the record that carries its time; the MQTT queues use `"tm"`. |
| `system_flag` | `system_flag2_t` | Flags of the topic. Its key type is forced to `sf_rowid_key`, whatever is passed; the MQTT queues pass `0`. |
| `max_inflight_messages` | `size_t` | How many messages may be inflight at once; beyond it they go to the queued list. `0` is no limit, `1` delivers one by one, in order. |
| `backup_queue_size` | `size_t` | The size of the topic that triggers a backup in [`tr2q_check_backup()`](#tr2q_check_backup); `0` never backs up. It is written in the topic's `topic_var.json`, and only by a master tranger. |

**Returns**

The queue, yours until [`tr2q_close()`](#tr2q_close). `NULL` (logged) when it
cannot be allocated or its topic cannot be created.

**Example** — the input queue of a session, as `C_PROT_MQTT2` opens it:

```C
char queue_name[NAME_MAX];
build_queue_name(queue_name, sizeof(queue_name), client_id, mosq_md_in);

tr2_queue_t *trq_in_msgs = tr2q_open(
    tranger_queues,
    queue_name,         // "<client_id>-IN"
    "tm",
    0,                  // system_flag
    max_inflight_messages,
    backup_queue_size
);
if(!trq_in_msgs) {
    return -1;  // Error already logged
}
tr2q_load(trq_in_msgs);
```

---

(tr2q_close)=
## `tr2q_close()`

`tr2q_close()` frees the queue and every message it holds in memory. The
messages stay on disk as they are: the pending ones are loaded again by the
next [`tr2q_load()`](#tr2q_load). The topic is not closed: the tranger closes
it at `tranger2_shutdown()`.

```C
void tr2q_close(tr2_queue_t *trq);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr2_queue_t *` | The queue. It is freed: do not use it, nor any of its `q2_msg_t`, afterwards. |

**Returns** — nothing.

**Example** — append to the queue of a disconnected session, and close it,
as `C_MQTT_BROKER` does:

```C
tr2_queue_t *trq_out_msgs = tr2q_open(tranger_queues, queue_name, "tm", 0, 0, 0);
tr2q_append(trq_out_msgs, tm, kw_mqtt_msg, user_flag);
tr2q_close(trq_out_msgs);
```

---

(tr2q_set_verbose)=
## `tr2q_set_verbose()`

`tr2q_set_verbose()` turns on or off a trace of every message
[`tr2q_load()`](#tr2q_load) brings in (`💾💾 ==> LOAD MSG tr2q <topic>,
session '<client_id>', topic '<topic>', qos <n>, retain <n>`, followed by the
record).

```C
void tr2q_set_verbose(tr2_queue_t *trq, BOOL verbose);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr2_queue_t *` | The queue. |
| `verbose` | `BOOL` | `TRUE` to trace the loads. |

**Returns** — nothing.

**Example** — `C_PROT_MQTT2` ties it to its `messages2` trace level:

```C
if(gobj_trace_level(gobj) & MESSAGES2) {
    tr2q_set_verbose(trq_in_msgs, TRUE);
}
tr2q_load(trq_in_msgs);
```

---

(tr2q_load)=
## `tr2q_load()`

`tr2q_load()` brings into memory the messages of the topic that are still
pending (`TR2Q_MSG_PENDING` set), from the saved `first_rowid` on. Each one
lands inflight, with its content, while there is room
(`max_inflight_messages`), and queued, with only its metadata, after that.
Then the rowid of the first pending message is saved as the new
`first_rowid` (with none pending, the size of the topic).

```C
int tr2q_load(tr2_queue_t *trq);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr2_queue_t *` | The queue, just opened. |

**Returns**

`0`. `-1` when `trq` is `NULL` (logged), or when the load could not read every
pending message: *"Queue loaded without some of its messages: its first_rowid
is not moved nor saved"*. The messages it did read ARE in the queue, but
`first_rowid` keeps its old value and `load_failed` is set, so that the next
load, once the store is repaired, finds the ones this one missed, and
[`tr2q_check_backup()`](#tr2q_check_backup) refuses to back up the topic
meanwhile. Up to 7.25.4 `first_rowid` moved past them and they were lost.

**Example** — see [`tr2q_open()`](#tr2q_open).

---

(tr2q_load_all)=
## `tr2q_load_all()`

`tr2q_load_all()` brings into memory every message of a range of rowids,
pending or NOT, the same way [`tr2q_load()`](#tr2q_load) does (inflight first,
queued after the limit). It does not read nor save `first_rowid`. It is a tool
for inspecting a queue (nothing in the MQTT module calls it): a message that
is done with comes back into the lists, and unloading it again only clears a
bit that is already clear.

```C
int tr2q_load_all(tr2_queue_t *trq, int64_t from_rowid, int64_t to_rowid);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr2_queue_t *` | The queue. |
| `from_rowid` | `int64_t` | First rowid, `0` from the beginning. |
| `to_rowid` | `int64_t` | Last rowid, `0` to the end. |

**Returns**

`0`, always: a failed read is logged by timeranger2 and not reported here.

**Example** — list every message of a queue, done or not:

```C
tr2_queue_t *trq = tr2q_open(tranger, "sensor-17-OUT", "tm", 0, 0, 0);
tr2q_load_all(trq, 0, 0);
tr2q_list_msgs(trq);
tr2q_close(trq);
```

---

(tr2q_load_all_by_time)=
## `tr2q_load_all_by_time()`

`tr2q_load_all_by_time()` is [`tr2q_load_all()`](#tr2q_load_all) with a range
of times (the `__t__` of the records) instead of rowids.

```C
int tr2q_load_all_by_time(tr2_queue_t *trq, int64_t from_t, int64_t to_t);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr2_queue_t *` | The queue. |
| `from_t` | `int64_t` | First time, `0` from the beginning. |
| `to_t` | `int64_t` | Last time, `0` to the end. |

**Returns** — `0`, always.

**Example** — the messages of the last hour:

```C
time_t now = time(NULL);
tr2q_load_all_by_time(trq, now - 3600, now);
```

---

(tr2q_append)=
## `tr2q_append()`

`tr2q_append()` adds a message to the queue: it is written to the topic with
`TR2Q_MSG_PENDING` and the given flags, and put inflight (with its content)
or queued (metadata only) by the same rule as a load. The payload gbuffer of
the record (`"gbuffer"`) is serialized as `"payload"` for the disk, and given
back to the in-memory record as a gbuffer.

```C
q2_msg_t *tr2q_append(
    tr2_queue_t *trq,
    json_int_t t,
    json_t *kw,         // owned
    uint16_t user_flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr2_queue_t *` | The queue. |
| `t` | `json_int_t` | The `__t__` of the record; `0` lets timeranger2 put the time of now. |
| `kw` | `json_t *` | The message (owned), as built by [`new_mqtt_message()`](#new_mqtt_message); its `mid` is required. |
| `user_flag` | `uint16_t` | QoS, retain, direction, origin and state bits, added to `TR2Q_MSG_PENDING`. |

**Returns**

The message, owned by the queue (released by
[`tr2q_unload_msg()`](#tr2q_unload_msg) or [`tr2q_close()`](#tr2q_close)).
`NULL` (logged) when `kw` is `NULL`, when the record cannot be written -- `kw`
and its gbuffer are released then -- or when the message cannot be allocated.

**Example** — a QoS 1 message for a subscriber, as `C_MQTT_BROKER` queues it:

```C
uint16_t user_flag = mosq_mo_client | mosq_md_out;
if(qos == 1) {
    user_flag |= mosq_m_qos1;
} else if(qos == 2) {
    user_flag |= mosq_m_qos2;
}
if(retain) {
    user_flag |= mosq_m_retain;
}
q2_msg_t *qmsg = tr2q_append(trq_out_msgs, tm, kw_mqtt_msg, user_flag);
if(!qmsg) {
    return -1;  // Error already logged
}
```

---

(tr2q_move_from_queued_to_inflight)=
## `tr2q_move_from_queued_to_inflight()`

`tr2q_move_from_queued_to_inflight()` promotes a queued message to the
inflight list, reading its content from disk FIRST: a message in flight must
have it. Call it when an inflight slot frees up.

```C
int tr2q_move_from_queued_to_inflight(q2_msg_t *msg);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q2_msg_t *` | A message of the queued list. |

**Returns**

`0`. `-1` when its content cannot be read -- *"Cannot load the content of a
queued message: it stays queued"* -- or it is not in the queued list (logged
by the list). Up to 7.25.4 the message moved first and a failed read left it
in flight with no content.

**Example** — fill the free slots after an acknowledgement:

```C
q2_msg_t *msg, *next;
Q2MSG_FOREACH_FORWARD_QUEUED_SAFE(trq, msg, next) {
    if(trq->max_inflight_messages &&
            tr2q_inflight_size(trq) >= trq->max_inflight_messages) {
        break;
    }
    if(tr2q_move_from_queued_to_inflight(msg) < 0) {
        break;  // Error already logged, the message stays queued
    }
}
```

---

(tr2q_unload_msg)=
## `tr2q_unload_msg()`

`tr2q_unload_msg()` finishes a message: it clears `TR2Q_MSG_PENDING` on disk,
so it is not loaded again, and takes it out of its list and frees it (its
content too).

```C
int tr2q_unload_msg(q2_msg_t *msg, int32_t result);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q2_msg_t *` | The message. It is freed: do not use it afterwards. |
| `result` | `int32_t` | Not used; callers pass `0`. |

**Returns**

`0`, or `-1` when the message is not in its list (logged by the list). A
failure to clear the bit on disk is logged and NOT reported: the message is
freed anyway, and will be loaded again after a restart.

**Example** — a PUBACK arrives for an outgoing QoS 1 message:

```C
q2_msg_t *qmsg = tr2q_get_by_mid(trq_out_msgs, mid);
if(qmsg) {
    tr2q_unload_msg(qmsg, 0);
}
```

---

(tr2q_get_by_rowid)=
## `tr2q_get_by_rowid()`

`tr2q_get_by_rowid()` finds a message of the queue by its rowid, inflight
first and queued after.

```C
q2_msg_t *tr2q_get_by_rowid(tr2_queue_t *trq, uint64_t rowid);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr2_queue_t *` | The queue. |
| `rowid` | `uint64_t` | The rowid of its record. |

**Returns**

The message (owned by the queue), or `NULL`, with nothing logged.

**Example**

```C
q2_msg_t *qmsg = tr2q_get_by_rowid(trq, rowid);
if(qmsg && !qmsg->inflight) {
    tr2q_move_from_queued_to_inflight(qmsg);
}
```

---

(tr2q_get_by_mid)=
## `tr2q_get_by_mid()`

`tr2q_get_by_mid()` finds a message of the queue by its MQTT packet
identifier, inflight first and queued after: this is how an acknowledgement
(PUBACK, PUBREC, PUBREL, PUBCOMP) finds the message it answers.

```C
q2_msg_t *tr2q_get_by_mid(tr2_queue_t *trq, json_int_t mid);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr2_queue_t *` | The queue. |
| `mid` | `json_int_t` | The packet identifier. |

**Returns**

The first message with that `mid` (owned by the queue), or `NULL`, with
nothing logged.

**Example** — a PUBREC arrives: the QoS 2 handshake goes on to PUBREL:

```C
q2_msg_t *qmsg = tr2q_get_by_mid(trq_out_msgs, mid);
if(!qmsg) {
    return -1;  // not a message of this queue: C_PROT_MQTT2 ignores it quietly
}
msg_flag_set_state(qmsg, mosq_ms_wait_for_pubcomp);
tr2q_save_hard_mark(qmsg, qmsg->md_record.user_flag);
```

---

(tr2q_msg_json)=
## `tr2q_msg_json()`

`tr2q_msg_json()` returns the content of a message, reading it from disk when
it is not in memory (a queued message, or one loaded at startup) and turning
its serialized `"payload"` back into a `"gbuffer"`. The content is then kept
in the message.

```C
json_t *tr2q_msg_json(q2_msg_t *msg);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q2_msg_t *` | The message. |

**Returns**

The content, NOT yours: it belongs to the message and is freed with it by
[`tr2q_unload_msg()`](#tr2q_unload_msg). Incref it to keep it, or to hand it
to a function that takes ownership. `NULL` when it cannot be read (logged),
for instance when the queue has lost its topic after a failed backup (see
[`tr2q_check_backup()`](#tr2q_check_backup)).

**Example**

```C
json_t *kw_msg = tr2q_msg_json(qmsg);
if(!kw_msg) {
    return -1;  // Error already logged
}
const char *topic = kw_get_str(gobj, kw_msg, "topic", "", 0);
gbuffer_t *gbuf = (gbuffer_t *)(uintptr_t)kw_get_int(gobj, kw_msg, "gbuffer", 0, 0);
```

---

(tr2q_save_hard_mark)=
## `tr2q_save_hard_mark()`

`tr2q_save_hard_mark()` writes the `user_flag` of a message to disk, in its
record's metadata, with `TR2Q_MSG_PENDING` always set. The in-memory flag is
set to the same value. This is how the step of a QoS handshake survives a
restart.

```C
int tr2q_save_hard_mark(q2_msg_t *msg, uint16_t value);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `msg` | `q2_msg_t *` | The message. |
| `value` | `uint16_t` | The new flags; `TR2Q_MSG_PENDING` is added. |

**Returns**

What `tranger2_write_user_flag()` answers: `0`, or `-1` when it cannot be
written (logged). `-1` (logged) also when the queue has no topic.

**Example** — change the bits in memory with the helpers, then save them:

```C
msg_flag_set_dup(qmsg, 1);
msg_flag_set_state(qmsg, mosq_ms_wait_for_puback);
tr2q_save_hard_mark(qmsg, qmsg->md_record.user_flag);
```

---

(tr2q_check_backup)=
## `tr2q_check_backup()`

`tr2q_check_backup()` backs up the queue's topic when it has grown to the
`backup_queue_size` given to [`tr2q_open()`](#tr2q_open): the topic is moved
to a backup and re-created empty, and `first_rowid` is reset. Call it only with
nothing in flight; it does nothing while messages are QUEUED either, since they
keep only the rowid of a record the backup would take away.

It behaves exactly as [`trq_check_backup()`](#trq_check_backup) of
`tr_queue`, whose page tells each case with its log lines: refused after a
failed load, a failed backup that leaves the queue in its topic, and a topic
that cannot be opened again and is taken back by name later.

```C
int tr2q_check_backup(tr2_queue_t *trq);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr2_queue_t *` | The queue. |

**Returns**

`0` when there was nothing to do (no `backup_queue_size`, a topic still
small, messages queued) or the backup was done. `-1` when the backup is
REFUSED, said once: *"Queue backup refused: its last load did not read every
pending message"*; when it FAILS: *"Queue backup failed: the queue goes on in
its topic, not backed up"* (or *"..., and the queue has no topic"*); and
when the queue has no topic and it cannot be opened yet.

**Example** — `C_PROT_MQTT2` calls it every `timeout_backup` seconds, for each
queue with nothing in flight:

```C
if(priv->timeout_backup > 0 && test_sectimer(priv->t_backup)) {
    if(priv->trq_in_msgs && tr2q_inflight_size(priv->trq_in_msgs) == 0) {
        tr2q_check_backup(priv->trq_in_msgs);
    }
    if(priv->trq_out_msgs && tr2q_inflight_size(priv->trq_out_msgs) == 0) {
        tr2q_check_backup(priv->trq_out_msgs);
    }
    priv->t_backup = start_sectimer(priv->timeout_backup);
}
```

---

(tr2q_list_msgs)=
## `tr2q_list_msgs()`

`tr2q_list_msgs()` traces the messages of the queue, one line each, inflight
first (with session, topic, QoS, retain and payload length) and queued after
(only that it is there: its content is not in memory).

```C
int tr2q_list_msgs(tr2_queue_t *trq);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `trq` | `tr2_queue_t *` | The queue. |

**Returns** — `0`.

**Example** — `C_MQTT_BROKER` lists a queue after appending to it, under its
`messages2` trace level:

```C
if(gobj_trace_level(gobj) & TRACE_MESSAGES2) {
    tr2q_list_msgs(trq_out_msgs);
}
// 💾 TR2Q INFLIGHT, session 'sensor-17', topic 'sensors/room1/temp', qos 1, retain 0 , msg_len 14
```

---

(build_queue_name)=
## `build_queue_name()`

`build_queue_name()` writes the name of a session's queue:
`<client_id>-IN` or `<client_id>-OUT`.

```C
int build_queue_name(
    char *bf,
    size_t bfsize,
    const char *client_id,
    mqtt_msg_direction_t mqtt_msg_direction
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | The buffer to write in. |
| `bfsize` | `size_t` | Its size; `NAME_MAX` is what the module uses. |
| `client_id` | `const char *` | The client id of the session. |
| `mqtt_msg_direction` | `mqtt_msg_direction_t` | `mosq_md_in` or `mosq_md_out`; `0` writes the bare client id. |

**Returns**

What `snprintf()` answers: the length of the whole name, which is `bfsize` or
more when it did not fit (truncated, and not logged).

**Example**

```C
char queue_name[NAME_MAX];
build_queue_name(queue_name, sizeof(queue_name), "sensor-17", mosq_md_out);
// "sensor-17-OUT"
```

---

(msg_flag_state_to_str)=
## `msg_flag_state_to_str()`

`msg_flag_state_to_str()` returns the name of the QoS-handshake state of a
message, for traces.

```C
const char *msg_flag_state_to_str(mqtt_msg_state_t state);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `state` | `mqtt_msg_state_t` | A `mosq_ms_*` value. |

**Returns**

A static string: `"invalid"`, `"publish_qos0"`, `"publish_qos1"`,
`"wait_for_puback"`, `"publish_qos2"`, `"wait_for_pubrec"`,
`"resend_pubrel"`, `"wait_for_pubrel"`, `"resend_pubcomp"`,
`"wait_for_pubcomp"`, `"send_pubrec"`, `"queued"`, or `"unknown"`.

**Example**

```C
trace_machine2("msg mid %d: %s",
    (int)qmsg->mid,
    msg_flag_state_to_str(msg_flag_get_state(qmsg))
);
// msg mid 42: wait_for_puback
```

---

(msg_flag_direction_to_str)=
## `msg_flag_direction_to_str()`

`msg_flag_direction_to_str()` returns the name of a direction, the suffix
[`build_queue_name()`](#build_queue_name) puts on a queue.

```C
const char *msg_flag_direction_to_str(mqtt_msg_direction_t dir);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `dir` | `mqtt_msg_direction_t` | `mosq_md_in` or `mosq_md_out`. |

**Returns** — `"IN"`, `"OUT"`, or `""` for anything else.

**Example**

```C
msg_flag_direction_to_str(msg_flag_get_direction(qmsg));    // "OUT"
```

---

(msg_flag_origin_to_str)=
## `msg_flag_origin_to_str()`

`msg_flag_origin_to_str()` returns the name of the origin of a message.

```C
const char *msg_flag_origin_to_str(mqtt_msg_origin_t orig);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `orig` | `mqtt_msg_origin_t` | `mosq_mo_client` or `mosq_mo_broker`. |

**Returns** — `"Client"`, `"Broker"`, or `"None"` for anything else.

**Example**

```C
msg_flag_origin_to_str(msg_flag_get_origin(qmsg));      // "Client"
```
