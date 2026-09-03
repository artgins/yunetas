---
title: 'JS: kw, kwid and inter-event helpers'
description: >-
  The helpers that read, write, filter and clone a kw, the kwid record
  helpers, and the inter-event metadata of a message.
---

# `kw`, `kwid` and inter-event helpers

A `kw` is the JSON payload that travels with every event. These helpers read
it, write it, filter it and clone it.

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js)

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
## [`kw_flag_t`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L15)

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
### [`kw_has_key(kw, key)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1006)

Tells if `kw` has the key as its own property. It takes a key, not a path, and
it takes no `gobj`. Returns a boolean.

(js_kw_find_path)=
### [`kw_find_path(gobj, kw, path, verbose)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1019)

Gives the value at a back-tick path. Returns `undefined` when the path does not
exist. With `verbose` set to `true` the function writes a log error first.

(js_kw_delete)=
### [`kw_delete(gobj, kw, path)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1066)

Deletes the key at a back-tick path. Returns `0`.

(js_kw_pop)=
### [`kw_pop(kw1, kw2)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L985)

Deletes from `kw1` every key that `kw2` names. `kw2` can be a string, an object
or an array, and an array goes down into each of its elements. It takes no
`gobj`, and it returns nothing.

(js_kw_set_dict_value)=
### [`kw_set_dict_value(gobj, kw, path, value)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1402)

Writes a value at a back-tick path, and creates the intermediate objects that
the path needs.

(js_kw_set_subdict_value)=
### [`kw_set_subdict_value(gobj, kw, path, key, value)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1440)

Writes `key` inside the object at `path`, and creates that object when it does
not exist.

---

## Typed readers

Every reader takes the same five parameters, and each one gives the type of its
name. A value of a different type gives the default value back.

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
### [`kw_get_bool()`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1085)

Reads a boolean.

(js_kw_get_int)=
### [`kw_get_int()`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1117)

Reads an integer.

(js_kw_get_real)=
### [`kw_get_real()`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1160)

Reads a real number.

(js_kw_get_str)=
### [`kw_get_str()`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1203)

Reads a string. The function puts the result through `String()`, so the return
value is always a string.

(js_kw_get_dict)=
### [`kw_get_dict()`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1281)

Reads an object.

(js_kw_get_dict_value)=
### [`kw_get_dict_value()`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1324)

Reads a value of any type from an object.

(js_kw_get_list)=
### [`kw_get_list()`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1359)

Reads an array.

(js_kw_get_pointer)=
### [`kw_get_pointer()`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1246)

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
### [`kw_match_simple(kw, jn_filter)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1556)

Tells if `kw` matches a filter. It compares strings and numbers only. An empty
filter matches everything. It takes no `gobj`.

(js_kw_select)=
### [`kw_select(gobj, kw, jn_filter, match_fn)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1605)

