---
title: 'JS: kw, kwid and inter-event helpers'
description: >-
  The helpers that read, write, filter and clone a kw, the kwid record
  helpers, and the inter-event metadata of a message.
---

# `kw`, `kwid` and inter-event helpers

A `kw` is the JSON payload that travels with every event. These helpers read
it, write it, filter it and clone it.

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js)

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
## [`kw_flag_t`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L16)

The flags of the typed readers. Combine them with the bit-or operator.

| Flag | Effect |
|---|---|
| `KW_REQUIRED` | Writes a log error when the path does not exist, and in `kw_get_dict()` and `kw_get_list()` also when its value has a different type. `kw_get_bool()`, `kw_get_int()`, `kw_get_real()` and `kw_get_str()` log a value of a different type also without it, as in C. Example: `kw_get_dict(gobj, {}, "cfg", null, kw_flag_t.KW_REQUIRED)` gives `null` and logs *"path not found: 'cfg'"*. |
| `KW_CREATE` | Creates the path with the default value when it does not exist. |
| `KW_WILD_NUMBER` | In `kw_get_int()`, `kw_get_real()` and `kw_get_bool()`: also reads a value of a simple type that is not the reader's own (a boolean, a number, a string, `null`), and writes no log for it. A list or a dict gives `0` (`false`) and a log error. |
| `KW_EXTRACT` | Deletes the key after the read, only when the reader answers with its value. |
| `KW_BACKWARD` | Searches from the end in a list. |
| `KW_VERBOSE` | Writes a log message when the operation fails. |
| `KW_LOWER` | Puts the key in lower case. |
| `KW_RECURSIVE` | Goes down into the sub-objects. |

---

## Read and write

(js_kw_has_key)=
### [`kw_has_key(kw, key)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L2781)

Tells if `kw` has the key as its own property. It takes a key, not a path, and
it takes no `gobj`. Returns a boolean.

(js_kw_find_path)=
### [`kw_find_path(gobj, kw, path, verbose)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L2849)

Gives the value at a back-tick path. Returns `undefined` when the path does not
exist, as the C `kw_find_path()` returns NULL. With `verbose` set to `true` the
function writes a log error first.

When `kw` is not a dict or a list, or when a segment in the middle of the path
holds a scalar, the function logs *"kw must be list or dict"* and returns
`undefined`. A middle segment that is absent or `null` returns `undefined`, and
logs only when `verbose` is `true`. So a typed reader (`kw_get_int()`,
`kw_get_str()`, ...) that gets a bad kw returns its default (since gobj-js
7.25.6; before, a bad kw returned `0` and a `null` middle segment threw a
`TypeError`):

```js
kw_find_path(null, {a: [{b: 1}, {b: 2}]}, "a`1`b", false);  // 2
kw_find_path(null, {a: null}, "a`b", false);                // undefined, no log
kw_get_int(null, null, "x", 5, 0);   // 5, logged: kw must be list or dict: 'x'
```

In a list, a segment is an index: digits only, read as C's `atoi()` reads
them. Any other segment is not found (since gobj-js 7.26.2; before,
`length` answered the size of the list). C answers item 0 for a segment
that is not a number.

```js
kw_find_path(null, {a: [1, 2]}, "a`1", false);       // 2
kw_find_path(null, {a: [1, 2]}, "a`length", false);  // undefined
```

(js_kw_delete)=
### [`kw_delete(gobj, kw, path)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L2920)

Deletes the key at a back-tick path, as the C `kw_delete()`: it returns `0`,
or `-1` with a logged error when the key or its parent is not there, or when
the last segment would be an item of a list (only a key of a dict is deleted).
Since gobj-js 7.26.6; before, a missing key answered `0` in silence and a
`length` segment threw a `TypeError`.

```js
kw_delete(gobj, {a: {b: 1}}, "a`b");     // 0, kw is {a: {}}
kw_delete(gobj, {a: [1, 2]}, "a`0");     // -1, logged: cannot delete an item of a list
kw_delete(gobj, {a: 1}, "b");            // -1, logged: path not found
```

(js_kw_pop)=
### [`kw_pop(kw1, kw2)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L2760)

