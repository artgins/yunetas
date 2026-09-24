---
title: 'JS: kw, kwid and inter-event helpers'
description: >-
  The helpers that read, write, filter and clone a kw, the kwid record
  helpers, and the inter-event metadata of a message.
---

# `kw`, `kwid` and inter-event helpers

A `kw` is the JSON payload that travels with every event. These helpers read
it, write it, filter it and clone it.

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js)

:::{important}
A path uses the **back-tick** as its separator, not the point:
``"__md_iev__`__msg_type__"``. A path with a point in it is one key with a
point in the name.
:::

Most helpers take `gobj` as their first parameter, and they use it only to name
the gobj in a log message. `kw_has_key()`, `kw_pop()` and `kw_match_simple()`
are the exceptions, and they take no `gobj`.

---

(js_kw_flag_t)=
## [`kw_flag_t`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L15)

The flags of the typed readers. Combine them with the bit-or operator.

| Flag | Effect |
|---|---|
| `KW_REQUIRED` | Writes a log error when the path does not exist. |
| `KW_CREATE` | Creates the path with the default value when it does not exist. |
| `KW_WILD_NUMBER` | Accepts a real, an integer, a boolean or a string for a number, and writes no log. |
| `KW_EXTRACT` | Deletes the key after the read. |
| `KW_BACKWARD` | Searches from the end in a list. |
| `KW_VERBOSE` | Writes a log message when the operation fails. |
| `KW_LOWER` | Puts the key in lower case. |
| `KW_RECURSIVE` | Goes down into the sub-objects. |

---

## Read and write

(js_kw_has_key)=
### [`kw_has_key(kw, key)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1058)

Tells if `kw` has the key as its own property. It takes a key, not a path, and
it takes no `gobj`. Returns a boolean.

(js_kw_find_path)=
### [`kw_find_path(gobj, kw, path, verbose)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1071)

Gives the value at a back-tick path. Returns `undefined` when the path does not
exist. With `verbose` set to `true` the function writes a log error first.

(js_kw_delete)=
### [`kw_delete(gobj, kw, path)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1118)

Deletes the key at a back-tick path. Returns `0`.

(js_kw_pop)=
### [`kw_pop(kw1, kw2)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1037)

Deletes from `kw1` every key that `kw2` names. `kw2` can be a string, an object
or an array, and an array goes down into each of its elements. It takes no
`gobj`, and it returns nothing.

(js_kw_set_dict_value)=
### [`kw_set_dict_value(gobj, kw, path, value)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1495)

Writes a value at a back-tick path, and creates the intermediate objects that
the path needs.

(js_kw_set_subdict_value)=
### [`kw_set_subdict_value(gobj, kw, path, key, value)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1533)

Writes `key` inside the object at `path`, and creates that object when it does
not exist.

---

## Typed readers

Every reader takes the same five parameters, and each one gives the type of its
name. A value of a different type, or a key that is not there, gives the
default value back **as you gave it**, as the C readers do. With
`KW_REQUIRED` a missing key or a value of a different type writes a log error.
In `kw_get_bool()`, `kw_get_dict()` and `kw_get_list()`, `KW_CREATE` writes
only a default of the right type, and `KW_EXTRACT` deletes only a value of the
right type, as in C. Before gobj-js 7.25.2, `kw_get_list()`, `kw_get_dict()` and
`kw_get_bool()` did not keep this rule (see each one below).

```javascript
kw_get_bool      (gobj, kw, path, default_value, flag)
kw_get_int       (gobj, kw, path, default_value, flag)
kw_get_real      (gobj, kw, path, default_value, flag)
kw_get_str       (gobj, kw, path, default_value, flag)
kw_get_dict      (gobj, kw, path, default_value, flag)
kw_get_list      (gobj, kw, path, default_value, flag)
kw_get_dict_value(gobj, kw, path, default_value, flag)
kw_get_pointer   (gobj, kw, path, default_value, flag)
```

(js_kw_get_bool)=
### [`kw_get_bool()`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1141)

Reads a boolean. Only a boolean is read: any other value gives the default
back, as a boolean. With `KW_WILD_NUMBER` the function also reads a number (`0`
is false), a string (`"true"` or `"false"` in any case, else its integer) and
`null` (false), as in C. Before gobj-js 7.25.2 the value went through
`Boolean()`, so the string `"false"` was true.

```js
kw_get_bool(gobj, {on: true}, "on", false, 0);                        // true
kw_get_bool(gobj, {}, "on", true, 0);                                 // true (the default)
kw_get_bool(gobj, {on: "false"}, "on", true, 0);                      // true: a string is not a boolean
kw_get_bool(gobj, {on: "false"}, "on", true, kw_flag_t.KW_WILD_NUMBER); // false
kw_get_bool(gobj, {on: 0}, "on", true, kw_flag_t.KW_WILD_NUMBER);       // false
```

