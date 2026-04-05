# Kwid Functions

Source code in:
- [kwid.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/kwid.c)
- [kwid.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/kwid.h)

(kw_add_binary_type)=
## `kw_add_binary_type()`

Registers a new binary type for serialization and deserialization in the kw system. `kw_add_binary_type()` associates a binary field with its serialized representation and provides functions for handling reference counting.

```C
int kw_add_binary_type(
    const char *binary_field_name,
    const char *serialized_field_name,
    serialize_fn_t serialize_fn,
    deserialize_fn_t deserialize_fn,
    incref_fn_t incref_fn,
    decref_fn_t decref_fn
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `binary_field_name` | `const char *` | The name of the binary field to be registered. |
| `serialized_field_name` | `const char *` | The name of the corresponding serialized field. |
| `serialize_fn` | `serialize_fn_t` | Function pointer for serializing the binary field. |
| `deserialize_fn` | `deserialize_fn_t` | Function pointer for deserializing the binary field. |
| `incref_fn` | `incref_fn_t` | Function pointer for incrementing the reference count of the binary field. |
| `decref_fn` | `decref_fn_t` | Function pointer for decrementing the reference count of the binary field. |

**Returns**

Returns `0` on success, or `-1` if the maximum number of serialized fields has been reached.

**Notes**

The function maintains an internal table of registered binary types. If the table is full, an error is logged, and the function returns `-1`.

---

(kw_clone_by_keys)=
## `kw_clone_by_keys()`

The function `kw_clone_by_keys()` returns a new JSON object containing only the keys specified in `keys`. The keys can be provided as a string, a list of strings, or a dictionary where the keys are the desired fields. If `keys` is empty, the original JSON object is returned.

```C
json_t *kw_clone_by_keys(
    hgobj   gobj,
    json_t *kw,     // owned
    json_t *keys,   // owned
    BOOL    verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj (Generic Object) system, used for logging and error handling. |
| `kw` | `json_t *` | The JSON object to be filtered. This parameter is owned and will be decremented. |
| `keys` | `json_t *` | A JSON object specifying the keys to retain. It can be a string, a list of strings, or a dictionary. This parameter is owned and will be decremented. |
| `verbose` | `BOOL` | If `TRUE`, logs an error when a specified key is not found in `kw`. |

**Returns**

A new JSON object containing only the specified keys. If `keys` is empty, the original `kw` is returned. The caller owns the returned JSON object.

**Notes**

If `keys` is a dictionary or list, only the keys present in `keys` will be retained in the returned JSON object. If `verbose` is enabled, missing keys will be logged as errors.

---

(kw_clone_by_not_keys)=
## `kw_clone_by_not_keys()`

Return a new JSON object excluding the specified keys from the input `json_t *`. The keys can be provided as a string, dictionary, or list. If no keys are specified, an empty JSON object is returned.

```C
json_t *kw_clone_by_not_keys(
    hgobj      gobj,
    json_t     *kw,     // owned
    json_t     *keys,   // owned
    BOOL       verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Pointer to the GObj instance for logging and error handling. |
| `kw` | `json_t *` | The input JSON object to be cloned, excluding the specified keys. This parameter is owned and will be decremented. |
| `keys` | `json_t *` | A JSON object, list, or string specifying the keys to exclude from the cloned object. This parameter is owned and will be decremented. |
| `verbose` | `BOOL` | If `TRUE`, logs an error when a specified key is not found in the input JSON object. |

**Returns**

A new `json_t *` object with the specified keys removed. If no keys are specified, an empty JSON object is returned.

**Notes**

The function performs a shallow copy of the input JSON object, removing the specified keys. The input `kw` and `keys` parameters are owned and will be decremented.

---

(kw_clone_by_path)=
## `kw_clone_by_path()`

Return a new JSON object containing only the keys specified in `paths`. If `paths` is empty, the original JSON object is returned. This function does not perform a deep copy.

```C
json_t *kw_clone_by_path(
    hgobj gobj,
    json_t *kw,     // owned
    const char **paths
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj context. |
| `kw` | `json_t *` | The JSON object to be cloned. This parameter is owned and will be decremented. |
| `paths` | `const char **` | An array of key paths specifying which keys to include in the cloned JSON object. |

**Returns**

A new JSON object containing only the specified keys. If `paths` is empty, the original JSON object is returned.

**Notes**

This function does not perform a deep copy. The returned JSON object contains references to the original values.

---

(kw_collapse)=
## `kw_collapse()`

`kw_collapse()` returns a new JSON object where arrays or dictionaries exceeding specified size limits are collapsed into a placeholder structure indicating their path and size.

```C
json_t *kw_collapse(
    hgobj gobj,
    json_t *kw,         // not owned
    int collapse_lists_limit,
    int collapse_dicts_limit
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the calling object, used for logging and error reporting. |
| `kw` | `json_t *` | The JSON object to be processed. Must be a dictionary. |
| `collapse_lists_limit` | `int` | The maximum allowed size for lists before they are collapsed. |
| `collapse_dicts_limit` | `int` | The maximum allowed size for dictionaries before they are collapsed. |

**Returns**

A new JSON object with large lists and dictionaries collapsed. Returns `NULL` if `kw` is not a dictionary.

**Notes**

Collapsed elements are replaced with a structure containing `__collapsed__` metadata, including their path and size.

---

(kw_decref)=
## `kw_decref()`

`kw_decref()` decrements the reference count of a JSON object and its associated binary fields, freeing memory if necessary.

```C
json_t *kw_decref(
    json_t *kw
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `kw` | `json_t *` | The JSON object whose reference count will be decremented. |

**Returns**

Returns `NULL` if the operation is successful or if `kw` is already `NULL`. If an error occurs, logs an error message and returns `NULL`.

**Notes**

If `kw` is `NULL`, the function returns immediately.
If the reference count of `kw` is already zero or negative, an error is logged.
If `kw` contains binary fields, their reference counts are also decremented using the appropriate function.
Uses [`JSON_DECREF`](#JSON_DECREF) to safely decrement the reference count of `kw`.

---

(kw_delete)=
## `kw_delete()`

The function `kw_delete()` removes a value from a JSON dictionary based on a specified path.

```C
int kw_delete(
    hgobj    gobj,
    json_t  *kw,
    const char *path
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the gobj (generic object) that may be used for logging or context. |
| `kw` | `json_t *` | A JSON dictionary from which the value will be deleted. |
| `path` | `const char *` | The path to the value that should be removed from the dictionary. |

**Returns**

Returns 0 on success, or -1 if the specified path does not exist.

**Notes**

If the path does not exist, an error is logged, and the function returns -1. The function does not support deleting values from JSON arrays.

---

(kw_delete_metadata_keys)=
## `kw_delete_metadata_keys()`

Removes metadata keys from the given JSON object. Metadata keys are identified by the convention of starting with '__'.

```C
int kw_delete_metadata_keys(
    json_t *kw  // NOT owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `kw` | `json_t *` | A JSON object from which metadata keys will be removed. The object is not owned by the function. |

**Returns**

Returns 0 upon successful removal of metadata keys.

**Notes**

Metadata keys are identified as keys that start with '__'.
Only the first level of keys in the JSON object is checked and removed.
This function does not modify nested objects or arrays.

---

(kw_delete_private_keys)=
## `kw_delete_private_keys()`

Deletes private keys from the given JSON object. Private keys are identified by names that begin with a single underscore (`_`).

```C
int kw_delete_private_keys(
    json_t *kw  // NOT owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `kw` | `json_t *` | A JSON object from which private keys will be removed. The object is not owned by the function. |

**Returns**

Returns `0` on success.

**Notes**

This function only removes private keys at the first level of the JSON object. It does not perform a deep traversal.

---

(kw_delete_subkey)=
## `kw_delete_subkey()`

Deletes a sub-key from a JSON dictionary located at the specified path. If the sub-key does not exist, an error is logged.

```C
int kw_delete_subkey(
    hgobj    gobj,
    json_t  *kw,
    const char *path,
    const char *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The handler object used for logging errors. |
| `kw` | `json_t *` | The JSON dictionary from which the sub-key will be deleted. |
| `path` | `const char *` | The path to the dictionary containing the sub-key. |
| `key` | `const char *` | The sub-key to be deleted. |

**Returns**

Returns 0 on success, or -1 if the sub-key or dictionary is not found.

**Notes**

If the dictionary at the specified path does not exist, an error is logged. If the sub-key does not exist, an error is also logged.

---

(kw_deserialize)=
## `kw_deserialize()`

The function `kw_deserialize()` deserializes specific fields in a JSON object, converting stored serialized data back into its original binary representation.

```C
json_t *kw_deserialize(
    hgobj     gobj,
    json_t   *kw
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | ``hgobj`` | A handle to the gobj (generic object) that may be used for logging or context. |
| `kw` | ``json_t *`` | A JSON object containing serialized fields that need to be deserialized. |

**Returns**

Returns the same JSON object `kw` with its serialized fields converted back to their original binary representation.

**Notes**

The function iterates over predefined serialized fields and applies the corresponding deserialization function. If a field is found and successfully deserialized, the original serialized field is removed from the JSON object.

---

(kw_duplicate)=
## `kw_duplicate()`

`kw_duplicate()` creates a deep copy of a JSON object or array, processing serialized fields to ensure proper duplication.

```C
json_t *kw_duplicate(
    hgobj gobj,
    json_t *kw  // NOT owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Handle to the gobj (generic object) for logging and error handling. |
| `kw` | `json_t *` | The JSON object or array to duplicate. Must not be NULL. |

**Returns**

A new JSON object or array that is a deep copy of `kw`, with serialized fields properly handled. Returns `NULL` if `kw` is not an object or array.

**Notes**

Unlike `json_deep_copy()`, [`kw_duplicate()`](#kw_duplicate) processes serialized fields to ensure correct duplication of binary data.

---

(kw_filter_metadata)=
## `kw_filter_metadata()`

The function `kw_filter_metadata()` returns a duplicate of the given JSON object or array, removing all metadata keys that begin with '__'.

```C
json_t *kw_filter_metadata(
    hgobj gobj,
    json_t *kw  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj system, used for logging errors. |
| `kw` | `json_t *` | The JSON object or array to be filtered. This parameter is owned and will be decremented. |

**Returns**

A new JSON object or array with all metadata keys removed. The caller owns the returned JSON object.

**Notes**

Metadata keys are identified as those beginning with '__'. If `kw` is not an object or array, an error is logged, and `NULL` is returned.

---

(kw_filter_private)=
## `kw_filter_private()`

The function `kw_filter_private()` returns a duplicate of the given JSON object or array, removing all private keys that begin with a single underscore (`_`).

```C
json_t *kw_filter_private(
    hgobj gobj,
    json_t *kw  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj instance, used for logging errors. |
| `kw` | `json_t *` | The JSON object or array to be filtered. This parameter is owned and will be decremented. |

**Returns**

A new JSON object or array with private keys removed. The caller owns the returned JSON object.

**Notes**

Private keys are identified as those that begin with a single underscore (`_`). The function uses `_duplicate_object()` and `_duplicate_array()` to create a filtered copy of the input JSON.

---

(kw_find_json_in_list)=
## `kw_find_json_in_list()`

Search for a JSON item in a JSON list and return its index. If the item is not found, return -1. The comparison is performed using [`json_is_identical()`](<#json_is_identical>).

```C
int kw_find_json_in_list(
    hgobj     gobj,
    json_t   *kw_list,  // not owned
    json_t   *item,     // not owned
    kw_flag_t flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Pointer to the gobj context. |
| `kw_list` | `json_t *` | JSON array to search within. Must not be NULL. |
| `item` | `json_t *` | JSON item to search for in `kw_list`. Must not be NULL. |
| `flag` | `kw_flag_t` | Flags to modify behavior. If `KW_VERBOSE` is set, errors will be logged. |

**Returns**

Returns the index of `item` in `kw_list` if found, otherwise returns -1.

**Notes**

If `kw_list` is not a JSON array or `item` is NULL, the function returns -1. If `KW_VERBOSE` is set in `flag`, an error message is logged when `item` is not found.

---

(kw_find_path)=
## `kw_find_path()`

The function `kw_find_path()` retrieves a JSON value from a hierarchical JSON structure by following a specified path. It traverses dictionaries and lists to locate the desired value.

```C
json_t *kw_find_path(
    hgobj      gobj,
    json_t    *kw,
    const char *path,
    BOOL       verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the calling object, used for logging and error reporting. |
| `kw` | `json_t *` | The JSON object or array to search within. |
| `path` | `const char *` | The path to the desired value, using the configured delimiter (default: '`'). |
| `verbose` | `BOOL` | If `TRUE`, logs errors when the path is not found. |

**Returns**

Returns a pointer to the JSON value found at the specified path. If the path is invalid or not found, returns `NULL`.

**Notes**

The function [`kw_find_path()`](#kw_find_path) supports traversing both dictionaries and lists. If the path is invalid or the JSON structure is not an object or array, it logs an error if `verbose` is enabled.

---

(kw_find_str_in_list)=
## `kw_find_str_in_list()`

`kw_find_str_in_list()` searches for a string in a JSON list and returns its index if found, otherwise returns -1.

```C
int kw_find_str_in_list(
    hgobj      gobj,
    json_t *   kw_list,
    const char *str
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the gobj instance, used for logging errors. |
| `kw_list` | `json_t *` | A JSON array to search within. |
| `str` | `const char *` | The string to search for in the JSON list. |

**Returns**

Returns the index of the string in the list if found, otherwise returns -1.

**Notes**

The function only searches for exact string matches within the JSON array.

---

(kw_get_bool)=
## `kw_get_bool()`

Retrieves a boolean value from a JSON dictionary at the specified `path`. If the key does not exist, the `default_value` is returned. Supports type conversion from integers, strings, and null values.

```C
BOOL kw_get_bool(
    hgobj      gobj,
    json_t     *kw,
    const char *path,
    BOOL       default_value,
    kw_flag_t  flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Pointer to the GObj instance for logging and error handling. |
| `kw` | `json_t *` | JSON dictionary to search for the boolean value. |
| `path` | `const char *` | Path to the boolean value within the JSON dictionary. |
| `default_value` | `BOOL` | Default boolean value to return if the key is not found. |
| `flag` | `kw_flag_t` | Flags controlling behavior, such as `KW_REQUIRED` for logging errors if the key is missing. |

**Returns**

Returns the boolean value found at `path`. If the key does not exist, the `default_value` is returned. If `KW_EXTRACT` is set, the value is removed from `kw`.

**Notes**

Supports type conversion: integers and real numbers are treated as `TRUE` if nonzero, `FALSE` otherwise. Strings are compared case-insensitively against 'true' and 'false'.

---

(kw_get_dict)=
## `kw_get_dict()`

Retrieves the dictionary value from the `kw` JSON object at the specified `path`. If the key does not exist, it returns `default_value` or creates a new entry if `KW_CREATE` is set.

```C
json_t *kw_get_dict(
    hgobj      gobj,
    json_t     *kw,
    const char *path,
    json_t     *default_value,
    kw_flag_t  flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Handle to the calling object, used for logging errors. |
| `kw` | `json_t *` | JSON object to search within. |
| `path` | `const char *` | Path to the dictionary key, using the configured delimiter. |
| `default_value` | `json_t *` | Default value to return if the key is not found. |
| `flag` | `kw_flag_t` | Flags controlling behavior, such as `KW_CREATE` to create the key if missing. |

**Returns**

Returns the dictionary value at `path` if found. If `KW_CREATE` is set, it creates and returns a new dictionary. If the key is missing and `KW_REQUIRED` is set, an error is logged and `default_value` is returned.

**Notes**

If `KW_EXTRACT` is set, the retrieved value is removed from `kw`. The function logs an error if `path` does not exist and `KW_REQUIRED` is set.

---

(kw_get_dict_value)=
## `kw_get_dict_value()`

Retrieves a JSON value from the dictionary `kw` using the specified `path`. If the key does not exist, it returns `default_value` based on the provided `flag` options.

```C
json_t *kw_get_dict_value(
    hgobj      gobj,
    json_t     *kw,
    const char *path,
    json_t     *default_value,  // owned
    kw_flag_t  flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the calling object, used for logging and error reporting. |
| `kw` | `json_t *` | A JSON dictionary from which the value is retrieved. |
| `path` | `const char *` | The key path used to locate the value within `kw`. |
| `default_value` | `json_t *` | The default value to return if the key is not found. This value is owned by the caller. |
| `flag` | `kw_flag_t` | Flags that modify the behavior of the function, such as `KW_REQUIRED`, `KW_CREATE`, or `KW_EXTRACT`. |

**Returns**

Returns the JSON value found at `path`. If the key does not exist, it returns `default_value` based on the `flag` settings. If `KW_EXTRACT` is set, the value is removed from `kw`.

**Notes**

If `KW_REQUIRED` is set and the key is not found, an error is logged. If `KW_CREATE` is set, `default_value` is inserted into `kw` if the key does not exist.

---

(kw_get_int)=
## `kw_get_int()`

Retrieves an integer value from a JSON object by following a specified path. If the path does not exist, a default value is returned. Supports optional creation of the key if it does not exist.

```C
json_int_t kw_get_int(
    hgobj      gobj,
    json_t     *kw,
    const char *path,
    json_int_t default_value,
    kw_flag_t  flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Pointer to the GObj context, used for logging errors. |
| `kw` | `json_t *` | JSON object to search within. |
| `path` | `const char *` | Path to the desired integer value within the JSON object. |
| `default_value` | `json_int_t` | Default integer value to return if the path does not exist. |
| `flag` | `kw_flag_t` | Flags controlling behavior, such as `KW_REQUIRED` for error logging or `KW_CREATE` for creating the key if missing. |

**Returns**

Returns the integer value found at the specified path, or the `default_value` if the path does not exist or is not an integer.

**Notes**

If `KW_CREATE` is set and the path does not exist, a new key is created with the `default_value`. If `KW_REQUIRED` is set and the path is missing, an error is logged.

---

(kw_get_list)=
## `kw_get_list()`

Retrieves a JSON list from the dictionary `kw` at the specified `path`. If the key does not exist, it returns `default_value` or creates a new list if `KW_CREATE` is set in `flag`.

```C
json_t *kw_get_list(
    hgobj      gobj,
    json_t     *kw,
    const char *path,
    json_t     *default_value,
    kw_flag_t   flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Handle to the calling object, used for logging errors. |
| `kw` | `json_t *` | JSON dictionary to search for the list. |
| `path` | `const char *` | Path to the list within `kw`, using the configured delimiter. |
| `default_value` | `json_t *` | Default value to return if the key does not exist. Owned by the caller. |
| `flag` | `kw_flag_t` | Flags controlling behavior, such as `KW_CREATE` to create the list if missing. |

**Returns**

Returns a JSON list if found. If `KW_CREATE` is set and the key does not exist, a new list is created and returned. If the key is not found and `KW_REQUIRED` is set, an error is logged and `default_value` is returned.

**Notes**

If `KW_EXTRACT` is set in `flag`, the retrieved list is removed from `kw` and returned with an increased reference count. If the key exists but is not a list, an error is logged and `default_value` is returned.

---

(kw_get_list_value)=
## `kw_get_list_value()`

Retrieves the JSON value at the specified index from the JSON list `kw`. If the index is out of bounds and `KW_REQUIRED` is set, an error is logged. If `KW_EXTRACT` is set, the value is removed from the list.

```C
json_t *kw_get_list_value(
    hgobj     gobj,
    json_t    *kw,
    int        idx,
    kw_flag_t  flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Pointer to the gobj context, used for logging errors. |
| `kw` | `json_t *` | A JSON list from which the value is retrieved. |
| `idx` | `int` | The index of the value to retrieve. |
| `flag` | `kw_flag_t` | Flags controlling behavior, such as `KW_REQUIRED` for error logging and `KW_EXTRACT` for removing the value. |

**Returns**

Returns the JSON value at the specified index. If `KW_EXTRACT` is set, the value is removed from the list. Returns `NULL` if the index is out of bounds or `kw` is not a list.

**Notes**

If `kw` is not a JSON array, an error is logged. If `KW_REQUIRED` is set and the index is out of bounds, an error is also logged.

---

(kw_get_real)=
## `kw_get_real()`

Retrieves the real (floating-point) value associated with the given `path` in the JSON dictionary `kw`. If the key does not exist, the `default_value` is returned. Supports automatic type conversion for integers, booleans, and strings when `KW_WILD_NUMBER` is set.

```C
double kw_get_real(
    hgobj      gobj,
    json_t     *kw,
    const char *path,
    double     default_value,
    kw_flag_t  flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Handle to the GObj instance, used for logging errors. |
| `kw` | `json_t *` | JSON dictionary to search for the specified `path`. |
| `path` | `const char *` | Path to the desired real value within the JSON dictionary. |
| `default_value` | `double` | Value to return if the key does not exist or is of an incompatible type. |
| `flag` | `kw_flag_t` | Flags controlling behavior, such as `KW_REQUIRED`, `KW_CREATE`, and `KW_WILD_NUMBER`. |

**Returns**

Returns the real (floating-point) value found at `path`. If the key does not exist or is incompatible, `default_value` is returned.

**Notes**

If `KW_WILD_NUMBER` is set, the function attempts to convert integers, booleans, and numeric strings to a real value. If `KW_REQUIRED` is set and the key is missing, an error is logged.

---

(kw_get_str)=
## `kw_get_str()`

Retrieve a string value from a JSON dictionary at the specified `path`. If the key does not exist, return `default_value`.

```C
const char *kw_get_str(
    hgobj        gobj,
    json_t       *kw,
    const char   *path,
    const char   *default_value,
    kw_flag_t     flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Pointer to the GObj instance for logging and error handling. |
| `kw` | `json_t *` | JSON dictionary to search for the string value. |
| `path` | `const char *` | Path to the key in the JSON dictionary. |
| `default_value` | `const char *` | Default string value to return if the key is not found. |
| `flag` | `kw_flag_t` | Flags to control behavior, such as `KW_REQUIRED` for logging errors if the key is missing. |

**Returns**

Returns the string value associated with `path` in `kw`. If the key does not exist, returns `default_value`. If `KW_EXTRACT` is set, the key is removed from `kw`.

**Notes**

If the value at `path` is not a string, an error is logged, and `default_value` is returned. The function does not duplicate the returned string, so it should not be modified.

---

(kw_get_subdict_value)=
## `kw_get_subdict_value()`

Retrieve a value from a subdictionary within a JSON object. If the subdictionary does not exist, it can be created based on the provided flags. The function searches for the key within the subdictionary and returns the corresponding value.

```C
json_t *kw_get_subdict_value(
    hgobj      gobj,
    json_t    *kw,
    const char *path,
    const char *key,
    json_t    *jn_default_value,  // owned
    kw_flag_t  flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Pointer to the GObj (generic object) context. |
| `kw` | `json_t *` | JSON object containing the dictionary to search within. |
| `path` | `const char *` | Path to the subdictionary within the JSON object. |
| `key` | `const char *` | Key to retrieve from the subdictionary. |
| `jn_default_value` | `json_t *` | Default value to return if the key is not found. This value is owned by the caller. |
| `flag` | `kw_flag_t` | Flags controlling behavior, such as whether to create the subdictionary if it does not exist. |

**Returns**

Returns the JSON value associated with the specified key in the subdictionary. If the key is not found, the default value is returned. If `KW_CREATE` is set, the subdictionary is created if it does not exist.

**Notes**

If `KW_REQUIRED` is set and the key is not found, an error is logged. If `KW_EXTRACT` is set, the value is removed from the dictionary before returning.

---

(kw_has_key)=
## `kw_has_key()`

Checks if the dictionary `kw` contains the specified key `key`.

```C
BOOL kw_has_key(
    json_t *kw,
    const char *key
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `kw` | `json_t *` | The JSON dictionary to check. |
| `key` | `const char *` | The key to search for in the dictionary. |

**Returns**

Returns `TRUE` if the key exists in the dictionary, otherwise returns `FALSE`.

**Notes**

The function only works with JSON objects. If `kw` is not a dictionary, it returns `FALSE`.

---

(kw_has_word)=
## `kw_has_word()`

Checks if a given `word` exists within the JSON object `kw`. The word can be found in a string, list, or dictionary. Supports recursive search and verbosity options.

```C
BOOL kw_has_word(
    hgobj    gobj,
    json_t  *kw,   // NOT owned
    const char *word,
    kw_flag_t kw_flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Handle to the calling object, used for logging errors. |
| `kw` | `json_t *` | JSON object to search within. Must be a dictionary, list, or string. |
| `word` | `const char *` | The word to search for within `kw`. |
| `kw_flag` | `kw_flag_t` | Flags controlling search behavior, such as `KW_VERBOSE` for logging and `KW_REQUIRED` for strict checking. |

**Returns**

Returns `TRUE` if the `word` is found in `kw`, otherwise returns `FALSE`.

**Notes**

The function supports searching within JSON strings, lists, and dictionaries. If `kw_flag` includes `KW_VERBOSE`, errors will be logged when `kw` is not a valid type.

---

(kw_incref)=
## `kw_incref()`

Increments the reference count of the given JSON object `kw` and its associated binary fields if applicable.

```C
json_t *kw_incref(
    json_t *kw
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `kw` | `json_t *` | A pointer to the JSON object whose reference count should be incremented. |

**Returns**

Returns the same JSON object `kw` with an incremented reference count, or `NULL` if `kw` is invalid.

**Notes**

If `kw` contains binary fields registered with [`kw_add_binary_type()`](#kw_add_binary_type), their reference counts are also incremented.

---

(kw_match_simple)=
## `kw_match_simple()`

The function `kw_match_simple()` compares a JSON dictionary against a JSON filter, matching only string, integer, real, and boolean values.

```C
BOOL kw_match_simple(
    json_t *kw,         // NOT owned
    json_t *jn_filter   // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `kw` | `json_t *` | The JSON dictionary to be matched against the filter. This parameter is not owned by the function. |
| `jn_filter` | `json_t *` | The JSON filter used for comparison. This parameter is owned by the function and will be decremented. |

**Returns**

Returns `TRUE` if `kw` matches `jn_filter`, otherwise returns `FALSE`.

**Notes**

This function only compares simple JSON elements such as strings, integers, reals, and booleans. If `jn_filter` is `NULL`, the function returns `TRUE` by default. If `jn_filter` is an empty object, it also evaluates as `TRUE`.

---

(kw_pop)=
## `kw_pop()`

Removes all keys in `kw2` from the dictionary `kw1`. The `kw2` parameter can be a string, dictionary, or list.

```C
int kw_pop(
    json_t *kw1, // NOT owned
    json_t *kw2  // NOT owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `kw1` | `json_t *` | The dictionary from which keys will be removed. This parameter is not owned by the function. |
| `kw2` | `json_t *` | The keys to be removed from `kw1`. This can be a string, dictionary, or list. This parameter is not owned by the function. |

**Returns**

Returns 0 after removing the specified keys from `kw1`.

**Notes**

If `kw2` is a dictionary, all its keys are removed from `kw1`. If `kw2` is a list, all its elements are removed from `kw1`. If `kw2` is a string, the corresponding key is removed from `kw1`.

---

(kw_select)=
## `kw_select()`

`kw_select()` returns a new JSON list containing **duplicated** objects from `kw` that match the given `jn_filter`. If `match_fn` is `NULL`, [`kw_match_simple()`](#kw_match_simple) is used as the default matching function.

```C
json_t *kw_select(
    hgobj gobj,
    json_t *kw,         // NOT owned
    json_t *jn_filter,  // owned
    BOOL (*match_fn) (
        json_t *kw,         // NOT owned
        json_t *jn_filter   // owned
    )
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the gobj (generic object) context. |
| `kw` | `json_t *` | A JSON object or array to be filtered. It is **not owned** by the function. |
| `jn_filter` | `json_t *` | A JSON object containing the filter criteria. It is **owned** by the function. |
| `match_fn` | `BOOL (*)(json_t *, json_t *)` | A function pointer used to match elements in `kw` against `jn_filter`. If `NULL`, [`kw_match_simple()`](#kw_match_simple) is used. |

**Returns**

A new JSON array containing **duplicated** objects that match the filter criteria. The caller is responsible for freeing the returned JSON object.

**Notes**

If `kw` is an array, each element is checked against `jn_filter`, and matching elements are duplicated into the returned list.
If `kw` is an object, it is checked against `jn_filter`, and if it matches, it is duplicated into the returned list.
The returned JSON array contains **duplicated** objects, meaning they have new references and must be freed by the caller.

---

(kw_serialize)=
## `kw_serialize()`

`kw_serialize()` serializes specific fields in a JSON object by replacing binary fields with their serialized JSON representations.

```C
json_t *kw_serialize(
    hgobj     gobj,
    json_t   *kw
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the gobj instance, used for logging and context. |
| `kw` | `json_t *` | A JSON object containing fields to be serialized. |

**Returns**

Returns the same JSON object with specified binary fields replaced by their serialized JSON representations.

**Notes**

This function iterates over predefined binary fields and applies their corresponding serialization functions. If a field is not found or serialization fails, an error is logged.

---

(kw_set_dict_value)=
## `kw_set_dict_value()`

The function `kw_set_dict_value()` sets a value in a JSON dictionary at the specified path. If intermediate objects do not exist, they are created as dictionaries. Arrays are not created automatically.

```C
int kw_set_dict_value(
    hgobj      gobj,
    json_t    *kw,
    const char *path,   // The last word after delimiter (.) is the key
    json_t    *value    // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj context, used for logging and error handling. |
| `kw` | `json_t *` | A JSON dictionary where the value will be set. Must be a valid JSON object. |
| `path` | `const char *` | A dot-delimited path specifying where to set the value. The last segment is the key. |
| `value` | `json_t *` | The JSON value to set at the specified path. Ownership is transferred. |

**Returns**

Returns `0` on success, or `-1` if an error occurs (e.g., if `kw` is not a dictionary).

**Notes**

If the path does not exist, intermediate objects are created as dictionaries. Arrays are not created automatically. The function uses [`kw_find_path()`](<#kw_find_path>) internally to navigate the JSON structure.

---

(kw_set_path_delimiter)=
## `kw_set_path_delimiter()`

Sets the path delimiter used for key navigation in JSON structures. The default delimiter is '`'.

```C
char kw_set_path_delimiter(
    char delimiter
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `delimiter` | `char` | The new delimiter character to be used for path navigation. |

**Returns**

Returns the previous delimiter character before the change.

**Notes**

This function modifies a global setting, affecting all subsequent key path operations.

---

(kw_set_subdict_value)=
## `kw_set_subdict_value()`

The function `kw_set_subdict_value()` sets a key-value pair inside a subdictionary located at the specified path within a JSON object. If the subdictionary does not exist, it is created.

```C
int kw_set_subdict_value(
    hgobj      gobj,
    json_t     *kw,
    const char *path,
    const char *key,
    json_t     *value // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the gobj (generic object) that may be used for logging or context. |
| `kw` | `json_t *` | A JSON object where the subdictionary is located. |
| `path` | `const char *` | The path to the subdictionary within `kw`. The last segment of the path is the subdictionary name. |
| `key` | `const char *` | The key to be set inside the subdictionary. |
| `value` | `json_t *` | The JSON value to be assigned to `key`. Ownership is transferred to the function. |

**Returns**

Returns `0` on success, or `-1` if an error occurs (e.g., if `kw` is not a valid JSON object).

**Notes**

If the subdictionary at `path` does not exist, it is created as a new JSON object. The function uses [`kw_get_dict()`](#kw_get_dict) to retrieve or create the subdictionary.

---

(kw_update_except)=
## `kw_update_except()`

Updates the dictionary `kw` with key-value pairs from `other`, excluding keys specified in `except_keys`.

```C
void kw_update_except(
    hgobj      gobj,
    json_t    *kw,          // not owned
    json_t    *other,       // owned
    const char **except_keys
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Handle to the gobj (generic object) system. |
| `kw` | `json_t *` | The target JSON dictionary to be updated. This parameter is not owned by the function. |
| `other` | `json_t *` | The source JSON dictionary containing key-value pairs to update `kw`. This parameter is owned by the function. |
| `except_keys` | `const char **` | A NULL-terminated array of keys that should be excluded from the update. |

**Returns**

This function does not return a value.

**Notes**

Only the first level of `kw` is updated. Keys in `except_keys` are ignored during the update.

---

(kwid_compare_lists)=
## `kwid_compare_lists()`

Compare two JSON lists of records, allowing for unordered comparison. The function checks if both lists contain the same elements, considering optional metadata and private key exclusions.

```C
BOOL kwid_compare_lists(
    hgobj gobj,
    json_t *list,
    json_t *expected,
    const char **ignore_keys,
    BOOL without_metadata,
    BOOL without_private,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Pointer to the GObj context for logging and error handling. |
| `list` | `json_t *` | The first JSON list to compare. Not owned by the function. |
| `expected` | `json_t *` | The second JSON list to compare against. Not owned by the function. |
| `without_metadata` | `BOOL` | If TRUE, metadata keys (keys starting with '__') are ignored during comparison. |
| `without_private` | `BOOL` | If TRUE, private keys (keys starting with '_') are ignored during comparison. |
| `verbose` | `BOOL` | If TRUE, detailed error messages are logged when mismatches occur. |

**Returns**

Returns TRUE if both lists contain the same elements, considering the specified exclusions. Returns FALSE if they differ.

**Notes**

This function performs a deep comparison of JSON lists, allowing for unordered elements. It internally calls [`kwid_compare_records()`](#kwid_compare_records) for record-level comparison.

---

(kwid_compare_records)=
## `kwid_compare_records()`

Compares two JSON records deeply, allowing for unordered elements. The function checks for structural and value equivalence, optionally ignoring metadata and private fields.

```C
BOOL kwid_compare_records(
    hgobj gobj,
    json_t *record,
    json_t *expected,
    const char **ignore_keys,
    BOOL without_metadata,
    BOOL without_private,
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Handle to the GObj instance, used for logging errors. |
| `record` | `json_t *` | The JSON record to compare, not owned by the function. |
| `expected` | `json_t *` | The expected JSON record to compare against, not owned by the function. |
| `without_metadata` | `BOOL` | If TRUE, metadata fields (keys starting with '__') are ignored during comparison. |
| `without_private` | `BOOL` | If TRUE, private fields (keys starting with '_') are ignored during comparison. |
| `verbose` | `BOOL` | If TRUE, logs detailed error messages when mismatches occur. |

**Returns**

Returns TRUE if the records are identical, considering the specified filtering options. Returns FALSE if differences are found.

**Notes**

This function is useful for validating JSON records in databases or structured data comparisons. It internally calls [`kwid_compare_lists()`](#kwid_compare_lists) for array comparisons.

---

(kwid_get_ids)=
## `kwid_get_ids()`

Extracts and returns a new JSON list containing all unique IDs from the given JSON object, list, or string representation of IDs.

```C
json_t *kwid_get_ids(
    json_t *ids // not owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ids` | `json_t *` | A JSON object, list, or string containing IDs to be extracted. |

**Returns**

A new JSON list containing all extracted IDs as strings. Returns an empty list if no valid IDs are found.

**Notes**

The function supports extracting IDs from various JSON structures, including objects with ID keys, lists of ID strings, and mixed lists containing both strings and objects with 'id' fields.

---

(kwid_match_id)=
## `kwid_match_id()`

Checks if the given `id` exists within the JSON structure `ids`, which can be a string, list, or dictionary. The function returns `TRUE` if the `id` is found, otherwise `FALSE`.

```C
BOOL kwid_match_id(
    hgobj   gobj,
    json_t *ids,
    const char *id
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj instance, used for logging errors. |
| `ids` | `json_t *` | A JSON object, array, or string containing the possible matches for `id`. |
| `id` | `const char *` | The identifier to search for within `ids`. |

**Returns**

Returns `TRUE` if `id` is found in `ids`, otherwise returns `FALSE`.

**Notes**

The function supports different JSON structures:
- If `ids` is a string, it is directly compared to `id`.
- If `ids` is an array, it checks for `id` in string elements or in objects with an `id` field.
- If `ids` is a dictionary, it checks if `id` exists as a key.

---

(kwid_match_nid)=
## `kwid_match_nid()`

Checks if the given `id` with a limited size exists in the JSON list, dictionary, or string `ids`. The comparison is performed up to `max_id_size` characters.

```C
BOOL kwid_match_nid(
    hgobj    gobj,
    json_t  *ids,
    const char *id,
    int      max_id_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the GObj instance, used for logging errors. |
| `ids` | `json_t *` | A JSON object, array, or string containing the IDs to check against. |
| `id` | `const char *` | The ID to search for within `ids`. |
| `max_id_size` | `int` | The maximum number of characters to compare in the `id`. |

**Returns**

Returns `TRUE` if the `id` (up to `max_id_size` characters) is found in `ids`, otherwise returns `FALSE`.

**Notes**

If `ids` is an empty object or array, the function returns `TRUE`. The function supports searching in JSON objects, arrays, and strings.

---

(kwjr_get)=
## `kwjr_get()`

Retrieve the first record from a JSON list or dictionary that matches the given `id`. If the record is not found and `KW_CREATE` is set, a new record is created using `json_desc`. The function supports extracting the record from the list or dictionary if `KW_EXTRACT` is set.

```C
json_t *kwjr_get(
    hgobj gobj,
    json_t *kw,
    const char *id,
    json_t *new_record,
    const json_desc_t *json_desc,
    size_t *idx_,
    kw_flag_t flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | Pointer to the GObj context. |
| `kw` | `json_t *` | JSON object or array containing the records. Not owned by the caller. |
| `id` | `const char *` | The identifier of the record to retrieve. |
| `new_record` | `json_t *` | A new record to insert if `KW_CREATE` is set. Owned by the caller. |
| `json_desc` | `const json_desc_t *` | JSON descriptor defining the structure of the records. |
| `idx` | `size_t *` | Pointer to store the index of the found record in case of an array. Can be NULL. |
| `flag` | `kw_flag_t` | Flags controlling the behavior of the function, such as `KW_CREATE`, `KW_EXTRACT`, and `KW_BACKWARD`. |

**Returns**

Returns a pointer to the found or newly created JSON record. If `KW_EXTRACT` is set, the record is removed from `kw`. Returns NULL if the record is not found and `KW_CREATE` is not set.

**Notes**

If `kw` is a dictionary, the function searches for a key matching `id`. If `kw` is a list, it searches for a record where the `id` field matches. If `KW_CREATE` is set, a new record is created using `json_desc` and `new_record`. If `KW_EXTRACT` is set, the record is removed from `kw` and returned with an increased reference count.

---

(json_flatten_dict)=
## `json_flatten_dict()`

*Description pending — signature extracted from header.*

```C
json_t *json_flatten_dict(
    json_t *jn_nested
);
```

---

(json_unflatten_dict)=
## `json_unflatten_dict()`

*Description pending — signature extracted from header.*

```C
json_t *json_unflatten_dict(
    json_t *jn_flat
);
```

---

(kw_collect)=
## `kw_collect()`

*Description pending — signature extracted from header.*

```C
json_t *kw_collect(
    hgobj gobj,
    json_t *kw,
    json_t *jn_filter,
    BOOL (*match_fn) ( json_t *kw, json_t *jn_filter )
);
```

---

(kw_serialize_to_string)=
## `kw_serialize_to_string()`

*Description pending — signature extracted from header.*

```C
char *kw_serialize_to_string(
    hgobj gobj,
    json_t *kw
);
```

---

(kw_size)=
## `kw_size()`

*Description pending — signature extracted from header.*

```C
size_t kw_size(
    json_t *kw
);
```

---

(kw_walk)=
## `kw_walk()`

*Description pending — signature extracted from header.*

```C
int kw_walk(
    hgobj gobj,
    json_t *kw,
    int (*callback)(hgobj gobj, json_t *kw, const char *key, json_t *value)
);
```

---

(kwid_get)=
## `kwid_get()`

*Description pending — signature extracted from header.*

```C
json_t *kwid_get(
    hgobj gobj,
    json_t *kw,
    kw_flag_t flag,
    const char *path,
    ... ) JANSSON_ATTRS((format(printf, 4, 5))
);
```

---

(kwid_new_dict)=
## `kwid_new_dict()`

*Description pending — signature extracted from header.*

```C
json_t *kwid_new_dict(
    hgobj gobj,
    json_t *kw,
    kw_flag_t flag,
    const char *path,
    ... ) JANSSON_ATTRS((format(printf, 4, 5))
);
```

---

(kwid_new_list)=
## `kwid_new_list()`

*Description pending — signature extracted from header.*

```C
json_t *kwid_new_list(
    hgobj gobj,
    json_t *kw,
    kw_flag_t flag,
    const char *path,
    ... ) JANSSON_ATTRS((format(printf, 4, 5))
);
```

---