Deletes from `kw1` every key that `kw2` names. `kw2` can be a string, an object
or an array, and an array goes down into each of its elements. It takes no
`gobj`, and it returns nothing.

(js_kw_set_dict_value)=
### [`kw_set_dict_value(gobj, kw, path, value)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3481)

Writes a value at a back-tick path, and creates the intermediate objects that
the path needs. It returns `0`, or `-1` with a logged error when `kw` is not an
object, when the path is empty (as C; since gobj-js 7.26.10 it no longer writes
`kw[""]`), or when a middle segment is `null` or a scalar: nothing can be
created inside one, so the kw is left as it was (C logs and stops there too). A
typed reader with `KW_CREATE` over such a path answers its default, logged and
NOT written -- `kw_get_dict()`, `kw_get_list()` and `kw_get_dict_value()` too.
For what `kw_get_dict()` and `kw_get_list()` do with `KW_CREATE` over a value
that IS there (a `null`, another type), see their entries below.

**C differs here**: since SDK 7.26.9 its json readers with `KW_CREATE` return
NULL when they cannot answer what `kw` holds (a write that fails, a path of
another type, no kw), because the C default is freed by then (see
[`kw_get_dict()` in C](../helpers/kwid.md)); in JS nothing is freed, and the
default comes back.

```js
const kw = {a: {}};
kw_set_dict_value(gobj, kw, "a`b`c", 1);       // 0, kw is {a: {b: {c: 1}}}
kw_set_dict_value(gobj, {a: null}, "a`b", 1);  // -1, logged
kw_set_dict_value(gobj, kw, "", 1);            // -1, logged: path empty
kw_get_int(gobj, {a: 5}, "a`b", 7, kw_flag_t.KW_CREATE); // 7, logged
```

An empty SEGMENT is refused too, by both writers (gobj-js 7.26.13, SDK 7.26.9):
``"a`"``, ``"`a"`` and ``"a``b"`` name no key. Before, JS wrote the key `""`
(`{a: {"": 1}}`) and C dropped the segment and wrote over `a`.

```js
kw_set_dict_value(gobj, {a: 5}, "a`", 1);      // -1, logged; a is still 5
```

A list on the path is walked, never grown: its segment is an index inside the
list (digits, read as C's `atoi()` reads them), and anything else is refused
with `-1` and a logged error (since gobj-js 7.26.4, the same rule as
[`kw_find_path()`](#js_kw_find_path)):

```js
const kw = {a: [1, 2]};
kw_set_dict_value(gobj, kw, "a`1", 9);         // 0, kw is {a: [1, 9]}
kw_set_dict_value(gobj, kw, "a`length", 0);    // -1, logged; the list is kept
kw_set_dict_value(gobj, kw, "a`5", 0);         // -1, logged: not in the list
```

(js_kw_set_subdict_value)=
### [`kw_set_subdict_value(gobj, kw, path, key, value)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3582)

Writes `key` inside the object at `path`, and creates that object when it does
not exist. Returns `0`, or `-1` with a log error when `path` cannot be written
(not a string, a segment that is not an index of its list, a value that is not
an object): nothing is written then (since gobj-js 7.26.8).

