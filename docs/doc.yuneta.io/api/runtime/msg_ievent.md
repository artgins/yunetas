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
## `iev_create2()`

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
## `iev_create_to_gbuffer()`

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
## `iev_create_from_gbuffer()`

Deserializes an inter-event message received from the outside world.

```C
json_t *iev_create_from_gbuffer(
    hgobj gobj,
    gobj_event_t *event,   // output: extracted event name
    gbuffer_t *gbuf,       // owned, data consumed
    int verbose             // 1 = log, 2 = log + dump
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject instance. |
| `event` | `gobj_event_t *` | Output — receives the extracted event name. |
| `gbuf` | `gbuffer_t *` | Buffer with the serialized message (owned and consumed). |
| `verbose` | `int` | Verbosity level: `1` = log, `2` = log + dump. |

**Returns**

Deserialized `kw` JSON object, or `NULL` on error.

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
| `stack` | `const char *` | Stack name (e.g. `"ievent_gate_stack"`). |
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
| `verbose` | `BOOL` | `TRUE` to log errors when the stack is empty. |

**Returns**

Top record (not owned — do not free), or `NULL` if not found.

---

(msg_iev_pop_stack)=
## `msg_iev_pop_stack()`

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

Popped record (owned — caller must free), or `NULL` if not found.

---

(msg_iev_set_back_metadata)=
## `msg_iev_set_back_metadata()`

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
| `kw_response` | `json_t *` | Response kw (like owned, returned; created if `NULL`). |
| `reverse_dst` | `BOOL` | `TRUE` to swap `src`/`dst` in the ievent gate stack. |

**Returns**

The `kw_response` with metadata set.

---

(msg_iev_build_response)=
## `msg_iev_build_response()`

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
## `msg_iev_build_response_without_reverse_dst()`

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
## `msg_iev_clean_metadata()`

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
| `msg_type` | `const char *` | Message type string (e.g. `"__command__"`, `"__stats__"`, `"__message__"`). Pass an empty string to delete. |

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
`__md_iev__`).

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

---

(trace_inter_event2)=
## `trace_inter_event2()`

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
