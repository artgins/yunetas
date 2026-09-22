(kwid)=

# **Kwid**

## Overview of `kwid`

`kwid` is a library designed to provide a higher-level abstraction for managing JSON data across multiple programming languages. It extends basic JSON handling capabilities by introducing advanced features like path-based manipulation, cloning, filtering, serialization, and database-like utilities.

In **C**, the library is built on top of the [Jansson library](https://jansson.readthedocs.io/), while in other languages like **JavaScript** and **Python**, it uses native types (`bool`, `array`, `object` in JS and `list`, `dict` in Python). This design makes sure of seamless integration with the native JSON structures of each language. This enables consistent behavior and cross-platform portability.

Source code in:
- [kwid.c](https://github.com/artgins/yunetas/blob/7.25.2/kernel/c/gobj-c/src/kwid.c)
- [kwid.h](https://github.com/artgins/yunetas/blob/7.25.2/kernel/c/gobj-c/src/kwid.h)

---

## Key Features and Goals

1. **Enhanced JSON Management**:
    - Provides functions for advanced JSON manipulations such as cloning, filtering, and path-based access.
    - Supports structured operations with JSON objects, arrays, and dictionaries.

2. **Cross-Language Implementation**:
    - Functions are implemented in **C** using the Jansson library.
    - In **JavaScript**, native types like `object`, `array`, and `bool` replace the need for external libraries.
    - In **Python**, the library will use native types like `list` and `dict`.
    - This cross-language compatibility makes sure of consistent function across environments.

3. **Path-Based Access and Manipulation**:
    - Functions like `kw_find_path`, `kw_set_dict_value`, and `kw_delete` allow for fine-grained control over nested JSON structures using path-based syntax.

4. **Database-Like Utilities**:
    - Provides record-based operations such as [`kwid_find_record_in_list`](https://github.com/artgins/yunetas/blob/7.25.2/kernel/c/gobj-c/src/kwid.c#L937), [`kwid_compare_records`](#kwid_compare_records), and [`kwjr_get`](#kwjr_get).
    - Enables filtering and matching of JSON data with `kw_clone_by_keys` and `kw_match_simple`.

5. **Customizability**:
    - Supports user-defined behavior through function pointers like `serialize_fn_t`, `deserialize_fn_t`, `incref_fn_t`, and `decref_fn_t`.

6. **Integration with Yuneta**:
    - Designed for seamless integration with the GObj framework, leveraging its logging, memory management, and contextual handling.

---

## Multi-Language Behavior

- **C**: Utilizes the Jansson library for reliable JSON parsing, manipulation, and serialization.
- **JavaScript**: Leverages native JSON-like types (`object`, `array`, `bool`) for lightweight and efficient operations.
- **Python**: Planned implementation will use native types (`dict`, `list`) to align with Python's dynamic JSON-like data structures.

This multi-language approach makes sure of the library remains idiomatic in each environment while preserving a consistent API.

---

## Primary Use Cases

1. **JSON Manipulation**:
    - Simplify complex JSON operations like cloning, filtering, and updating.
    - Manage deeply nested JSON structures using path-based access.

2. **Data Storage and Persistence**:
    - Serialize and deserialize JSON data for integration with persistent storage systems.

3. **Application Configuration**:
    - Manage hierarchical application settings using JSON structures across platforms.

4. **Cross-Platform Portability**:
    - Provide consistent JSON manipulation utilities across C, JavaScript, Python, and other languages.

---

## The part that `kwid` plays in Yuneta

- Acts as an intermediary between low-level JSON handling (via Jansson in C) and high-level application logic (for example in GObjs).
- Ensures compatibility across Yuneta's multi-language ecosystem by abstracting JSON operations into reusable, extensible utilities.
- Provides a standardized API that abstracts the complexities of JSON manipulation while remaining native to each language environment.












## A JSON as a table: the flat form

Sometimes the useful way to look at a JSON is not its own shape but a **table**:
one row per **leaf**, where the id is the **path** of the item and the value is
its value. It is a better form to store, to compare and to diff — and it is the
only one a person can read when two configurations disagree.

```
{"a": {"b": 1}, "c": [10, 20]}
    ->  {"a`b": 1, "c`[0]": 10, "c`[1]": 20}
```

[`json2flat()`](#json2flat) writes it and [`flat2json()`](#flat2json) reads it
back. The same grammar is implemented in `gobj-js`, and the two must stay
identical: a flat JSON is written by one side and read by the other.

### The grammar, and the reason for each rule

- **Segments are joined by a backtick**, which is already the path delimiter of
  this library ([`kw_get_dict()`](#kw_get_dict) and friends). It is rare in real
  keys and it reads as a joint rather than as part of a name.
- **A literal backtick inside a key is doubled.** With that, every key is
  representable and **the form forbids nothing** — which matters more than it
  sounds: the first implementation had to reserve all-digit keys for array
  indices, and a dictionary keyed by a yuno id (`"1630"`) came back as an array
  of 1631 elements.
- **An array index is `[N]`**, canonical, no leading zeros. It costs one byte
  over a bare number and it buys the forbidden-key rule back.
- **A dict key that begins with `[` doubles it** (`[[0]`), so it can never be
  read as an index.
- **An empty container is a leaf.** `{}` and `[]` hold no leaves of their own,
  so a strict leaves-only form loses them — and an empty `properties` object is
  ordinary in a configuration. Stored as themselves, the round trip holds and a
  diff can say *this became empty* instead of saying nothing.

### It refuses instead of guessing

`flat2json()` fails, and says which id and why, when the flat dictionary cannot
be rebuilt **exactly**: an id used as a leaf and as a container (the answer
would depend on the order the ids are read in), an index over the limit (one id
would otherwise materialise a million nulls), or a path deeper than the limit.
Rebuilding *most of it* is how a configuration comes back subtly different from
the one that was saved.

### Comparing

[`flat_diff()`](#flat_diff) answers `{added, removed, changed}` over two flat
dictionaries, and [`flat_apply()`](#flat_apply) applies that to a flat
dictionary — the flat form on purpose, because there an id addresses one value,
so applying is setting and deleting with nothing to walk.

For the other question — *are these two records equal?* —
[`kwid_compare_records()`](#kwid_compare_records) works on the nested form and
tolerates disorder. They do not compete: the flat form is for **seeing** a
difference and carrying it; the nested one for **answering** whether there is
any.

---

(json-ownership)=

## Ownership: who frees this json?

Jansson counts references; it does not say who holds them. Every json bug of
this family comes from one unanswered question:

> **After this line, how many owners does this json have?**

An owner is somebody who must call `decref` once. Too many owners is a leak.
Too few is a double free — the framework prints *"BAD json_decref()"* or dies
later in an unrelated place.

The question is answered in **four** places, not one. Learn the four and the
rest is arithmetic.

---

### 1. What you RECEIVE (a parameter)

The signature says it, and nothing else does. There is no type for ownership,
so a signature without the comment is an incomplete signature.

| Annotation | What it means for you |
|---|---|
| `json_t *kw // owned` | It is yours. `decref` it on **every** exit, including the early `return -1`. |
| `json_t *kw // NOT owned` | You read it. You never free it. |

`owned` is the default of the public API: the `kw` of
[`gobj_send_event()`](https://github.com/artgins/yunetas/blob/7.25.2/kernel/c/gobj-c/src/gobj.h),
`gobj_publish_event()`, `gobj_post_event()`, `gobj_command()`, `gobj_create()`,
`gobj_write_json_attr()`, `build_command_response()`,
`msg_iev_build_response()` and `json2gbuf()`.

`NOT owned` is the default of a callback the framework calls with something it
still owns: `mt_publish_event()` and the `mt_publication_*_filter()` methods
(there you may even **modify** the kw), `gobj_trace_json()`, and the `%j` field
of any `gobj_log_*()` line.

**The early exit is where ownership is lost.** Free it on the failure path too:

```c
PRIVATE int ac_on_message(hgobj gobj, const char *event, json_t *kw, hgobj src)
{
    gbuffer_t *gbuf = (gbuffer_t *)(size_t)kw_get_int(gobj, kw, "gbuffer", 0, 0);
    if(!gbuf) {
        gobj_log_error(gobj, 0,
            "function",     "%s", __FUNCTION__,
            "msgset",       "%s", MSGSET_PROTOCOL,
            "msg",          "%s", "Message without gbuffer",
            NULL
        );
        KW_DECREF(kw)       // <-- the failure path owns it too
        return -1;
    }
    ...
}
```

---

### 2. What you RETURN (and what is returned to you)

| Annotation | Examples |
|---|---|
| `// Return is YOURS` / `MUST be decref` | `gobj_list_nodes()`, `gobj_get_node()`, `gobj_create_node()`, `gobj_update_node()`, `kwid_new_list()`, `kwid_new_dict()`, `kwid_get_ids()`, `gbuf2json()`, `gobj_read_attrs()`, the response of `gobj_command()`, and every `json_pack()` or `json_deep_copy()` |
| `// Return is NOT YOURS` | `gobj_read_json_attr()`, `gobj_jn_stats()`, `treedb_get_node()`, `treedb_open_db()`, `treedb_create_topic()`, `tranger2_open_topic()`, `json_object_get()`, `json_array_get()`, [`kwid_get()`](#kwid_get), and every `kw_get_*()` without flags |

A borrowed string is borrowed too: **`kw_get_str()` returns a `const char *`
that points INSIDE the json**. It lives exactly as long as that json does.
Copy it before you free the owner.

```c
const char *id = kw_get_str(gobj, node, "id", "", 0);
char my_id[NAME_MAX];
snprintf(my_id, sizeof(my_id), "%s", id);   // the node may die after this
```

**The same call returns borrowed or owned, by flag:**

- `KW_EXTRACT` increfs the value and deletes the key: the return becomes
  **yours**.
- `KW_CREATE` stores the default inside the kw and returns it **borrowed** —
  do not free it.

---

### 3. What you PUT INTO a container

This is the one that is easy to get wrong, because the decision is written in
four different notations that all mean the same thing:

| Transfer — the container takes your reference | Share — the container takes ANOTHER reference |
|---|---|
| `json_pack("{s:o}", ...)` | `json_pack("{s:O}", ...)` |
| `json_object_set_new()` | `json_object_set()` |
| `json_array_append_new()` | `json_array_append()` |
| `json_object_update_new()` | `json_object_update()` (shares the **values**) |
| you own nothing afterwards | **you still own yours, and must free it** |

Read the case rule as: *lower case, the container keeps it; UPPER case, the
container keeps a copy of the reference and yours is still alive.*

**Use `o` for what you just built. Use `O` for what belongs to somebody else:**

```c
json_t *naves = json_array();               // built here, one owner
...
json_array_append_new(talleres, json_pack(
    "{s:s, s:O, s:o}",
    "id",            taller_id,
    "observaciones", kw_get_list(gobj, w, "observaciones", json_array(), 0),
    "naves",         naves                  // 'o': handed over, not freed here
));
```

A real bug of this exact shape (2026-09-20, `db_history_ce`): a locally built
array was packed with `s:O` instead of `s:o`, so it had two owners and only one
of them freed it. Freeing the configuration left the whole branch behind — 1207
blocks, 63 KB, on every configuration sent to a box. One character.

---

### 4. What you PASS ON, and still need

If you hand a json to a parameter marked `owned` and you go on using your own
copy, say so with an explicit incref **before** the call:

```c
JSON_INCREF(cols)                   // the topic keeps one, we keep ours
topic = tranger2_create_topic(
    tranger, topic_name, "id", topic_tkey, NULL, sf_string_key,
    cols,                           // owned by the callee
    jn_topic_var                    // owned by the callee
);
...
JSON_DECREF(cols)                   // and we free ours
```

---

## The traps

### A default value that the callee may eat

`kw_get_dict()`, `kw_get_list()` and `kw_get_dict_value()` **`JSON_DECREF()`
the `default_value` on the path where they FIND the key** — the path that
almost always runs. If the key is missing they return the default and it is
still yours. So the same argument is consumed or not, depending on the data.

It reads like the harmless idiom it resembles (`kw_get_str()`'s default is a
plain `const char *`, owned by nobody), and it survives every test that
exercises the missing-key path.

**Ask without a default and choose afterwards:**

```c
/*
 *  Asked without a default: kw_get_dict() decrefs the default on the path
 *  where it finds the key, so a fallback here would be spent every call.
 */
json_t *jn_initial_load = kw_get_dict(gobj, kw, "initial_load", 0, 0);
if(jn_initial_load) {
    json_object_set(kw_resource, "initial_load", jn_initial_load);
}
```

### `KW_REQUIRED` with a default: what it can and cannot mean

`KW_REQUIRED` says *this must be here, say so loudly if it is not*. A default
says *give me this instead*. Written in the same call, the **return can no
longer answer the question**: found or missing, you get a usable value, and
only the log knows which path ran.

```c
jn_triggers = kw_get_list(gobj, alarm, "triggers", json_array(), KW_REQUIRED);
```

That is a legitimate intention — *record the anomaly, but always leave me
something to work with* — and it is written all over this code base, almost
always on a scalar:

```c
const char *id = kw_get_str(gobj, trigger, "id", "", KW_REQUIRED);
```

What it is not, is a way to **learn** anything. A log line is not a control
flow: if the caller must act differently when the key is missing, the flag and
the default cannot both stay.

**With an OWNING default the combination stops working**, and not as a matter
of style. `kw_get_list()` and `kw_get_dict()` decref the default only on the
path where they FIND the key, so on the other one you are the owner — and to
free it correctly you have to know which path ran, which is exactly what the
return no longer tells you. *I do not need to know* is not available here.

Three ways out, in order of preference:

```c
/*  1. Ask without a default, and read the answer.
       KW_REQUIRED still logs the absence.  */
json_t *triggers = kw_get_list(gobj, alarm, "triggers", 0, KW_REQUIRED);
if(!triggers) {
    return -1;      // Error already logged
}

/*  2. Only walking it? Then it needs no fallback: json_array_foreach() and
       json_object_foreach() over NULL iterate nothing, because
       json_array_size(NULL) is 0.  */
json_t *triggers = kw_get_list(gobj, alarm, "triggers", 0, KW_REQUIRED);
int idx; json_t *trigger;
json_array_foreach(triggers, idx, trigger) {
    ...
}

/*  3. Answer the question another way, and keep the fallback.
       The guard is what makes the missing path unreachable.  */
if(kw_has_key(kw, "body")) {
    json_t *jn_body = kw_duplicate(gobj,
        kw_get_dict_value(gobj, kw, "body", json_object(), KW_REQUIRED)
    );
    ...
}
```

On a scalar default, the combination costs information. On a container
default, it costs memory as well.

### A kw is not a json

A `kw` is refcounted with `kw_incref()` / `kw_decref()`, **never** with
`json_incref()` / `json_decref()`. The pair is not a synonym of the json one:
`kw_decref()` also drops the serialized binary fields (the gbuffer) on **every**
call, not only on the last one, and `kw_incref()` is what balances that.

A `json_incref(kw)` therefore raises the json count and leaves the gbuffer's
untouched, and every `KW_DECREF` downstream frees a gbuffer that was never
increfed — a double free. It bites only once the kw actually carries a gbuffer,
so the wrong call sits there looking fine for years. **Write `kw_incref()` even
when today's kw is plain JSON.**

Related: `kw["gbuffer"]` is auto-decrefed by the serializer table when the kw is
decrefed. Reading the pointer with `extract=FALSE` and then calling
`GBUFFER_DECREF` is a double free.

### The singletons do not count

`json_null()`, `json_true()` and `json_false()` carry `refcount == (size_t)-1`,
and incref/decref on them do nothing. So this is safe:

```c
"template_settings", template_settings? template_settings: json_null()   // s:O, safe
```

and this **leaks one empty array per call** when the value is missing, because
`O` increfs a fresh array that nobody owns afterwards:

```c
"scopes", scopes? scopes: json_array()                                   // s:O, LEAKS
```

### `json_pack()` can return NULL

A `s:s` whose string is `NULL` is an error: the whole `json_pack()` returns
`NULL`. Jansson releases the `o` values it reaches while unwinding, so the
ownership is not lost — but the caller gets nothing where it expected an
object. **Check the return**, or say what you mean:

- `s:s*` — omit the key when the string is `NULL`.
- `s:o?` — store a json null instead of the value.

### Fix the pair, never one half

`json_incref(kw)` plus `JSON_DECREF(kw)` in the same function is two errors
that cancel out. Correcting only the incref turns a wrong-but-balanced ledger
into a **leak**; correcting only the decref turns it into a **double free**.
When you touch one side, check the other in the same function.

---

## The checklist

Three questions, at every call that takes or returns a json:

1. **Who created it?**
2. **Who else points at it after this line?** (a `O`, a `set()`, an `append()`,
   an incref — each one adds an owner)
3. **Who drops the last reference, and on every exit path?**

And when the arithmetic is already wrong, the audit answers it: run the yuno in
the foreground with `YUNETA_TRACK_MEM_DUMP=1` to see **what** leaked (a leaked
string carries its own characters and names the field), then
`YUNETA_TRACK_MEM=<ref_min>-<ref_max>:<size>,...` to get the stack of the line
that allocated it. Full recipe in
[Debugging a yuno](../../../yunos/c/yuno_agent/DEBUGGING.md), §11.7.

---

## JSON Reference Count Macros: `JSON_DECREF` and `JSON_INCREF`

## 📌 Overview

The macros `JSON_DECREF` and `JSON_INCREF` manage the reference count of `json_t *` objects. This makes sure of proper memory management in applications using the Jansson library.

---

(JSON_DECREF)=

## 🔻 `JSON_DECREF(json)`

### **Description**
Decreases the reference count of a JSON object and frees it if the count reaches zero.

### **Parameters**
- **json** (`json_t *`) → The JSON object whose reference count must be decreased.

### **Return Value**
- **None** → This macro does not return a value.

### **Notes**
Use this macro to safely free JSON objects when they are no longer needed.

---

(JSON_INCREF)=
## 🔺 `JSON_INCREF(json)`

### **Description**
Increases the reference count of a JSON object, preventing it from being freed prematurely.

### **Parameters**
- **json** (`json_t *`) → The JSON object whose reference count must be increased.

### **Return Value**
- **None** → This macro does not return a value.

### **Notes**
Use this macro when passing a JSON object to multiple owners to make sure that it remains valid while in use.

---

## ✅ Conclusion

These macros help prevent memory leaks and segmentation faults when managing `json_t *` objects in Yuneta and other systems using Jansson.