```js
let kw = {a: [1, 2]};
kw_set_subdict_value(gobj, kw, "cfg`net", "port", 80);    // 0: kw.cfg.net.port === 80
kw_set_subdict_value(gobj, kw, "a`length", "port", 80);   // -1, logged; kw.a is kept
```

---

## Typed readers

Every reader takes the same five parameters, and each one gives the type of its
name. A value of a different type, or a key that is not there, gives the
default value back, as the C readers do: **as you gave it** in `kw_get_str()`,
`kw_get_dict()` and `kw_get_list()`, and converted to the reader's type in
`kw_get_bool()` (`Boolean()`), `kw_get_int()` (`parseInt()`) and
`kw_get_real()` (`Number()`):

```js
kw_get_str (gobj, {}, "name", null, 0);       // null, as given
kw_get_dict(gobj, {}, "cfg", null, 0);        // null, as given
kw_get_bool(gobj, {}, "on", 1, 0);            // true: Boolean(1)
kw_get_int (gobj, {}, "port", "8080", 0);     // 8080: parseInt("8080")
kw_get_real(gobj, {x: "1.5"}, "x", "2.5", 0); // 2.5, and logged: a string is not a number
```

With `KW_REQUIRED` a missing key or a value of a different type writes a log
error; `kw_get_bool()`, `kw_get_int()`, `kw_get_real()` and `kw_get_str()` log
a value of a different type also without it, as in C (`kw_get_str()` does not
log a `null` value). In `kw_get_bool()`, `kw_get_dict()`
and `kw_get_list()`, `KW_CREATE` writes only a default of the right type. In
every typed reader, `KW_EXTRACT` deletes only a value that the reader gives
back: a value of a different type stays in the kw. Before gobj-js 7.25.2,
`kw_get_list()`, `kw_get_dict()` and `kw_get_bool()` did not keep this rule,
before 7.25.4 `kw_get_int()`, `kw_get_real()` and `kw_get_str()` did not, and
before 7.25.5 `kw_get_str()` logged a value of a different type only with
`KW_REQUIRED` (see each one below).

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
### [`kw_get_bool()`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L2971)

Reads a boolean. Only a boolean is read: any other value gives the default
back, as a boolean, and writes a log error (*"path MUST BE a json boolean"*)
with or without `KW_REQUIRED`, as in C. With `KW_WILD_NUMBER` the function also
reads a number (`0` is false), a string (`"true"` or `"false"` in any case,
else its decimal integer, as C's `atoi()` reads it: `"0x1F"` is 0, so false)
and `null` (false), as in C; a list or a dict then gives `false` and a log
error (*"path MUST BE a simple json element"*). Before gobj-js 7.25.2 the value
went through `Boolean()`, so the string `"false"` was true. Before 7.25.3 a
value of another type gave the default back with no log. Before 7.25.5 the
string went through `parseInt()` with no base, which reads hex: `"0x1F"` was
true.

```js
kw_get_bool(gobj, {on: true}, "on", false, 0);                        // true
kw_get_bool(gobj, {}, "on", true, 0);                                 // true (the default)
kw_get_bool(gobj, {on: "false"}, "on", true, 0);                      // true, and logged: a string is not a boolean
kw_get_bool(gobj, {on: "false"}, "on", true, kw_flag_t.KW_WILD_NUMBER); // false
kw_get_bool(gobj, {on: 0}, "on", true, kw_flag_t.KW_WILD_NUMBER);       // false
kw_get_bool(gobj, {on: "12"}, "on", false, kw_flag_t.KW_WILD_NUMBER);   // true
kw_get_bool(gobj, {on: "0x1F"}, "on", true, kw_flag_t.KW_WILD_NUMBER);  // false: atoi("0x1F") is 0
```

(js_kw_get_int)=
### [`kw_get_int()`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3067)

Reads an integer. A number is read and truncated toward zero, as the C cast
does. Any other value gives the default back, as an integer (`parseInt()`), and
writes a log error (*"path MUST BE a json integer"*) with or without
`KW_REQUIRED`, as in C. With `KW_WILD_NUMBER` the function also reads a boolean
(`1` or `0`), a string (as C's `strtoll()` with base 0: `"0x1F"` is 31, `"017"`
is 15, `"12abc"` is 12, `"abc"` is 0) and `null` (0); a list or a dict then
gives `0` and a log error (*"path MUST BE a simple json element"*).
`KW_EXTRACT` deletes the key only when the function gives its value back.

Before gobj-js 7.25.4, `KW_EXTRACT` deleted the key before the type check (a
value that was not a number was lost), a value of another type was logged
only with `KW_REQUIRED`, `KW_WILD_NUMBER` had no effect, and a number went
through `parseInt()`, which reads `1e-7` as 1.

```js
kw_get_int(gobj, {port: 8080}, "port", 0, 0);                         // 8080
kw_get_int(gobj, {x: 3.9}, "x", 0, 0);                                // 3
kw_get_int(gobj, {}, "port", 80, 0);                                  // 80 (the default)
kw_get_int(gobj, {port: "8080"}, "port", 80, 0);                      // 80, and logged: a string is not a number
kw_get_int(gobj, {port: "8080"}, "port", 80, kw_flag_t.KW_WILD_NUMBER); // 8080
kw_get_int(gobj, {mask: "0x1F"}, "mask", 0, kw_flag_t.KW_WILD_NUMBER);  // 31