(js_kw_get_int)=
### [`kw_get_int()`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1198)

Reads an integer.

(js_kw_get_real)=
### [`kw_get_real()`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1241)

Reads a real number.

(js_kw_get_str)=
### [`kw_get_str()`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1289)

Reads a string. If the key has a string, you get that string. If the key is
not there, or its value is not a string, you get the default **as you gave
it**, as in C. Before gobj-js 7.25.1 the function put the default through
`String()`, so a default of `0` or `null` came back as `"0"` or `"null"`, and
an `if()` on the result was true.

With `KW_CREATE`, a string default is written into the kw. Any other default
is written as `null`.

```js
kw_get_str(gobj, {name: "x"}, "name", "", 0);     // "x"
kw_get_str(gobj, {}, "name", "", 0);              // ""
kw_get_str(gobj, {}, "name", null, 0);            // null (not "null")
kw_get_str(gobj, {name: 5}, "name", "none", 0);   // "none": 5 is not a string
```

(js_kw_get_dict)=
### [`kw_get_dict()`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1371)

Reads an object. An object found is given back as it is (the same object, not
a copy). Anything else gives the default back as you gave it, `null` included.
Before gobj-js 7.25.2 the default went through `Object()`, so a `null` default
came back as `{}`, and a `0` as a `Number` object.

```js
kw_get_dict(gobj, {cfg: {a: 1}}, "cfg", {}, 0);    // {a: 1}, the object in the kw
kw_get_dict(gobj, {}, "cfg", null, 0);             // null (not {})
kw_get_dict(gobj, {cfg: [1]}, "cfg", null, 0);     // null: a list is not a dict
let kw = {};
kw_get_dict(gobj, kw, "a`b", {}, kw_flag_t.KW_CREATE); // {} -- and kw is now {a: {b: {}}}
```

(js_kw_get_dict_value)=
### [`kw_get_dict_value()`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1413)

Reads a value of any type from an object.

(js_kw_get_list)=
### [`kw_get_list()`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1453)

Reads an array. An array found is given back as it is. Anything else gives the
default back as you gave it. Before gobj-js 7.25.2 the answer went through
`Array()`, which **wraps** its argument: a list found came back inside another
list, a default of `[]` as `[[]]`, and a default of `null` as `[null]`.

```js
kw_get_list(gobj, {ids: [1, 2]}, "ids", [], 0);    // [1, 2] (was [[1, 2]])
kw_get_list(gobj, {}, "ids", [], 0);               // [] (was [[]])
kw_get_list(gobj, {}, "ids", null, 0);             // null (was [null])
kw_get_list(gobj, {ids: 5}, "ids", null, 0);       // null: 5 is not a list
```

(js_kw_get_pointer)=
### [`kw_get_pointer()`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1332)

Reads a value that is not JSON, such as a gobj or a DOM node.

:::{warning}
A `kw` that holds a gobj, a widget or a DOM node breaks the `machine` trace.
The trace writes the `kw` with `trace_json()`, and those objects have cycles, so
the write throws. Put an identity in the `kw` instead, such as a key or an
identifier, and find the object inside the action.
:::

---

## Match and filter

(js_kw_match_simple)=
### [`kw_match_simple(kw, jn_filter)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1649)

Tells if `kw` matches a filter. It compares strings and numbers only. An empty
filter matches everything. It takes no `gobj`.

(js_kw_select)=
### [`kw_select(gobj, kw, jn_filter, match_fn)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1698)

