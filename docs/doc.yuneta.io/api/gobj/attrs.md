# Attributes

Read and write gobj attributes. Attributes are typed fields declared through the GClass's SData schema and can be marked writable, persistent, statistic, etc.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)

(gobj_attr_desc)=
## `gobj_attr_desc()`

Retrieves the attribute description of a given gobj. If the attribute name is NULL, it returns the full attribute table of the gobj.

```C
const sdata_desc_t *gobj_attr_desc(
    hgobj gobj, 
    const char *attr, 
    BOOL verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The gobj whose attribute description is to be retrieved. |
| `attr` | `const char *` | The name of the attribute. If NULL, the function returns the full attribute table. |
| `verbose` | `BOOL` | If TRUE, logs an error message when the attribute is not found. |

**Returns**

A pointer to the attribute description (`sdata_desc_t *`). Returns NULL if the attribute is not found.

**Notes**

If `verbose` is set to TRUE and the attribute is not found, an error message is logged.

---

(gobj_attr_type)=
## `gobj_attr_type()`

Returns the data type of a given attribute in the specified `hgobj`.

```C
data_type_t gobj_attr_type(
    hgobj gobj,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose attribute type is being queried. |
| `name` | `const char *` | The name of the attribute whose type is to be retrieved. |

**Returns**

Returns the `data_type_t` of the specified attribute, or `0` if the attribute does not exist.

**Notes**

If the attribute does not exist, the function returns `0` without logging an error.

---

(gobj_has_attr)=
## `gobj_has_attr()`

Checks if the given `hgobj` has an attribute with the specified name.

```C
BOOL gobj_has_attr(
    hgobj hgobj,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `hgobj` | `hgobj` | The `hgobj` instance to check. |
| `name` | `const char *` | The name of the attribute to check. |

**Returns**

Returns `TRUE` if the attribute exists, otherwise returns `FALSE`.

**Notes**

This function performs a case-sensitive check for the attribute name.

---

(gobj_has_bottom_attr)=
## `gobj_has_bottom_attr()`

Checks if the given `hgobj` or any of its bottom objects has the specified attribute.

```C
BOOL gobj_has_bottom_attr(
    hgobj gobj_,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The object to check for the attribute. |
| `name` | `const char *` | The name of the attribute to search for. |

**Returns**

Returns `TRUE` if the attribute exists in the given `hgobj` or any of its bottom objects, otherwise returns `FALSE`.

**Notes**

This function traverses the bottom hierarchy of the given `hgobj` to check for the presence of the attribute.

---

(gobj_is_readable_attr)=
## `gobj_is_readable_attr()`

Checks if a given attribute of a `hgobj` is readable, meaning it has the `SDF_RD` flag set.

```C
BOOL gobj_is_readable_attr(
    hgobj gobj,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose attribute is being checked. |
| `name` | `const char *` | The name of the attribute to check. |

**Returns**

Returns `TRUE` if the attribute is readable (`SDF_RD` flag is set), otherwise returns `FALSE`.

**Notes**

If the attribute does not exist, the function returns `FALSE` and logs an error.

---

(gobj_is_writable_attr)=
## `gobj_is_writable_attr()`

Checks if a given attribute of a `hgobj` is writable based on its flags.

```C
BOOL gobj_is_writable_attr(
    hgobj gobj,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose attribute is being checked. |
| `name` | `const char *` | The name of the attribute to check. |

**Returns**

Returns `TRUE` if the attribute is writable, otherwise returns `FALSE`.

**Notes**

The function verifies if the attribute has the `SDF_WR` or `SDF_PERSIST` flag set.

---

(gobj_list_persistent_attrs)=
## `gobj_list_persistent_attrs()`

Retrieves a list of persistent attributes for a given `hgobj` or all services if `gobj` is `NULL`.

```C
json_t *gobj_list_persistent_attrs(
    hgobj gobj,
    json_t *jn_attrs  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` whose persistent attributes should be listed. If `NULL`, lists attributes for all services. |
| `jn_attrs` | `json_t *` | A JSON object specifying which attributes to list. Can be a string, a list of keys, or a dictionary. |

**Returns**

A JSON object containing the persistent attributes. The caller owns the returned JSON object and must free it.

**Notes**

If [`__global_list_persistent_attrs_fn__`](#__global_list_persistent_attrs_fn__) is not set, an empty JSON object is returned.

---

(gobj_read_attr)=
## `gobj_read_attr()`

Retrieves the value of a specified attribute from the given `hgobj`. The function returns a JSON object representing the attribute value.

```C
json_t *gobj_read_attr(
    hgobj gobj,
    const char *path,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which the attribute value is retrieved. |
| `name` | `const char *` | The name of the attribute to retrieve. |
| `src` | `hgobj` | The source `hgobj` requesting the attribute value. |

**Returns**

A JSON object containing the attribute value. If the attribute is not found, a warning is logged and NULL is returned.

**Notes**

If the attribute exists, the function returns a reference to the JSON object stored in the `hgobj`. The caller should not modify or free the returned JSON object.

---

(gobj_read_attrs)=
## `gobj_read_attrs()`

Retrieves a JSON object containing attributes of the given `hgobj` that match the specified flag criteria.

```C
json_t *gobj_read_attrs(
    hgobj gobj,
    sdata_flag_t include_flag,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose attributes are to be read. |
| `include_flag` | `sdata_flag_t` | A flag specifying which attributes to include in the result. |
| `src` | `hgobj` | The source `hgobj` requesting the attributes. |

**Returns**

A new JSON object containing the selected attributes. The caller is responsible for freeing the returned object.

**Notes**

This function filters attributes based on the provided `include_flag`. If `include_flag` is set to `-1`, all attributes are included.

---

(gobj_read_bool_attr)=
## `gobj_read_bool_attr()`

The function `gobj_read_bool_attr()` retrieves the boolean value of a specified attribute from a given `hgobj`. It traverses the object's hierarchy to find the attribute if it is not directly present in the object.

```C
BOOL gobj_read_bool_attr(
    hgobj gobj,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which the attribute is read. |
| `name` | `const char *` | The name of the attribute to retrieve. |

**Returns**

Returns `TRUE` if the attribute is set to a boolean true value, `FALSE` otherwise.

**Notes**

If the attribute is not found, a warning is logged, and `FALSE` is returned. If the object has a `mt_reading` method, it is invoked before returning the attribute value.

---

(gobj_read_integer_attr)=
## `gobj_read_integer_attr()`

Retrieves the integer value of a specified attribute from the given `hgobj`. The function supports attribute inheritance from bottom objects.

```C
json_int_t gobj_read_integer_attr(
    hgobj gobj,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The object from which the attribute value is retrieved. |
| `name` | `const char *` | The name of the attribute to retrieve. |

**Returns**

Returns the integer value of the specified attribute. If the attribute is not found, returns 0.

**Notes**

If the attribute is inherited from a bottom object, the function will traverse the hierarchy to retrieve the value. If the attribute is not found, a warning is logged.

---

(gobj_read_json_attr)=
## `gobj_read_json_attr()`

`gobj_read_json_attr()` retrieves the JSON attribute of a given GObj, including inherited attributes from bottom GObjs.

```C
json_t *gobj_read_json_attr(
    hgobj gobj, 
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj from which the attribute is read. |
| `name` | `const char *` | The name of the attribute to retrieve. |

**Returns**

Returns a pointer to the JSON attribute value. The returned JSON object is not owned by the caller and must not be modified or freed.

**Notes**

If the attribute is not found in `gobj`, the function searches in its bottom GObjs. If the attribute does not exist, a warning is logged.

---

(gobj_read_pointer_attr)=
## `gobj_read_pointer_attr()`

Retrieves the value of a pointer-type attribute from the given `hgobj`. The function searches for the attribute in the object's hierarchy, following inherited attributes if necessary.

```C
void *gobj_read_pointer_attr(
    hgobj gobj,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The object from which the attribute is read. |
| `name` | `const char *` | The name of the attribute to retrieve. |

**Returns**

Returns the pointer value of the specified attribute. If the attribute is not found, logs a warning and returns `NULL`.

**Notes**

If the attribute is inherited from a bottom object, [`gobj_read_pointer_attr()`](#gobj_read_pointer_attr) will traverse the hierarchy to find it.

---

(gobj_read_real_attr)=
## `gobj_read_real_attr()`

Retrieves the value of a real (floating-point) attribute from the given `hgobj`. The function searches for the attribute in the object's hierarchy, including inherited attributes from bottom objects.

```C
double gobj_read_real_attr(
    hgobj gobj,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which the attribute value is retrieved. |
| `name` | `const char *` | The name of the attribute to retrieve. |

**Returns**

Returns the floating-point value of the specified attribute. If the attribute is not found, logs a warning and returns 0.

**Notes**

If the attribute is found and the `hgobj` has a `mt_reading` method, that method is called before returning the value.

---

(gobj_read_str_attr)=
## `gobj_read_str_attr()`

Retrieves the string value of a specified attribute from the given `hgobj`, considering inherited attributes if applicable.

```C
const char *gobj_read_str_attr(
    hgobj gobj, 
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which the attribute value is retrieved. |
| `name` | `const char *` | The name of the attribute to retrieve. |

**Returns**

Returns a pointer to the string value of the specified attribute. If the attribute is not found, returns `NULL`.

**Notes**

If the attribute is inherited from a bottom `hgobj`, the function retrieves the value from the lowest level in the hierarchy.

---

(gobj_read_user_data)=
## `gobj_read_user_data()`

Retrieves user-defined data associated with the given `hgobj`. If a specific key is provided, it returns the corresponding value; otherwise, it returns the entire user data dictionary.

```C
json_t *gobj_read_user_data(
    hgobj gobj,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which to retrieve user data. |
| `name` | `const char *` | The key of the user data to retrieve. If NULL or empty, the entire user data dictionary is returned. |

**Returns**

A JSON object containing the requested user data. If `name` is specified, the corresponding value is returned. If `name` is NULL or empty, the entire user data dictionary is returned. Returns NULL if `gobj` is NULL.

**Notes**

The returned JSON object is not owned by the caller and should not be modified or freed.

---

(gobj_remove_persistent_attrs)=
## `gobj_remove_persistent_attrs()`

Removes persistent and writable attributes from a `hgobj`. If `jn_attrs` is empty, all attributes are removed.

```C
int gobj_remove_persistent_attrs(
    hgobj gobj, 
    json_t *jn_attrs  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which persistent attributes should be removed. |
| `jn_attrs` | `json_t *` | A JSON object specifying the attributes to remove. If empty, all attributes are removed. |

**Returns**

Returns `0` on success, or `-1` if the operation fails.

**Notes**

This function requires a global persistent attribute removal function to be set. If none is available, the function returns `-1`.

---

(gobj_reset_rstats_attrs)=
## `gobj_reset_rstats_attrs()`

Resets all attributes of the given `hgobj` that are marked with `SDF_RSTATS` to their default values.

```C
int gobj_reset_rstats_attrs(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` whose resettable statistics attributes should be reset. |

**Returns**

Returns `0` on success.

**Notes**

This function resets only attributes marked with `SDF_RSTATS`, leaving other attributes unchanged.

---

(gobj_reset_volatil_attrs)=
## `gobj_reset_volatil_attrs()`

Resets all attributes of the given `hgobj` that are marked as `SDF_VOLATIL` to their default values.

```C
int gobj_reset_volatil_attrs(
    hgobj gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` whose volatile attributes will be reset. |

**Returns**

Returns 0 on success.

**Notes**

This function resets only attributes marked with `SDF_VOLATIL`, leaving other attributes unchanged.

---

(gobj_save_persistent_attrs)=
## `gobj_save_persistent_attrs()`

The function `gobj_save_persistent_attrs()` saves the persistent attributes of a given `hgobj` object. It ensures that only named gobjs (services) can store persistent attributes.

```C
int gobj_save_persistent_attrs(
    hgobj gobj,
    json_t *jn_attrs  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` object whose persistent attributes are to be saved. |
| `jn_attrs` | `json_t *` | A JSON object containing the attributes to be saved. It can be a string, a list of keys, or a dictionary with the keys to be saved. |

**Returns**

Returns 0 on success, or -1 if the operation fails.

**Notes**

This function requires that the `hgobj` is a named gobj (service). If the global save function is not set, the function will return -1.

---

(gobj_write_attr)=
## `gobj_write_attr()`

The `gobj_write_attr` function writes a new value to a specified attribute of a given `hgobj`.

```C
int gobj_write_attr(
    hgobj gobj,
    const char *path,
    json_t *value,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The target `hgobj` whose attribute will be modified. |
| `path` | `const char *` | The attribute path to be modified. If it contains '`' characters, segments represent `hgobj` instances, and the leaf is the attribute. |
| `value` | `json_t *` | The new value to be assigned to the attribute. This parameter is owned and will be decremented. |
| `src` | `hgobj` | The source `hgobj` initiating the attribute modification. |

**Returns**

Returns 0 on success, or -1 if the attribute is not found or an error occurs.

**Notes**

If the attribute does not exist, an error is logged. The function ensures that the provided value is decremented after use.

---

(gobj_write_attrs)=
## `gobj_write_attrs()`

Writes multiple attributes of a `hgobj` object based on the provided JSON dictionary, applying the specified flag filter.

```C
int gobj_write_attrs(
    hgobj gobj,
    json_t *kw,
    sdata_flag_t include_flag,
    hgobj src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The target `hgobj` object whose attributes will be modified. |
| `kw` | `json_t *` | A JSON dictionary containing attribute names and their new values. The ownership of this object is transferred to the function. |
| `flag` | `sdata_flag_t` | A flag specifying which attributes should be updated. Only attributes matching this flag will be modified. |
| `src` | `hgobj` | The source `hgobj` object initiating the attribute modification. |

**Returns**

Returns 0 on success, or a negative value if an error occurs.

**Notes**

This function ensures that only attributes matching the specified flag are updated. The `kw` parameter is decremented after processing.

---

(gobj_write_bool_attr)=
## `gobj_write_bool_attr()`

Sets the boolean attribute of a `hgobj` instance to the specified value.

```C
int gobj_write_bool_attr(
    hgobj gobj, 
    const char *name, 
    BOOL value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose attribute is being modified. |
| `name` | `const char *` | The name of the attribute to modify. |
| `value` | `BOOL` | The boolean value to set for the attribute. |

**Returns**

Returns 0 on success, or -1 if the attribute is not found.

**Notes**

If the attribute is found, it is updated with the new boolean value. If the `hgobj` has a `mt_writing` method, it is called after updating the attribute.

---

(gobj_write_integer_attr)=
## `gobj_write_integer_attr()`

The function `gobj_write_integer_attr()` sets the value of an integer attribute in the given `hgobj` object. It updates the attribute if it exists and triggers the `mt_writing` method if defined.

```C
int gobj_write_integer_attr(
    hgobj gobj,   
    const char *name,   
    json_int_t value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` object whose attribute is being modified. |
| `name` | `const char *` | The name of the attribute to be updated. |
| `value` | `json_int_t` | The new integer value to be assigned to the attribute. |

**Returns**

Returns 0 on success, or -1 if the attribute is not found.

**Notes**

If the `mt_writing` method is defined in the object's gclass, it is called after updating the attribute.

---

(gobj_write_json_attr)=
## `gobj_write_json_attr()`

Writes a JSON value to the specified attribute of a `hgobj`. The function ensures that the attribute exists and is of the correct type before updating its value.

```C
int gobj_write_json_attr(
    hgobj gobj,
    const char *name,
    json_t *value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose attribute is being updated. |
| `name` | `const char *` | The name of the attribute to update. |
| `jn_value` | `json_t *` | The new JSON value to assign to the attribute. The function increases the reference count of this value. |

**Returns**

Returns 0 on success, or -1 if the attribute does not exist or an error occurs.

**Notes**

If the attribute does not exist, a warning is logged. The function does not check if the provided value matches the expected type of the attribute.

---

(gobj_write_new_json_attr)=
## `gobj_write_new_json_attr()`

Writes a new JSON value to the specified attribute of a `hgobj`. The provided JSON value is owned and will not be incremented in reference count.

```C
int gobj_write_new_json_attr(
    hgobj gobj,
    const char *name,
    json_t *value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance whose attribute will be modified. |
| `name` | `const char *` | The name of the attribute to be updated. |
| `jn_value` | `json_t *` | The new JSON value to be assigned to the attribute. This value is owned and will not be incremented in reference count. |

**Returns**

Returns 0 on success, or -1 if the attribute is not found.

**Notes**

This function does not increment the reference count of `jn_value`. Ensure that `jn_value` is not used elsewhere after calling [`gobj_write_new_json_attr()`](#gobj_write_new_json_attr).

---

(gobj_write_pointer_attr)=
## `gobj_write_pointer_attr()`

Sets the value of a pointer attribute in the given `hgobj` object.

```C
int gobj_write_pointer_attr(
    hgobj gobj,
    const char *name,
    void *value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` object whose attribute is being modified. |
| `name` | `const char *` | The name of the attribute to modify. |
| `value` | `void *` | The new pointer value to assign to the attribute. |

**Returns**

Returns 0 on success, or -1 if the attribute is not found.

**Notes**

If the attribute does not exist, an error is logged and -1 is returned.

---

(gobj_write_real_attr)=
## `gobj_write_real_attr()`

The function `gobj_write_real_attr()` sets the value of a real (floating-point) attribute in the given `hgobj` object. The attribute must exist and be of type `DTP_REAL`.

```C
int gobj_write_real_attr(
    hgobj gobj,
    const char *name,
    double value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the `hgobj` object whose attribute is being modified. |
| `name` | `const char *` | The name of the attribute to modify. It must be of type `DTP_REAL`. |
| `value` | `double` | The new floating-point value to assign to the attribute. |

**Returns**

Returns `0` on success, or `-1` if the attribute does not exist or is not of type `DTP_REAL`.

**Notes**

If the attribute exists and is writable, its value is updated. If the `hgobj` has a `mt_writing` method, it is called after updating the attribute.

---

(gobj_write_str_attr)=
## `gobj_write_str_attr()`

Sets the value of a string attribute in the given `hgobj` object. If the attribute does not exist, a warning is logged.

```C
int gobj_write_str_attr(
    hgobj gobj,
    const char *name,
    const char *value
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` object whose attribute is being modified. |
| `name` | `const char *` | The name of the attribute to modify. |
| `value` | `const char *` | The new string value to set. If `NULL`, the attribute is set to `json_null()`. |

**Returns**

Returns `0` on success, or `-1` if the attribute does not exist.

**Notes**

If the attribute does not exist in the `hgobj`, a warning is logged. If the `hgobj` has a `mt_writing` method, it is called after updating the attribute.

---

(gobj_write_strn_attr)=
## `gobj_write_strn_attr()`

Writes a string attribute to a `hgobj` object, ensuring the string is properly truncated to the specified length.

```C
int gobj_write_strn_attr(
    hgobj gobj_,
    const char *name,
    const char *s,
    size_t len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The target object where the attribute will be written. |
| `name` | `const char *` | The name of the attribute to be written. |
| `value` | `const char *` | The string value to be written to the attribute. |
| `len` | `size_t` | The maximum length of the string to be written. |

**Returns**

Returns 0 on success, or -1 if the attribute is not found or an error occurs.

**Notes**

If `value` is longer than `len`, it is truncated before being written. If `value` is NULL, the attribute is set to `json_null()`.

---

(gobj_write_user_data)=
## `gobj_write_user_data()`

Stores a JSON value in the user data dictionary of the given `hgobj` instance under the specified key.

```C
int gobj_write_user_data(
    hgobj gobj,
    const char *name,
    json_t *value // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance where the user data will be stored. |
| `name` | `const char *` | The key under which the JSON value will be stored. |
| `value` | `json_t *` | The JSON value to store. The function takes ownership of this value. |

**Returns**

Returns 0 on success, or -1 if an error occurs.

**Notes**

If `name` is empty, the entire user data dictionary is replaced with `value`.

---

(gobj_load_persistent_attrs)=
## `gobj_load_persistent_attrs()`

*Description pending — signature extracted from header.*

```C
int gobj_load_persistent_attrs(
    hgobj gobj,
    json_t *jn_attrs
);
```

---