Gives a new list with a **deep copy** of each row that matches the filter. Use
it when the caller changes the rows. With `match_fn` empty the function uses
[`kw_match_simple()`](#js_kw_match_simple).

(js_kw_collect)=
### [`kw_collect(gobj, kw, jn_filter, match_fn)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1642)

Gives a new list with a **reference** to each row that matches the filter. It is
[`kw_select()`](#js_kw_select) without the copy, so a change to a row changes
the source.

(js_kw_find_json_in_list)=
### [`kw_find_json_in_list(gobj, kw_list, item, flag)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1573)

Gives the index of a simple JSON item in a list. Returns `-1` when the list does
not hold it.

(js_kw_clone_by_keys)=
### [`kw_clone_by_keys(gobj, kw, keys, verbose)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1684)

Gives a new object with the keys that `keys` names. `keys` can be a string, an
array of strings or an object. It is not a deep copy. With empty keys the
function gives `kw` back.

(js_kw_clone_by_not_keys)=
### [`kw_clone_by_not_keys(gobj, kw, keys, verbose)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1736)

Gives a new object without the keys that `keys` names. It is the opposite of
[`kw_clone_by_keys()`](#js_kw_clone_by_keys).

---

## Local storage

These three helpers put a value in the local storage of the browser. The
persistent attributes use them. See [Persistence](persistence.md).

(js_kw_get_local_storage_value)=
### [`kw_get_local_storage_value(key, default_value, create)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1787)

Reads an attribute from the local storage. With `create` set to `true` the
function writes the default value when the key does not exist.

(js_kw_set_local_storage_value)=
### [`kw_set_local_storage_value(key, value)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1820)

Writes an attribute to the local storage. Returns `0` on success, and `-1` when
the value did not reach the store. An older version returned nothing and only
wrote to the console, so no caller saw the failure.

(js_kw_remove_local_storage_value)=
### [`kw_remove_local_storage_value(key)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1843)

Deletes an attribute from the local storage.

---

## `kwid` record helpers

A `kwid` is a collection of records. It can be a list of strings, a list of
objects, or an object of objects with the identifier as its key. These helpers
read the three forms in the same way.

(js_kwid_match_id)=
### [`kwid_match_id(ids, id)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1864)

Tells if `id` is in the collection `ids`. An empty `ids` matches every
identifier, because no filter lets everything through.

(js_kwid_collect)=
### [`kwid_collect(gobj, kw, ids, jn_filter, match_fn)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1914)

Gives a new list with the records that match both `ids` and the filter. With
`match_fn` empty the function uses [`kw_match_simple()`](#js_kw_match_simple).

(js_kwid_find_one_record)=
### [`kwid_find_one_record(gobj, kw, ids, jn_filter, match_fn)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L2007)

Gives the first record that matches. It takes the parameters of
[`kwid_collect()`](#js_kwid_collect).

(js_kwid_new_dict)=
### [`kwid_new_dict(gobj, kw, path)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L1976)

Builds an object of objects from a list of records, with the field `id` of each
record as the key. With a `path` that is not empty the function reads the list
at that path first. The function gives an unchanged result for a `kw` that is
an object already.

(js_kwid_get_ids)=
### [`kwid_get_ids(gobj, ids)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L2047)

Gives the list of the identifiers of a collection. It accepts a string, a list
of strings, a list of records or an object of records.

---

## Inter-event metadata

The inter-event protocol carries its metadata inside the `kw`, in the key
`__md_iev__`. Read and write it with these helpers and never by hand: the key
name is an internal detail, and a message that goes to a remote yuno and comes
back keeps only what these helpers wrote.

(js_msg_iev_write_key)=
### [`msg_iev_write_key(kw, key, value)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L2278)

Writes a key in the metadata of the message, and creates the metadata object
when it does not exist. It takes no `gobj`.

(js_msg_iev_read_key)=
### [`msg_iev_read_key(kw, key)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L2262)

Reads a key of the metadata. Returns `undefined` when the message has no
metadata. It takes no `gobj`.

(js_msg_iev_push_stack)=
### [`msg_iev_push_stack(gobj, kw, stack, jn_data)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L2291)

Puts `jn_data` on a stack with a name inside the metadata. The stack carries the
data of one hop when a message goes through more than one yuno.

(js_msg_iev_get_stack)=
### [`msg_iev_get_stack(gobj, kw, stack, verbose)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L2319)

Reads the top of a stack with a name. It does not take the element out.

(js_msg_iev_set_msg_type)=
### [`msg_iev_set_msg_type(gobj, kw, msg_type)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L2368)

Writes the type of the message. An empty string deletes the key. Returns `0`.

(js_msg_iev_get_msg_type)=
### [`msg_iev_get_msg_type(gobj, kw)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L2385)

Reads the type of the message. Returns an empty string when the message has
none.

---

## Metadata and private keys

(js_is_metadata_key)=
### [`is_metadata_key(key)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L944)

Tells if a key is a metadata key. A metadata key begins with two underscores.

(js_is_private_key)=
### [`is_private_key(key)`](https://github.com/artgins/gobj-js/blob/7.16.2/src/helpers.js#L964)

Tells if a key is a private key. A private key begins with one underscore.