Gives a new list with a **deep copy** of each row that matches the filter. Use
it when the caller changes the rows. With `match_fn` empty the function uses
[`kw_match_simple()`](#js_kw_match_simple).

(js_kw_collect)=
### [`kw_collect(gobj, kw, jn_filter, match_fn)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1735)

Gives a new list with a **reference** to each row that matches the filter. It is
[`kw_select()`](#js_kw_select) without the copy, so a change to a row changes
the source.

(js_kw_find_json_in_list)=
### [`kw_find_json_in_list(gobj, kw_list, item, flag)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1666)

Gives the index of a simple JSON item in a list. Returns `-1` when the list does
not hold it.

(js_kw_clone_by_keys)=
### [`kw_clone_by_keys(gobj, kw, keys, verbose)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1777)

Gives a new object with the keys that `keys` names. `keys` can be a string, an
array of strings or an object. It is not a deep copy. With empty keys the
function gives `kw` back.

(js_kw_clone_by_not_keys)=
### [`kw_clone_by_not_keys(gobj, kw, keys, verbose)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1829)

Gives a new object without the keys that `keys` names. It is the opposite of
[`kw_clone_by_keys()`](#js_kw_clone_by_keys).

---

## Local storage

These three helpers put a value in the local storage of the browser. The
persistent attributes use them. See [Persistence](persistence.md).

(js_kw_get_local_storage_value)=
### [`kw_get_local_storage_value(key, default_value, create)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1880)

Reads an attribute from the local storage. With `create` set to `true` the
function writes the default value when the key does not exist.

(js_kw_set_local_storage_value)=
### [`kw_set_local_storage_value(key, value)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1913)

Writes an attribute to the local storage. Returns `0` on success, and `-1` when
the value did not reach the store. An older version returned nothing and only
wrote to the console, so no caller saw the failure.

(js_kw_remove_local_storage_value)=
### [`kw_remove_local_storage_value(key)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1936)

Deletes an attribute from the local storage.

---

## `kwid` record helpers

A `kwid` is a collection of records. It can be a list of strings, a list of
objects, or an object of objects with the identifier as its key. These helpers
read the three forms in the same way.

(js_kwid_match_id)=
### [`kwid_match_id(ids, id)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1957)

Tells if `id` is in the collection `ids`. An empty `ids` matches every
identifier, because no filter lets everything through.

(js_kwid_collect)=
### [`kwid_collect(gobj, kw, ids, jn_filter, match_fn)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L2015)

Gives a new list with the records that match both `ids` and the filter. With
`match_fn` empty the function uses [`kw_match_simple()`](#js_kw_match_simple).

(js_kwid_find_one_record)=
### [`kwid_find_one_record(gobj, kw, ids, jn_filter, match_fn)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L2171)

Gives the first record that matches. It takes the parameters of
[`kwid_collect()`](#js_kwid_collect).

(js_kwid_new_dict)=
### [`kwid_new_dict(gobj, kw, path)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L2082)

Builds an object of objects from a list of records, with the field `id` of each
record as the key. With a `path` that is not empty the function reads the list
at that path first. The function gives an unchanged result for a `kw` that is
an object already.

(js_kwid_new_list)=
### [`kwid_new_list(gobj, kw, path)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L2134)

Builds a list of records from an object of records, and writes the KEY of each
record into the record as its `id`. With a `path` that is not empty the
function reads the object at that path first. The function gives the same list
back for a `kw` that is a list already.

This is the normalizing half of the pair: whatever shape the data arrives in,
what comes back is the shape a table indexed and sorted by `id` wants.

```js
kwid_new_list(gobj, {a: {n: 1}, b: {n: 2}});
// [{id: "a", n: 1}, {id: "b", n: 2}]
```

:::{warning}
The key WINS over an `id` that disagrees with it, and the record is written in
place — the function makes no copy. That is the behaviour of the C twin
(`json_object_set_new(v, "id", ...)`) and it is the point: an object keyed by
`id` whose records disagree with their own key is what this function is for.
:::

(js_kwid_get_ids)=
### [`kwid_get_ids(gobj, ids)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L2215)

Gives the list of the identifiers of a collection. It accepts a string, a list
of strings, a list of records or an object of records.

---

## Inter-event metadata

The inter-event protocol carries its metadata inside the `kw`, in the key
`__md_iev__`. Read and write it with these helpers and never by hand: the key
name is an internal detail, and a message that goes to a remote yuno and comes
back keeps only what these helpers wrote.

(js_msg_iev_write_key)=
### [`msg_iev_write_key(kw, key, value)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L2446)

Writes a key in the metadata of the message, and creates the metadata object
when it does not exist. It takes no `gobj`.

(js_msg_iev_read_key)=
### [`msg_iev_read_key(kw, key)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L2430)

Reads a key of the metadata. Returns `undefined` when the message has no
metadata. It takes no `gobj`.

(js_msg_iev_push_stack)=
### [`msg_iev_push_stack(gobj, kw, stack, jn_data)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L2459)

Puts `jn_data` on a stack with a name inside the metadata. The stack carries the
data of one hop when a message goes through more than one yuno.

(js_msg_iev_get_stack)=
### [`msg_iev_get_stack(gobj, kw, stack, verbose)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L2487)

Reads the top of a stack with a name. It does not take the element out.

(js_msg_iev_set_msg_type)=
### [`msg_iev_set_msg_type(gobj, kw, msg_type)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L2536)

Writes the type of the message. An empty string deletes the key. Returns `0`.

(js_msg_iev_get_msg_type)=
### [`msg_iev_get_msg_type(gobj, kw)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L2553)

Reads the type of the message. Returns an empty string when the message has
none.

---

## Metadata and private keys

(js_is_metadata_key)=
### [`is_metadata_key(key)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L996)

Tells if a key is a metadata key. A metadata key begins with two underscores.

(js_is_private_key)=
### [`is_private_key(key)`](https://github.com/artgins/gobj-js/blob/7.25.2/src/helpers.js#L1016)

Tells if a key is a private key. A private key begins with one underscore.