let kw = {port: "8080"};
kw_get_int(gobj, kw, "port", 80, kw_flag_t.KW_EXTRACT);  // 80, logged; kw is still {port: "8080"}
kw = {port: 8080};
kw_get_int(gobj, kw, "port", 80, kw_flag_t.KW_EXTRACT);  // 8080; kw is now {}
```

(js_kw_get_real)=
### [`kw_get_real()`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3132)

Reads a real number. A number is read as it is. Any other value gives the
default back, as a number (`Number()`), and writes a log error (*"path MUST BE a
json real"*) with or without `KW_REQUIRED`, as in C. With `KW_WILD_NUMBER` the
function also reads a boolean (`1` or `0`), a string (as `parseFloat()` reads
it; `0` when the string does not start with a number) and `null` (0); a list or
a dict then gives `0` and a log error (*"path MUST BE a simple json element"*).
`KW_EXTRACT` deletes the key only when the function gives its value back.
Before gobj-js 7.25.4 it did not keep these rules, as `kw_get_int()`.

```js
kw_get_real(gobj, {t: 21.5}, "t", 0, 0);                         // 21.5
kw_get_real(gobj, {t: "21.5"}, "t", 0, 0);                       // 0, and logged: a string is not a number
kw_get_real(gobj, {t: "21.5"}, "t", 0, kw_flag_t.KW_WILD_NUMBER);  // 21.5
kw_get_real(gobj, {t: null}, "t", 1, kw_flag_t.KW_WILD_NUMBER);    // 0
```

(js_kw_get_str)=
### [`kw_get_str()`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3228)

Reads a string. If the key has a string, you get that string. If the key is
not there, or its value is not a string, you get the default **as you gave
it**, as in C. Before gobj-js 7.25.1 the function put the default through
`String()`, so a default of `0` or `null` came back as `"0"` or `"null"`, and
an `if()` on the result was true.

What it logs is what C logs:

- A value that is there and is not a string (a number, a boolean, a dict, a
  list) writes a log error, *"path MUST BE a json str"*, with or without
  `KW_REQUIRED`. To read a value that can be a number, use `kw_get_int()` or
  `kw_get_dict_value()`, not this function.
- A `null` value writes no log, with or without `KW_REQUIRED`: the key is there
  and says "no string".
- With `KW_REQUIRED`, a path that is not there writes a log error, *"path not
  found"*.

Before gobj-js 7.25.5 a value of another type was logged only with
`KW_REQUIRED`, and then a `null` value was logged too.

With `KW_CREATE`, a string default is written into the kw. Any other default
is written as `null`. With `KW_EXTRACT` only a string is deleted from the kw;
before gobj-js 7.25.4 the key was deleted before the type check, so a value
that was not a string was lost. (The C `kw_get_str()` does not extract at all:
its answer would point into the freed json. A JS string does not have that
problem.)

```js
kw_get_str(gobj, {name: "x"}, "name", "", 0);     // "x"
kw_get_str(gobj, {}, "name", "", 0);              // ""
kw_get_str(gobj, {}, "name", null, 0);            // null (not "null")
kw_get_str(gobj, {name: 5}, "name", "none", 0);   // "none", and logged: 5 is not a string
kw_get_str(gobj, {name: null}, "name", "none", kw_flag_t.KW_REQUIRED);  // "none", not logged
kw_get_str(gobj, {}, "name", "none", kw_flag_t.KW_REQUIRED);            // "none", logged: path not found

