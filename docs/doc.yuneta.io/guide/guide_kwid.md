(kwid)=

# **Kwid**

## Overview of `kwid`

`kwid` is a library designed to provide a higher-level abstraction for managing JSON data across multiple programming languages. It extends basic JSON handling capabilities by introducing advanced features like path-based manipulation, cloning, filtering, serialization, and database-like utilities.

In **C**, the library is built on top of the [Jansson library](https://jansson.readthedocs.io/), while in other languages like **JavaScript** and **Python**, it uses native types (`bool`, `array`, `object` in JS and `list`, `dict` in Python). This design makes sure of seamless integration with the native JSON structures of each language. This enables consistent behavior and cross-platform portability.

Source code in:
- [kwid.c](https://github.com/artgins/yunetas/blob/7.17.2/kernel/c/gobj-c/src/kwid.c)
- [kwid.h](https://github.com/artgins/yunetas/blob/7.17.2/kernel/c/gobj-c/src/kwid.h)

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
    - Provides record-based operations such as [`kwid_find_record_in_list`](https://github.com/artgins/yunetas/blob/7.17.2/kernel/c/gobj-c/src/kwid.c#L937), [`kwid_compare_records`](#kwid_compare_records), and [`kwjr_get`](#kwjr_get).
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
