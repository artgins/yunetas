# Helpers & Utilities

## JSON / object operations

```javascript
json_deep_copy(obj)
json_is_identical(a, b)

json_object_update(dst, src)           // merge src into dst
json_object_update_existing(dst, src)  // only existing keys
json_object_update_missing(dst, src)   // only missing keys

json_object_get(obj, key)
json_object_set(obj, key, value)
json_object_del(obj, key)

json_array_append(arr, value)
json_array_remove(arr, index)
json_array_extend(dst, src)

json_object_size(obj)
json_array_size(arr)
```

## Type checking

```javascript
is_object(v), is_array(v), is_string(v), is_number(v),
is_boolean(v), is_null(v), is_date(v), is_function(v), is_gobj(v)
```

## Keyword (`kw`) operations

Most `kw_*` helpers take `gobj` as the first argument (for error
logging) and a dot / back-tick path instead of a single key.
**Exceptions that do NOT take `gobj`:** `kw_has_key`, `kw_pop`,
`kw_match_simple`.

```javascript
kw_has_key(kw, key)                             // → boolean
kw_pop(kw1, kw2)                                // delete from kw1 the keys in kw2
kw_delete(gobj, kw, path)
kw_find_path(gobj, kw, path, verbose)           // back-tick path: "a`b`c"
```

### Typed getters

```javascript
kw_get_bool(gobj, kw, path, default_value, flag)
kw_get_int (gobj, kw, path, default_value, flag)
kw_get_real(gobj, kw, path, default_value, flag)
kw_get_str (gobj, kw, path, default_value, flag)
kw_get_dict(gobj, kw, path, default_value, flag)
kw_get_list(gobj, kw, path, default_value, flag)
kw_get_dict_value(gobj, kw, path, default_value, flag)
```

**Flags:** `KW_REQUIRED`, `KW_CREATE`, `KW_EXTRACT`, `KW_RECURSIVE`,
`KW_WILD_NUMBER`.

### Setters

```javascript
kw_set_dict_value(gobj, kw, path, value)
kw_set_subdict_value(gobj, kw, path, key, value)
```

### Matching and filtering

```javascript
kw_match_simple(kw, filter)                     // → boolean
kw_select (gobj, kw, jn_filter, match_fn)       // filter list
kw_collect(gobj, kw, jn_filter, match_fn)       // extract subset

kw_clone_by_keys    (gobj, kw, keys, verbose)
kw_clone_by_not_keys(gobj, kw, keys, verbose)
```

## Inter-event message stack

The inter-event protocol uses a small stack embedded in `kw` to carry
per-hop metadata when messages traverse multiple yunos.

```javascript
msg_iev_push_stack  (gobj, kw, stack, jn_data)  // push jn_data onto named stack
msg_iev_get_stack   (gobj, kw, stack, verbose)  // peek top of named stack
msg_iev_set_msg_type(gobj, kw, msg_type)        // "" to delete
msg_iev_get_msg_type(gobj, kw)
msg_iev_write_key(kw, key, value)               // no gobj
msg_iev_read_key (kw, key)                      // no gobj
```

## Misc utilities

```javascript
current_timestamp()         // ISO string
get_now()                   // Date object
node_uuid()                 // generate UUID
parseBoolean(v)             // coerce to boolean
empty_string(s)             // true if null/undefined/""

str_in_list(list, str, ci)  // case-insensitive optional
index_in_list(list, value)
delete_from_list(list, value)

jwtDecode(token)            // → { header, payload, signature }
jwt2json(token)             // → payload JSON

debounce(fn, delay)
timeTracker()
```
