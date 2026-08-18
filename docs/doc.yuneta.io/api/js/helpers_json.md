---
title: 'JS: JSON and type helpers'
description: >-
  The jansson-shaped JSON helpers of @yuneta/gobj-js and the type
  predicates that go with them.
---

# JSON and type helpers

The C side builds every payload with jansson. The JS side gives the same
function names on top of plain objects and arrays, so a gclass reads the same
in both languages.

**Source code:** [`src/helpers.js`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js)

:::{note}
The names that end in `_new` are names of jansson, where the function takes
the reference of the value. JavaScript has no reference count, so `json_object_set_new()` and
`json_object_set()` are the same function. Both exist to keep a C gclass and a
JS gclass close to each other, line by line.
:::

---

## Copy and compare

(js_json_deep_copy)=
### [`json_deep_copy(obj)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L247)

Gives a deep copy of one value, with `structuredClone()`.

(js_duplicate_objects)=
### [`duplicate_objects(obj)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L256)

Gives a new object in which every value is a deep copy.

(js_json_is_identical)=
### [`json_is_identical(a, b)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L269)

Tells if two JSON values hold the same data. It compares the content, and not
the reference.

---

## Update an object

The three update functions are **not recursive**. They change the destination in
place, and they go one level deep.

(js_json_object_update)=
### [`json_object_update(dst, src)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L317)

Adds the new keys of `src` and writes over the keys that `dst` holds already.

(js_json_object_update_existing)=
### [`json_object_update_existing(dst, src)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L332)

Writes only the keys that `dst` holds already. It adds nothing.

(js_json_object_update_missing)=
### [`json_object_update_missing(dst, src)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L349)

Adds only the keys that `dst` does not hold. It writes over nothing.

---

## Read and write

(js_json_object_get)=
### [`json_object_get(o, k)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L365)

Gives the value of a key.

(js_json_object_set)=
### [`json_object_set(o, k, v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L386)

Writes a key.

(js_json_object_set_new)=
### [`json_object_set_new(o, k, v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L394)

Writes a key. In JavaScript it is the same as
[`json_object_set()`](#js_json_object_set).

(js_json_object_del)=
### [`json_object_del(o, k)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L376)

Deletes a key.

(js_json_array_append)=
### [`json_array_append(a, v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L413)

Puts a value at the end of an array.

(js_json_array_append_new)=
### [`json_array_append_new(a, v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L421)

Puts a value at the end of an array. In JavaScript it is the same as
[`json_array_append()`](#js_json_array_append).

(js_json_array_remove)=
### [`json_array_remove(a, idx)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L429)

Takes the element at an index out of an array.

(js_json_array_extend)=
### [`json_array_extend(destination, source)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L437)

Puts every element of `source` at the end of `destination`, and gives
`destination` back. An empty source changes nothing.

---

## Size

(js_json_object_size)=
### [`json_object_size(a)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L449)

Gives the quantity of keys of an object. It gives `0` for every other type.

(js_json_array_size)=
### [`json_array_size(a)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L460)

Gives the length of an array. It gives `0` for every other type.

(js_json_size)=
### [`json_size(a)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L471)

Gives the size of any JSON value: the quantity of keys of an object, the length
of an array, `1` for a string that is not empty, and `0` for everything else.

---

## Type predicates

Each one gives a boolean.

(js_is_object)=
### [`is_object(v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L608)

Tells if the value is an object. An array is not an object here, and `null` is
not an object.

(js_is_array)=
### [`is_array(v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L615)

Tells if the value is an array.

(js_is_string)=
### [`is_string(v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L622)

Tells if the value is a string.

(js_is_number)=
### [`is_number(v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L629)

Tells if the value is a number. `NaN` and `Infinity` are not numbers here.

(js_is_boolean)=
### [`is_boolean(v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L636)

Tells if the value is a boolean.

(js_is_null)=
### [`is_null(v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L643)

Tells if the value is `null` or `undefined`.

(js_is_date)=
### [`is_date(v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L680)

Tells if the value is a `Date`.

(js_is_function)=
### [`is_function(v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L687)

Tells if the value is a function.

(js_is_gobj)=
### [`is_gobj(v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L694)

Tells if the value is a gobj.

(js_is_pure_number)=
### [`is_pure_number(v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L655)

Tells if a **string** holds a number. It accepts an integer, a real number, the
scientific form such as `"1.2e3"`, and a sign.

(js_empty_string)=
### [`empty_string(s)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L701)

Tells if a value is an empty string. A value that is not a string is empty too.

(js_parseBoolean)=
### [`parseBoolean(v)`](https://github.com/artgins/gobj-js/blob/7.12.0/src/helpers.js#L717)

Changes a string into a boolean. It accepts `true`, `on` and `1` as true. It
ignores the spaces around the value, and it ignores the case.