let kw = {name: 5};
kw_get_str(gobj, kw, "name", "none", kw_flag_t.KW_EXTRACT);  // "none", logged; kw is still {name: 5}
```

(js_kw_get_dict)=
### [`kw_get_dict()`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3313)

Reads an object. An object found is given back as it is (the same object, not
a copy). Anything else gives the default back as you gave it, `null` included.
A value of a different type is logged with `KW_REQUIRED` (*"path MUST BE a json
dict"*) and, since gobj-js 7.26.14, with `KW_CREATE` (*"…, cannot create"*): the
caller means to write inside what it gets, and the default it gets is held by
nobody. With `KW_CREATE` a `null` at the path is "no value": a dict default
replaces it, as a missing key (since 7.26.14). A `null` with NO dict default
is a missing key too (since 7.26.16): it is given back as the default, said
only with `KW_REQUIRED` (*"path not found"*), never as *"cannot create"* --
the same as C.

Where C differs (SDK 7.26.9+): C answers NULL, not the default, for a value of
another type with `KW_CREATE` (the C default is freed by then); C replaces a
`null` with any non-NULL default, JS only with a dict default; and JS dumps the
kw (`trace_json_masked`) with the *"cannot create"* error, C does not.
Before gobj-js 7.25.2 the default went through `Object()`, so a `null` default
came back as `{}`, and a `0` as a `Number` object.

```js
kw_get_dict(gobj, {cfg: {a: 1}}, "cfg", {}, 0);    // {a: 1}, the object in the kw
kw_get_dict(gobj, {}, "cfg", null, 0);             // null (not {})
kw_get_dict(gobj, {cfg: [1]}, "cfg", null, 0);     // null: a list is not a dict
kw_get_dict(gobj, {cfg: [1]}, "cfg", null, kw_flag_t.KW_REQUIRED); // null, logged: path MUST BE a json dict
let kw = {};
kw_get_dict(gobj, kw, "a`b", {}, kw_flag_t.KW_CREATE); // {} -- and kw is now {a: {b: {}}}
kw_get_dict(gobj, {d: null}, "d", {}, kw_flag_t.KW_CREATE); // {}, and kw.d is it
kw_get_dict(gobj, {s: 5}, "s", {}, kw_flag_t.KW_CREATE);    // {} unwritten, logged
kw_get_dict(gobj, {d: null}, "d", null, kw_flag_t.KW_CREATE); // null, silent: a missing key
kw_get_dict(gobj, {d: null}, "d", null,
    kw_flag_t.KW_CREATE|kw_flag_t.KW_REQUIRED);              // null, logged: path not found
```

(js_kw_get_dict_value)=
### [`kw_get_dict_value()`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3376)

Reads a value of any type from an object.

(js_kw_get_list)=
### [`kw_get_list()`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3418)

Reads an array. An array found is given back as it is. Anything else gives the
default back as you gave it. A value of a different type is logged with
`KW_REQUIRED` (*"path MUST BE a json list"*) and, since gobj-js 7.26.14, with
`KW_CREATE`; with `KW_CREATE` a `null` at the path is replaced by a list
default, and with no list default it is a missing key (since 7.26.16) -- as
[`kw_get_dict()`](#js_kw_get_dict), where the differences with C are listed. Before gobj-js 7.25.2
the answer went through `Array()`, which **wraps** its argument: a list found
came back inside another list, a default of `[]` as `[[]]`, and a default of
`null` as `[null]`.

```js
kw_get_list(gobj, {ids: [1, 2]}, "ids", [], 0);    // [1, 2] (was [[1, 2]])
kw_get_list(gobj, {}, "ids", [], 0);               // [] (was [[]])
kw_get_list(gobj, {}, "ids", null, 0);             // null (was [null])
kw_get_list(gobj, {ids: 5}, "ids", null, 0);       // null: 5 is not a list
```

(js_kw_get_pointer)=
### [`kw_get_pointer()`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3272)

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
### [`kw_match_simple(kw, jn_filter)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3712)

Tells if `kw` matches a filter. It compares strings, numbers and booleans. An
empty filter matches everything. It takes no `gobj`.

