---
title: 'JS: String, list and general helpers'
description: >-
  The C-shaped string comparisons, the list helpers and the general
  utilities of @yuneta/gobj-js.
---

# String, list and general helpers

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js)

---

## String comparison

These four keep the names and the return values of the C library, so a gclass
that moves between the two languages keeps its comparisons.

(js_strcmp)=
### [`strcmp(str1, str2)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L2582)

Compares two strings. Returns a negative number, `0` or a positive number.

(js_strncmp)=
### [`strncmp(str1, str2, lgth)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L2558)

Compares the first `lgth` characters of two strings.

(js_strcasecmp)=
### [`strcasecmp(str1, str2)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L2600)

Compares two strings, and ignores the case.

(js_strstr)=
### [`strstr(haystack, needle, bool)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L2684)

Finds `needle` in `haystack`. Returns the part of `haystack` that begins at the
first occurrence. With `bool` set to `true` it returns the part **before** the
occurrence instead. Returns `false` when there is no occurrence.

(js_cmp_two_simple_json)=
### [`cmp_two_simple_json(jn_var1, jn_var2)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L2622)

Compares two simple JSON values, such as a string or a number. Returns a
negative number, `0` or a positive number.

---

## Lists

(js_str_in_list)=
### [`str_in_list(list, str, ignore_case)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L4529)

Tells if a string is in a list.

(js_strs_in_list)=
### [`strs_in_list(list, strs, ignore_case)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L4562)

Tells if one of the strings of `strs` is in a list.

(js_index_in_list)=
### [`index_in_list(list, elm)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L5782)

Gives the index of an element in a list. Returns `-1` when the list does not
hold it.

(js_id_index_in_obj_list)=
### [`id_index_in_obj_list(list, id)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L4514)

Gives the index of the record with the field `id` in a list of records. Returns
`-1` when the list does not hold it.

(js_delete_from_list)=
### [`delete_from_list(list, elm)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L4585)

Takes an element out of a list, in place.

(js_list2options)=
### [`list2options(list, field_id, field_value)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L4450)

Changes a list of strings, a list of records or an object of records into a list
of options with the form `[{id: "", value: ""}, …]`. A select element and a
combo box take that form.

---

## Time

(js_current_timestamp)=
### [`current_timestamp()`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L5749)

Gives the time of now as an ISO 8601 string with the milliseconds and the time
zone, such as `"2024-02-24T14:05:30.123+0200"`.

(js_get_now)=
### [`get_now()`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L5774)

Gives the UTC time of now.

(js_timeTracker)=
### [`timeTracker(tracker_name, verbose)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L5714)

Builds a small tracker of times, for a measurement in development.

```javascript
const tracker = timeTracker("Track1");
tracker.mark("Step 1");
tracker.mark("Step 2");
```

---

## General utilities

(js_build_path)=
### [`build_path(...segments)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L5984)

Builds a path or a URL from its segments, with exactly one `/` between each
pair. It follows the C [`build_path()`](../helpers/string_helper.md#build_path).

- The first segment is the root. It keeps its own leading `/`, so an absolute
  path stays absolute. A root made only of `/` stays `/`.
- The function removes a `/` at the start and at the end of each later
  segment, and a double `/` inside it.
- A `.` or a `..` in a later segment is resolved, and a `..` never goes above
  the root.
- A `null`, `undefined` or empty segment is skipped.

Use it for every path. A path that is built with a template string gives two
slashes, or none, as soon as one segment changes.

```javascript
build_path(bff_url, "auth", "login");       // "https://bff.example.com/auth/login"
build_path("/", "tmp");                     // "/tmp"
build_path("/store", "../../etc");          // "/store/etc"
```

Before gobj-js 7.29.0, a root `/` was lost (`("/", "tmp")` gave `"tmp"`), and
a `.` or a `..` stayed in the path as it came (`("/", "..", "etc")` gave
`"../etc"`).

(js_clean_name)=
### [`clean_name(name)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L5882)

Gives a name without the characters that a name must not hold.

(js_node_uuid)=
### [`node_uuid()`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L5839)

Gives the unique identifier of this browser (a uuid). The function gives the
same uuid for all the life of the page (gobj-js 7.26.28). The yuno writes it in
its `node_uuid` attribute, and [`C_IEVENT_CLI`](builtin_gclasses.md) sends it as
the id of the identity card and as the `host` of each message.

- The local storage keeps the uuid in the key `host_uuid`. The first page makes
  it and writes it there. The next pages read it.
- When the store cannot keep the uuid, the page makes a uuid and keeps it in
  memory. When the data of the site is blocked or the store is full, the
  function logs one WARNING. When there is no `localStorage` or no browser (a
  test under node), it logs nothing.
- Without `crypto.randomUUID()` (a page on `http` without TLS is not a secure
  context), the function makes a uuid v4 with `crypto.getRandomValues()`.
  Without `crypto`, it uses `Math.random()`.
- A stored value that is not a uuid (a foreign or corrupt write: `"garbage"`,
  `"42"`) is replaced by a new uuid, written to the store, with one WARNING
  (gobj-js 7.26.29).

Before 7.26.28, the function gave a new uuid on each call when the store could
not keep it, and `null` without `crypto.randomUUID()`.

```javascript
const a = node_uuid();   // "3f2c9a6e-0d4b-4c1e-9a7f-5b8e2d1c4a90"
const b = node_uuid();   // the same uuid, also with the store blocked
```

(js_debounce)=
### [`debounce(func, wait, immediate)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L5918)

Gives a function that waits `wait` milliseconds after the last call before it
runs `func`. With `immediate` set to `true` it runs `func` on the first call
instead of the last.

Use it for a rate that the user controls, such as the keys of a search box.
Do not use it to delay work inside a gclass. Use
[`gobj_post_event()`](events.md#js_gobj_post_event) for that.

(js_get_function_name)=
### [`get_function_name(func)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L5890)

Gives the name of a function. A trace of an action uses it.

(js_create_json_record)=
### [`create_json_record(json_desc, value)`](https://github.com/artgins/gobj-js/blob/7.29.0/src/helpers.js#L5131)

Builds a record from a description of its fields, and puts the default value of
each field in it.
