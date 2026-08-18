---
title: 'JS: String, list and general helpers'
description: >-
  The C-shaped string comparisons, the list helpers and the general
  utilities of @yuneta/gobj-js.
---

# String, list and general helpers

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js)

---

## String comparison

These four keep the names and the return values of the C library, so a gclass
that moves between the two languages keeps its comparisons.

(js_strcmp)=
### [`strcmp(str1, str2)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L758)

Compares two strings. Returns a negative number, `0` or a positive number.

(js_strncmp)=
### [`strncmp(str1, str2, lgth)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L734)

Compares the first `lgth` characters of two strings.

(js_strcasecmp)=
### [`strcasecmp(str1, str2)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L776)

Compares two strings, and ignores the case.

(js_strstr)=
### [`strstr(haystack, needle, bool)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L851)

Finds `needle` in `haystack`. Returns the part of `haystack` that begins at the
first occurrence. With `bool` set to `true` it returns the part **before** the
occurrence instead. Returns `false` when there is no occurrence.

(js_cmp_two_simple_json)=
### [`cmp_two_simple_json(jn_var1, jn_var2)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L798)

Compares two simple JSON values, such as a string or a number. Returns a
negative number, `0` or a positive number.

---

## Lists

(js_str_in_list)=
### [`str_in_list(list, str, ignore_case)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2134)

Tells if a string is in a list.

(js_strs_in_list)=
### [`strs_in_list(list, strs, ignore_case)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2167)

Tells if one of the strings of `strs` is in a list.

(js_index_in_list)=
### [`index_in_list(list, elm)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L3276)

Gives the index of an element in a list. Returns `-1` when the list does not
hold it.

(js_id_index_in_obj_list)=
### [`id_index_in_obj_list(list, id)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2119)

Gives the index of the record with the field `id` in a list of records. Returns
`-1` when the list does not hold it.

(js_delete_from_list)=
### [`delete_from_list(list, elm)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2190)

Takes an element out of a list, in place.

(js_list2options)=
### [`list2options(list, field_id, field_value)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2055)

Changes a list of strings, a list of records or an object of records into a list
of options with the form `[{id: "", value: ""}, …]`. A select element and a
combo box take that form.

---

## Time

(js_current_timestamp)=
### [`current_timestamp()`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L3245)

Gives the time of now as an ISO 8601 string with the milliseconds and the time
zone, such as `"2024-02-24T14:05:30.123+0200"`.

(js_get_now)=
### [`get_now()`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L3268)

Gives the UTC time of now.

(js_timeTracker)=
### [`timeTracker(tracker_name, verbose)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L3210)

Builds a small tracker of times, for a measurement in development.

```javascript
const tracker = timeTracker("Track1");
tracker.mark("Step 1");
tracker.mark("Step 2");
```

---

## General utilities

(js_build_path)=
### [`build_path(...segments)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L3408)

Builds a path or a URL from its segments, with exactly one `/` between each
pair. The first segment keeps its own leading `/`, so an absolute path stays
absolute. The function removes a `/` at the end of each segment.

Use it for every path. A path that is built with a template string gives two
slashes, or none, as soon as one segment changes.

(js_clean_name)=
### [`clean_name(name)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L3313)

Gives a name without the characters that a name must not hold.

(js_node_uuid)=
### [`node_uuid()`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L3291)

Builds a unique identifier for this browser, and keeps it.

(js_debounce)=
### [`debounce(func, wait, immediate)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L3349)

Gives a function that waits `wait` milliseconds after the last call before it
runs `func`. With `immediate` set to `true` it runs `func` on the first call
instead of the last.

Use it for a rate that the user controls, such as the keys of a search box.
Do not use it to delay work inside a gclass. Use
[`gobj_post_event()`](events.md#js_gobj_post_event) for that.

(js_get_function_name)=
### [`get_function_name(func)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L3321)

Gives the name of a function. A trace of an action uses it.

(js_create_json_record)=
### [`create_json_record(json_desc, value)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L2698)

Builds a record from a description of its fields, and puts the default value of
each field in it.