```javascript
kw_match_simple({online: false, n: 0}, {online: false})   // true
kw_match_simple({n: 0}, {n: 0})                           // true
kw_match_simple({n: "abc"}, {n: 5})                       // false: "abc" reads 0
kw_match_simple({}, {n: 0})                               // false: no `n`
```

A false value (`0`, `false`, `""`) is a value to compare. Up to gobj-js 7.25.9
it was taken for "not there", and a filter on it matched nothing.

(js_kw_select)=
### [`kw_select(gobj, kw, jn_filter, match_fn)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3761)

Gives a new list with a **deep copy** of each row that matches the filter. Use
it when the caller changes the rows. With `match_fn` empty the function uses
[`kw_match_simple()`](#js_kw_match_simple).

(js_kw_collect)=
### [`kw_collect(gobj, kw, jn_filter, match_fn)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3798)

Gives a new list with a **reference** to each row that matches the filter. It is
[`kw_select()`](#js_kw_select) without the copy, so a change to a row changes
the source.

(js_kw_find_json_in_list)=
### [`kw_find_json_in_list(gobj, kw_list, item, flag)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3729)

Gives the index of a simple JSON item in a list. Returns `-1` when the list does
not hold it.

(js_kw_clone_by_keys)=
### [`kw_clone_by_keys(gobj, kw, keys, verbose)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3840)

Gives a new object with the keys that `keys` names. `keys` can be a string, an
array of strings or an object. It is not a deep copy. With empty keys the
function gives `kw` back.

(js_kw_clone_by_not_keys)=
### [`kw_clone_by_not_keys(gobj, kw, keys, verbose)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L3892)

Gives a new object without the keys that `keys` names. It is the opposite of
[`kw_clone_by_keys()`](#js_kw_clone_by_keys).

---

## Local storage

These three helpers put a value in the local storage of the browser. The
persistent attributes use them. See [Persistence](persistence.md).

(js_kw_get_local_storage_value)=
### [`kw_get_local_storage_value(key, default_value, create)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4002)

Reads an attribute from the local storage. It returns the JSON value of the
entry, or `default_value` when there is no value. With `create` set to `true`
the function also writes the default value there.

These cases give the default value (7.26.26):

- The key does not exist, or there is no browser (a test in Node.js). Before
  7.26.26, a read outside the browser returned `undefined`.
- The entry holds a JSON `null`. A `null` is no value, as `json_absent()` in C.
- The entry does not hold JSON (a corrupt or foreign write). The function logs
  one WARNING, which does not show the text, and removes the entry. The next
  read finds no entry, so it does not warn again. A new corrupt entry in the
  same key gives a new WARNING (7.26.27). Before 7.26.26, each read wrote the
  error to the console.
- The store cannot be read. The function logs one WARNING for each key, until
  a read of that key succeeds again. It does not write the default to that
  store, also with `create`. This includes a browser that blocks the data of
  the site (cookies blocked in Chrome, a sandboxed iframe): there, the read of
  `window.localStorage` itself throws a `SecurityError`. Before 7.26.27, that
  error went out of the function, so an app that reads its locale at start did
  not start.
- With `create`, the store can be read but not written (it is full). The
  function logs one WARNING for each key, until a read of that key succeeds
  again. Before 7.26.28, each read logged an ERROR, because each read tried to
  write the default again.

These answers are BREAKING in 7.26.26: an older version answered `null` for an
absent entry or a JSON `null`, and `undefined` outside a browser, and it kept an
entry that is not JSON.

```javascript
kw_get_local_storage_value("theme", "light", true);   // "light", and written if absent
localStorage.setItem("theme", "dark");               // not JSON
kw_get_local_storage_value("theme", "light", false);  // "light", one WARNING, entry removed
```

(js_kw_set_local_storage_value)=
### [`kw_set_local_storage_value(key, value)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4074)

Writes an attribute to the local storage. Returns `0` on success, and `-1` when
the value did not reach the store. An older version returned nothing and only
wrote to the console, so no caller saw the failure.

A value with no JSON text is refused with an ERROR and `-1`, as a typed attribute
writer refuses it (7.26.27): a function, a symbol, a `BigInt`, a cycle, `NaN` or
`Infinity`, a `Date`, a `Map`, an object with a `toJSON()`, and a function,
`undefined` or a hole inside an array. Before 7.26.27, a function was stored as
the text `"undefined"`, and `{w: NaN}` as `{"w":null}`. A key whose value is
`undefined` is an absent key, as in `JSON.stringify()`.

```javascript
kw_set_local_storage_value("rect", {x: 1, y: 2});     // 0
kw_set_local_storage_value("rect", {x: 1, y: NaN});   // -1, ERROR "a value with no json text"
kw_set_local_storage_value("rect", () => 1);          // -1, nothing stored
```

(js_kw_remove_local_storage_value)=
### [`kw_remove_local_storage_value(key)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4103)

Deletes an attribute from the local storage. A store that refuses the removal
gives a WARNING (7.26.27; before, a message on the console).

---

## `kwid` record helpers

A `kwid` is a collection of records. It can be a list of strings, a list of
objects, or an object of objects with the identifier as its key. These helpers
read the three forms in the same way.

(js_kwid_match_id)=
### [`kwid_match_id(ids, id)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4126)

Tells if `id` is in the collection `ids`. An empty `ids` matches every
identifier, because no filter lets everything through.

(js_kwid_collect)=
### [`kwid_collect(gobj, kw, ids, jn_filter, match_fn)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4184)

Gives a new list with the records that match both `ids` and the filter. With
`match_fn` empty the function uses [`kw_match_simple()`](#js_kw_match_simple).

(js_kwid_find_one_record)=
### [`kwid_find_one_record(gobj, kw, ids, jn_filter, match_fn)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4340)

Gives the first record that matches. It takes the parameters of
[`kwid_collect()`](#js_kwid_collect).

(js_kwid_new_dict)=
### [`kwid_new_dict(gobj, kw, path)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4251)

Builds an object of objects from a list of records, with the field `id` of each
record as the key. With a `path` that is not empty the function reads the list
at that path first. The function gives an unchanged result for a `kw` that is
an object already.

(js_kwid_new_list)=
### [`kwid_new_list(gobj, kw, path)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4303)

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
### [`kwid_get_ids(gobj, ids)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4384)

Gives the list of the identifiers of a collection. It accepts a string, a list
of strings, a list of records or an object of records.

---

## Inter-event metadata

The inter-event protocol carries its metadata inside the `kw`, in the key
`__md_iev__`. Read and write it with these helpers and never by hand: the key
name is an internal detail, and a message that goes to a remote yuno and comes
back keeps only what these helpers wrote.

(js_msg_iev_write_key)=
### [`msg_iev_write_key(kw, key, value)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4615)

Writes a key in the metadata of the message, and creates the metadata object
when it does not exist. It takes no `gobj`.

(js_msg_iev_read_key)=
### [`msg_iev_read_key(kw, key)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4599)

Reads a key of the metadata. Returns `undefined` when the message has no
metadata. It takes no `gobj`.

(js_msg_iev_push_stack)=
### [`msg_iev_push_stack(gobj, kw, stack, jn_data)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4628)

Puts `jn_data` on a stack with a name inside the metadata. The stack carries the
data of one hop when a message goes through more than one yuno.

(js_msg_iev_get_stack)=
### [`msg_iev_get_stack(gobj, kw, stack, verbose)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4656)

Reads the top of a stack with a name. It does not take the element out.

(js_msg_iev_set_msg_type)=
### [`msg_iev_set_msg_type(gobj, kw, msg_type)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4705)

Writes the type of the message. An empty string deletes the key. Returns `0`.

(js_msg_iev_get_msg_type)=
### [`msg_iev_get_msg_type(gobj, kw)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L4722)

Reads the type of the message. Returns an empty string when the message has
none.

---

## Metadata and private keys

(js_is_metadata_key)=
### [`is_metadata_key(key)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L2719)

Tells if a key is a metadata key. A metadata key begins with two underscores.

(js_is_private_key)=
### [`is_private_key(key)`](https://github.com/artgins/gobj-js/blob/7.28.1/src/helpers.js#L2739)

Tells if a key is a private key. A private key begins with one underscore.
