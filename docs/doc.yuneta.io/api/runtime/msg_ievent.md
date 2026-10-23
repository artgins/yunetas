# Inter-Event Messaging

Functions for creating, serializing, and managing inter-event messages
exchanged between yunos.

**Source:** `kernel/c/root-linux/src/msg_ievent.h`

---

(iev_create)=
## `iev_create()`

Creates an inter-event object for use within a yuno. Adds the
`__iev_event__` field to `kw`.

```C
json_t *iev_create(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw          // like owned, same kw returned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `event` | `gobj_event_t` | Event to encapsulate. |
| `kw` | `json_t *` | Keyword arguments (like owned — same pointer is returned). |

**Returns**

The modified `kw` with `__iev_event__` added, or `NULL` on error.

---

(iev_create2)=
## [`iev_create2()`](https://github.com/artgins/yunetas/blob/7.27.9/kernel/c/root-linux/src/msg_ievent.c#L89)

Creates an inter-event object and copies the `__temp__` metadata from a
request `kw`.

```C
json_t *iev_create2(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw,           // like owned, same kw returned
    json_t *kw_request    // owned, only __temp__ is extracted
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `event` | `gobj_event_t` | Event to encapsulate. |
| `kw` | `json_t *` | Keyword arguments (like owned). |
| `kw_request` | `json_t *` | Request kw from which `__temp__` is extracted (owned). |

**Returns**

The modified `kw`, or `NULL` on error.

---

(iev_create_to_gbuffer)=
## [`iev_create_to_gbuffer()`](https://github.com/artgins/yunetas/blob/7.27.9/kernel/c/root-linux/src/msg_ievent.c#L115)

Serializes an inter-event message into a `gbuffer_t` for sending to the
outside world.

```C
gbuffer_t *iev_create_to_gbuffer(
    hgobj gobj,
    gobj_event_t event,
    json_t *kw   // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `event` | `gobj_event_t` | Event name. |
| `kw` | `json_t *` | Keyword arguments (owned). |

**Returns**

`gbuffer_t` pointer with the serialized JSON, or `NULL` on error.

---

(iev_create_from_gbuffer)=
## [`iev_create_from_gbuffer()`](https://github.com/artgins/yunetas/blob/7.27.9/kernel/c/root-linux/src/msg_ievent.c#L232)

Deserializes an inter-event message received from the outside world.

```C
json_t *iev_create_from_gbuffer(
    hgobj gobj,
    gobj_event_t *event,   // output: the public event the frame names, NULL if none
    gbuffer_t *gbuf,       // owned, data consumed
    int verbose             // 1 = log, 2 = log + dump
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `event` | `gobj_event_t *` | Output — the public event the frame names (the registered name, compared by pointer), `NULL` when no gclass has it as public. |
| `gbuf` | `gbuffer_t *` | Buffer with the serialized message (owned and consumed). |
| `verbose` | `int` | `0`: a frame that is not json or not an inter-event is the caller's to say; `1` or `2`: said here, a WARNING (a frame that is not json with a masked DEBUG dump of its first bytes). An event that is not public is said whatever `verbose` is. |

**Returns**

Deserialized `kw` JSON object (yours), or `NULL` for a frame that is not json
or not an inter-event (no `"event"` string, no `"kw"` dict). `*event` is the
public event the frame names, `NULL` when no gclass has it as public.

What a frame gets wrong is the peer's, said ONCE, a WARNING (`MSGSET_PROTOCOL`,
`peername`, no stack), saying what it got wrong:

- with `verbose`, a frame that is not json is *"frame is not json"*
  ([`gbuf2json_from_peer()`](#gbuf2json_from_peer): the parser's error and its
  position -- not the bytes jansson quotes near it, which showed the value of
  a secret (7.27.0+) --, the bytes, and a DEBUG dump of the first 256 masked
  as [`mask_secrets_in_text()`](#mask_secrets_in_text) masks them);
- with `verbose`, a frame of json that is not an inter-event (no `"event"`
  string, no `"kw"` dict) is *"Frame is not an inter-event (an "event" string
  and a "kw" dict), refused"*, with the frame in `frame`, masked
  ([`json_mask_secrets()`](#json_mask_secrets)) and capped to 256 bytes, and
  `frame_size`;
- with `0` the caller says both;
- an event that is not public is said HERE whatever `verbose` is, since only
  here is the name the frame gave known: *"Frame of an event that is not
  public, refused"*, with the name in `event` masked and THEN capped to 64
  bytes ([`mask_secrets_inline_capped()`](#mask_secrets_inline_capped): a
  password or a JWT sent as the name of the event was written in clear up to
  7.26.9; capped first, a token the cap cut showed its first bytes), the kw
  in `kw` masked and capped to 256 bytes, and `frame_size`.

`frame_size` is the bytes of the frame RECEIVED. Up to 7.26.9 the WARNING of an
event that is not public said a `kw_size` that was the size of the masked,
capped copy (a kw of 900 KB said 101). `C_IEVENT_SRV` and `C_IEVENT_CLI` pass
`1` (7.27.0+) and close the channel: they passed `0` and said *"Bad json"* for
both refusals, a frame of json among them, with a dump of the frame's bytes
raw, where a JWT under a plain key, cut by the window, showed its header. Up to
7.26.9 the lookup of the event was silent and both callers traced the whole kw
raw (*"iev_event NULL"*: a jwt and a password before the session among it),
and a frame with no `"event"` logged two ERRORs here and the caller's WARNING.

**Example**

```C
gobj_event_t iev_event;
json_t *iev_kw = iev_create_from_gbuffer(gobj, &iev_event, gbuf, 1);
if(!iev_kw) {
    // not json, or not an inter-event: said by iev_create_from_gbuffer()
} else if(!iev_event) {
    // an event that is not public: logged by iev_create_from_gbuffer()
    KW_DECREF(iev_kw)
}
```

```text
WARNING {"msgset": "Protocol", "msg": "Frame of an event that is not public, refused",
         "event": "EV_NO_SUCH_EVENT password=********", "peername": "127.0.0.1:45216",
         "kw": "{\"jwt\":\"********\",\"password\":\"********\",...", "frame_size": 66034}
WARNING {"msgset": "Protocol",
         "msg": "Frame is not an inter-event (an \"event\" string and a \"kw\" dict), refused",
         "peername": "127.0.0.1:45218", "frame": "{\"note\":\"xxxx ********\"}", "frame_size": 321}
```

---

(msg_iev_push_stack)=
## `msg_iev_push_stack()`

Pushes a record onto an inter-event metadata stack (LIFO).
The stack is stored in `__md_iev__[stack]`.

```C
int msg_iev_push_stack(
    hgobj gobj,
    json_t *kw,          // not owned
    const char *stack,
    json_t *jn_data      // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `kw` | `json_t *` | Message kw (not owned). |
| `stack` | `const char *` | Stack name (for example `"ievent_gate_stack"`). |
| `jn_data` | `json_t *` | Data to push (owned). |

**Returns**

`0` on success, `-1` on failure.

---

(msg_iev_get_stack)=
## `msg_iev_get_stack()`

Returns the top record of an inter-event stack **without** popping it.

```C
json_t *msg_iev_get_stack(
    hgobj gobj,
    json_t *kw,          // not owned
    const char *stack,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `kw` | `json_t *` | Message kw (not owned). |
| `stack` | `const char *` | Stack name. |
| `verbose` | `BOOL` | `TRUE` to log when the kw has no `__md_iev__` or no such stack. |

**Returns**

Top record (not owned — do not free), or `NULL` if not found.

**Notes**

The routing travels in the frame, so a kw without it is what the peer sent: with `verbose`, one WARNING (`MSGSET_PROTOCOL`, *"__md_iev__ NOT FOUND"* or *"__md_iev__ stack NOT FOUND"*) naming the stack and the peer, with the kw masked and capped to 256 bytes (7.27.0+). Up to 7.26.9 it was an ERROR with a stack trace and the whole kw traced in clear -- the frame of a server, a command's password among it, reached it in `C_IEVENT_CLI`. Without `verbose` nothing is logged: the caller decides.

**Example**

```C
json_t *jn_ievent_id = msg_iev_get_stack(gobj, kw, IEVENT_STACK_ID, FALSE);
if(jn_ievent_id) {
    // __RESPONSE__: the request this kw answers
} else {
    // __REQUEST__: the routing is pushed here
}
```

---

(msg_iev_pop_stack)=
## [`msg_iev_pop_stack()`](https://github.com/artgins/yunetas/blob/7.27.9/kernel/c/root-linux/src/msg_ievent.c#L554)

Pops and returns the top record from an inter-event stack.

```C
json_t *msg_iev_pop_stack(
    hgobj gobj,
    json_t *kw,
    const char *stack
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `kw` | `json_t *` | Message kw. |
| `stack` | `const char *` | Stack name. |

**Returns**

Popped record (owned — caller must free), or `NULL` if not found. A kw without `__md_iev__` or without the stack is logged as [`msg_iev_get_stack()`](#msg_iev_get_stack) with `verbose` logs it: a WARNING, the kw masked and capped (7.27.0+; it was an ERROR with the kw traced in clear).

**Example**

The agent forwarding the answer of a yuno to whoever asked:

```C
json_t *jn_ievent_id = msg_iev_pop_stack(gobj, kw, IEVENT_STACK_ID);
const char *dst_service = kw_get_str(gobj, jn_ievent_id, "dst_service", "", 0);
// ... route kw to dst_service ...
JSON_DECREF(jn_ievent_id)
```

---

(msg_iev_set_back_metadata)=
## [`msg_iev_set_back_metadata()`](https://github.com/artgins/yunetas/blob/7.27.9/kernel/c/root-linux/src/msg_ievent.c#L622)

Copies inter-event metadata from a request into a response.
Optionally reverses `src`/`dst` in the `ievent_gate_stack`.

```C
json_t *msg_iev_set_back_metadata(
    hgobj gobj,
    json_t *kw_request,    // owned (only __md_iev__ extracted)
    json_t *kw_response,   // like owned, returned (created if NULL)
    BOOL reverse_dst
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `kw_request` | `json_t *` | Request kw (owned). |
| `kw_response` | `json_t *` | Response kw (like owned, returned, and created if `NULL`). |
| `reverse_dst` | `BOOL` | `TRUE` to swap `src`/`dst` in the ievent gate stack. |

**Returns**

The `kw_response` with metadata set.

---

(msg_iev_build_response)=
## [`msg_iev_build_response()`](https://github.com/artgins/yunetas/blob/7.27.9/kernel/c/root-linux/src/msg_ievent.c#L663)

Builds a complete command/stats response with result code, comment,
schema, and data fields. Sets metadata with reversed `src`/`dst`.

```C
json_t *msg_iev_build_response(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment,    // owned
    json_t *jn_schema,     // owned
    json_t *jn_data,       // owned
    json_t *kw_request     // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `result` | `json_int_t` | Result code (`0` = success, negative = error). |
| `jn_comment` | `json_t *` | Comment string (owned). |
| `jn_schema` | `json_t *` | Response schema (owned). |
| `jn_data` | `json_t *` | Response data (owned). |
| `kw_request` | `json_t *` | Original request kw (owned). |

**Returns**

Complete response kw ready to send.

---

(msg_iev_build_response_without_reverse_dst)=
## [`msg_iev_build_response_without_reverse_dst()`](https://github.com/artgins/yunetas/blob/7.27.9/kernel/c/root-linux/src/msg_ievent.c#L679)

Same as [`msg_iev_build_response()`](#msg_iev_build_response) but does
**not** reverse `src`/`dst` in the metadata.

```C
json_t *msg_iev_build_response_without_reverse_dst(
    hgobj gobj,
    json_int_t result,
    json_t *jn_comment,    // owned
    json_t *jn_schema,     // owned
    json_t *jn_data,       // owned
    json_t *kw_request     // owned
);
```

**Parameters**

Same as [`msg_iev_build_response()`](#msg_iev_build_response).

**Returns**

Complete response kw with metadata set but `src`/`dst` not reversed.

---

(msg_iev_clean_metadata)=
## [`msg_iev_clean_metadata()`](https://github.com/artgins/yunetas/blob/7.27.9/kernel/c/root-linux/src/msg_ievent.c#L700)

Removes all inter-event metadata from `kw`: `__md_iev__`, `__temp__`,
`__md_tranger__`, and `__md_yuno__`.

```C
json_t *msg_iev_clean_metadata(
    json_t *kw   // not owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `kw` | `json_t *` | Message kw (not owned). |

**Returns**

Same `kw` with metadata fields removed.

---

(msg_iev_set_msg_type)=
## `msg_iev_set_msg_type()`

Sets the message type in `__md_iev__.__msg_type__`.

```C
int msg_iev_set_msg_type(
    hgobj gobj,
    json_t *kw,
    const char *msg_type
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `kw` | `json_t *` | Message kw. |
| `msg_type` | `const char *` | Message type string (for example `"__command__"`, `"__stats__"`, `"__message__"`). Pass an empty string to delete. |

**Returns**

`0` on success.

---

(msg_iev_get_msg_type)=
## `msg_iev_get_msg_type()`

Reads the message type from `__md_iev__.__msg_type__`.

```C
const char *msg_iev_get_msg_type(
    hgobj gobj,
    json_t *kw
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `kw` | `json_t *` | Message kw. |

**Returns**

Message type string, or an empty string if not set.

---

(trace_inter_event)=
## `trace_inter_event()`

Traces an inter-event with compact metadata (only `result` and
`__md_iev__`). Used by the `ievents` trace level. The secrets are masked as in
[`trace_inter_event2()`](#trace_inter_event2) — the command of a v7
inter-event lives in `__md_iev__`, and a positional `SDF_SECRET` parameter in
it is masked by the command table of its destination service.

```C
void trace_inter_event(
    hgobj gobj,
    const char *prefix,
    const char *event,
    json_t *kw
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `prefix` | `const char *` | Trace message prefix. |
| `event` | `const char *` | Event name. |
| `kw` | `json_t *` | Message kw. |

**Returns**

This function does not return a value.

**Example**

```C
/*
 *  A service "vault" of this yuno, whose command takes its secret
 *  positionally (a leading SDF_REQUIRED parameter):
 *
 *      PRIVATE sdata_desc_t pm_set_password[] = {
 *      SDATAPM (DTP_STRING, "password", SDF_REQUIRED|SDF_SECRET, 0, "The new password"),
 *      SDATA_END()
 *      };
 */
json_t *kw = json_pack("{s:{s:[{s:s}], s:[{s:s}]}}",
    "__md_iev__",
        "ievent_gate_stack", "dst_service", "vault",
        "__command__", "command", "set-password hunter2"
);
trace_inter_event(gobj, "ievents", "EV_MT_COMMAND", kw);
// the trace shows "command": "set-password ********"
json_decref(kw);
```

Up to 7.25.21 this trace masked by key names only, and such a positional
secret went in clear.

---

(trace_inter_event2)=
## [`trace_inter_event2()`](https://github.com/artgins/yunetas/blob/7.27.9/kernel/c/root-linux/src/msg_ievent.c#L834)

Traces an inter-event with the **full** kw content.

```C
void trace_inter_event2(
    hgobj gobj,
    const char *prefix,
    const char *event,
    json_t *kw
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `prefix` | `const char *` | Trace message prefix. |
| `event` | `const char *` | Event name. |
| `kw` | `json_t *` | Full message kw. |

**Returns**

This function does not return a value.
