#!/usr/bin/python

from string import Template
import os

template = Template("""

<!-- ============================================================== -->
($_name_())=
# `$_name_()`
<!-- ============================================================== -->

$_description_

<!------------------------------------------------------------>
<!--                    Prototypes                          -->
<!------------------------------------------------------------>

``````{tab-set}

`````{tab-item} C

<!--====================================================-->
<!--                    Tab C                           -->
<!--====================================================-->

**Prototype**

```C
$_prototype_

```

**Parameters**

$_parameters_

---

**Return Value**

$_return_value_


<!--====================================================-->
<!--                    End Tab C                       -->
<!--====================================================-->

`````

`````{tab-item} JS

<!--====================================================-->
<!--                    Tab JS                          -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                JS Prototype                    -->
<!---------------------------------------------------->

**Prototype**

````JS
// Not applicable in JS
````

<!--====================================================-->
<!--                    EndTab JS                       -->
<!--====================================================-->

`````

`````{tab-item} Python

<!--====================================================-->
<!--                    Tab Python                      -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                Python Prototype                -->
<!---------------------------------------------------->

**Prototype**

````Python
# Not applicable in Python
````

<!--====================================================-->
<!--                    End Tab Python                   -->
<!--====================================================-->

`````

``````

<!------------------------------------------------------------>
<!--                    Examples                            -->
<!------------------------------------------------------------>

```````{dropdown} Examples

``````{tab-set}

`````{tab-item} C

<!--====================================================-->
<!--                    Tab C                           -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                C examples                      -->
<!---------------------------------------------------->

````C
// TODO C examples
````

<!--====================================================-->
<!--                    End Tab C                       -->
<!--====================================================-->

`````

`````{tab-item} JS

<!--====================================================-->
<!--                    Tab JS                          -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                JS examples                     -->
<!---------------------------------------------------->

````JS
// TODO JS examples
````

<!--====================================================-->
<!--                    EndTab JS                       -->
<!--====================================================-->

`````

`````{tab-item} Python

<!--====================================================-->
<!--                    Tab Python                      -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                Python examples                 -->
<!---------------------------------------------------->

````python
# TODO Python examples
````

<!--====================================================-->
<!--                    End Tab Python                  -->
<!--====================================================-->

`````

``````

```````

""")

functions_documentation = []
functions_documentation = [
    {
        "name": "kw_add_binary_type",
        "description": '''
Add a custom binary type to the system for handling serialization and deserialization with [`json_t *`](json_t) and [`gbuffer_t *`](gbuffer_t).
        ''',
        "prototype": '''
PUBLIC int kw_add_binary_type(
    const char  *type_name,
    int         (*serialize)(json_t *, gbuffer_t *),
    json_t      *(*deserialize)(gbuffer_t *)
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `type_name`
  - `const char *`
  - The name of the binary type to add.

* - `serialize`
  - `int (*)(json_t *, gbuffer_t *)`
  - A function pointer for serializing data of this type.

* - `deserialize`
  - `json_t *(*)(gbuffer_t *)`
  - A function pointer for deserializing data of this type.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    },
    {
        "name": "kw_serialize",
        "description": '''
Serialize a JSON object into a gbuffer for transmission or storage with [`json_t *`](json_t) and [`gbuffer_t *`](gbuffer_t).
        ''',
        "prototype": '''
PUBLIC gbuffer_t *kw_serialize(
    json_t      *kw
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to serialize.
:::
        ''',
        "return_value": '''
Returns a [`gbuffer_t *`](gbuffer_t) containing the serialized JSON data, or `NULL` on failure.
        '''
    },
    {
        "name": "kw_deserialize",
        "description": '''
Deserialize a gbuffer into a JSON object with [`json_t *`](json_t) and [`gbuffer_t *`](gbuffer_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_deserialize(
    gbuffer_t   *gbuffer
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gbuffer`
  - [`gbuffer_t *`](gbuffer_t)
  - The gbuffer containing the serialized JSON data to deserialize.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) created from the gbuffer, or `NULL` on failure.
        '''
    },
    {
        "name": "kw_incref",
        "description": '''
Increase the reference count of a JSON object with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC void kw_incref(
    json_t      *kw
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object whose reference count will be increased.
:::
        ''',
        "return_value": '''
No return value. This function modifies the reference count of the JSON object.
        '''
    },
    {
        "name": "kw_decref",
        "description": '''
Decrease the reference count of a JSON object and free it if the count reaches zero with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC void kw_decref(
    json_t      *kw
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object whose reference count will be decreased.
:::
        ''',
        "return_value": '''
No return value. This function modifies the reference count of the JSON object and may free it.
        '''
    },
    {
        "name": "kw_has_key",
        "description": '''
Check if a JSON object contains a specific key with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC BOOL kw_has_key(
    json_t      *kw,
    const char  *key
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to query.

* - `key`
  - `const char *`
  - The key to check for in the JSON object.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the key exists in the JSON object, otherwise returns `FALSE`.
        '''
    }
]

functions_documentation.extend([
    {
        "name": "kw_set_path_delimiter",
        "description": '''
Set the delimiter used for navigating JSON object paths.
        ''',
        "prototype": '''
PUBLIC void kw_set_path_delimiter(
    char delimiter
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `delimiter`
  - `char`
  - The character to use as the path delimiter.
:::
        ''',
        "return_value": '''
No return value. This function sets the global path delimiter.
        '''
    },
    {
        "name": "kw_find_path",
        "description": '''
Find a value in a JSON object using a specified path. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_find_path(
    json_t      *kw,
    const char  *path,
    BOOL        create
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to search.

* - `path`
  - `const char *`
  - The path to search for in the JSON object.

* - `create`
  - `BOOL`
  - If `TRUE`, creates the path if it does not exist.
:::
        ''',
        "return_value": '''
Returns a pointer to the value at the specified path, or `NULL` if not found or creation failed.
        '''
    },
    {
        "name": "kw_set_subdict_value",
        "description": '''
Set a value in a JSON sub-dictionary at a specified path. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int kw_set_subdict_value(
    json_t      *kw,
    const char  *path,
    const char  *value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to modify.

* - `path`
  - `const char *`
  - The path where the value will be set.

* - `value`
  - `const char *`
  - The value to set in the sub-dictionary.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "kw_delete",
        "description": '''
Delete a key from a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int kw_delete(
    json_t      *kw,
    const char  *key
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to modify.

* - `key`
  - `const char *`
  - The key to delete from the JSON object.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value if the key does not exist or deletion fails.
        '''
    },
    {
        "name": "kw_delete_subkey",
        "description": '''
Delete a subkey from a JSON object at a specified path. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int kw_delete_subkey(
    json_t      *kw,
    const char  *path,
    const char  *key
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to modify.

* - `path`
  - `const char *`
  - The path to the subkey's parent object.

* - `key`
  - `const char *`
  - The subkey to delete from the parent object.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value if the subkey does not exist or deletion fails.
        '''
    },
    {
        "name": "kw_find_str_in_list",
        "description": '''
Check if a specified string exists in a JSON array. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC BOOL kw_find_str_in_list(
    json_t      *kw,
    const char  *string
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON array to search.

* - `string`
  - `const char *`
  - The string to search for in the array.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the string exists in the array, otherwise returns `FALSE`.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "kw_find_json_in_list",
        "description": '''
Search for a JSON object in a JSON array. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_find_json_in_list(
    json_t      *kw,
    const char  *key,
    const char  *value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON array to search.

* - `key`
  - `const char *`
  - The key to match in the objects within the array.

* - `value`
  - `const char *`
  - The value to match for the specified key.
:::
        ''',
        "return_value": '''
Returns a pointer to the first JSON object in the array that matches the key-value pair, or `NULL` if not found.
        '''
    },
    {
        "name": "kwid_compare_records",
        "description": '''
Compare two JSON objects (records) based on their `kwid` fields. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int kwid_compare_records(
    json_t      *record1,
    json_t      *record2
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `record1`
  - [`json_t *`](json_t)
  - The first JSON object to compare.

* - `record2`
  - [`json_t *`](json_t)
  - The second JSON object to compare.
:::
        ''',
        "return_value": '''
Returns `0` if the `kwid` fields are equal, a positive value if `record1` is greater, or a negative value if `record2` is greater.
        '''
    },
    {
        "name": "kwid_compare_lists",
        "description": '''
Compare two JSON arrays (lists) of objects based on their `kwid` fields. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int kwid_compare_lists(
    json_t      *list1,
    json_t      *list2
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `list1`
  - [`json_t *`](json_t)
  - The first JSON array to compare.

* - `list2`
  - [`json_t *`](json_t)
  - The second JSON array to compare.
:::
        ''',
        "return_value": '''
Returns `0` if the lists are equal, a positive value if `list1` is greater, or a negative value if `list2` is greater.
        '''
    },
    {
        "name": "kw_get_dict",
        "description": '''
Get the value of a key from a JSON object as a dictionary. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_get_dict(
    json_t      *kw,
    const char  *key,
    BOOL        create
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to query.

* - `key`
  - `const char *`
  - The key whose value will be retrieved as a dictionary.

* - `create`
  - `BOOL`
  - If `TRUE`, creates an empty dictionary for the key if it does not exist.
:::
        ''',
        "return_value": '''
Returns a pointer to the dictionary, or `NULL` if the key does not exist and `create` is `FALSE`.
        '''
    },
    {
        "name": "kw_get_list",
        "description": '''
Get the value of a key from a JSON object as a list. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_get_list(
    json_t      *kw,
    const char  *key,
    BOOL        create
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to query.

* - `key`
  - `const char *`
  - The key whose value will be retrieved as a list.

* - `create`
  - `BOOL`
  - If `TRUE`, creates an empty list for the key if it does not exist.
:::
        ''',
        "return_value": '''
Returns a pointer to the list, or `NULL` if the key does not exist and `create` is `FALSE`.
        '''
    },
    {
        "name": "kw_get_list_value",
        "description": '''
Get the value at a specified index in a JSON array (list). Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_get_list_value(
    json_t      *kw,
    size_t      index
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON array to query.

* - `index`
  - `size_t`
  - The index of the value to retrieve.
:::
        ''',
        "return_value": '''
Returns a pointer to the value at the specified index, or `NULL` if the index is out of bounds.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "kw_get_int",
        "description": '''
Get the value of a key as an integer from a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int kw_get_int(
    json_t      *kw,
    const char  *key,
    int         default_value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to query.

* - `key`
  - `const char *`
  - The key whose value will be retrieved as an integer.

* - `default_value`
  - `int`
  - The default value to return if the key does not exist or is not an integer.
:::
        ''',
        "return_value": '''
Returns the integer value of the key, or `default_value` if the key does not exist or is not an integer.
        '''
    },
    {
        "name": "kw_get_real",
        "description": '''
Get the value of a key as a real (floating-point) number from a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC double kw_get_real(
    json_t      *kw,
    const char  *key,
    double      default_value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to query.

* - `key`
  - `const char *`
  - The key whose value will be retrieved as a real number.

* - `default_value`
  - `double`
  - The default value to return if the key does not exist or is not a real number.
:::
        ''',
        "return_value": '''
Returns the real value of the key, or `default_value` if the key does not exist or is not a real number.
        '''
    },
    {
        "name": "kw_get_bool",
        "description": '''
Get the value of a key as a boolean from a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC BOOL kw_get_bool(
    json_t      *kw,
    const char  *key,
    BOOL        default_value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to query.

* - `key`
  - `const char *`
  - The key whose value will be retrieved as a boolean.

* - `default_value`
  - `BOOL`
  - The default value to return if the key does not exist or is not a boolean.
:::
        ''',
        "return_value": '''
Returns the boolean value of the key, or `default_value` if the key does not exist or is not a boolean.
        '''
    },
    {
        "name": "kw_get_str",
        "description": '''
Get the value of a key as a string from a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC const char *kw_get_str(
    json_t      *kw,
    const char  *key,
    const char  *default_value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to query.

* - `key`
  - `const char *`
  - The key whose value will be retrieved as a string.

* - `default_value`
  - `const char *`
  - The default value to return if the key does not exist or is not a string.
:::
        ''',
        "return_value": '''
Returns the string value of the key, or `default_value` if the key does not exist or is not a string.
        '''
    },
    {
        "name": "kw_get_dict_value",
        "description": '''
Get the value of a key from a sub-dictionary in a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_get_dict_value(
    json_t      *kw,
    const char  *key,
    const char  *subkey
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to query.

* - `key`
  - `const char *`
  - The key whose value is a sub-dictionary.

* - `subkey`
  - `const char *`
  - The key to retrieve from the sub-dictionary.
:::
        ''',
        "return_value": '''
Returns the value of `subkey` in the sub-dictionary, or `NULL` if the key or subkey does not exist.
        '''
    },
    {
        "name": "kw_get_subdict_value",
        "description": '''
Get the value of a sub-dictionary's key from a JSON object at a specified path. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC const char *kw_get_subdict_value(
    json_t      *kw,
    const char  *path,
    const char  *key,
    const char  *default_value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to query.

* - `path`
  - `const char *`
  - The path to the sub-dictionary.

* - `key`
  - `const char *`
  - The key in the sub-dictionary to retrieve.

* - `default_value`
  - `const char *`
  - The default value to return if the key or subkey does not exist.
:::
        ''',
        "return_value": '''
Returns the string value of the key in the sub-dictionary, or `default_value` if not found.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "kw_update_except",
        "description": '''
Update a JSON object with the contents of another JSON object, excluding specified keys. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int kw_update_except(
    json_t      *destination,
    json_t      *source,
    const char  **keys_to_exclude
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `destination`
  - [`json_t *`](json_t)
  - The JSON object to update.

* - `source`
  - [`json_t *`](json_t)
  - The JSON object whose contents will be merged into the destination.

* - `keys_to_exclude`
  - `const char **`
  - A null-terminated array of keys to exclude from the update.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    },
    {
        "name": "kw_duplicate",
        "description": '''
Create a deep copy of a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_duplicate(
    json_t      *kw
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to duplicate.
:::
        ''',
        "return_value": '''
Returns a new [`json_t *`](json_t) object that is a deep copy of the input, or `NULL` on failure.
        '''
    },
    {
        "name": "kw_clone_by_path",
        "description": '''
Create a clone of a JSON object by including only values from a specified path. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_clone_by_path(
    json_t      *kw,
    const char  *path
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to clone.

* - `path`
  - `const char *`
  - The path specifying which parts of the object to include in the clone.
:::
        ''',
        "return_value": '''
Returns a new [`json_t *`](json_t) object containing only the values from the specified path, or `NULL` on failure.
        '''
    },
    {
        "name": "kw_clone_by_keys",
        "description": '''
Create a clone of a JSON object by including only specified keys. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_clone_by_keys(
    json_t      *kw,
    const char  **keys
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to clone.

* - `keys`
  - `const char **`
  - A null-terminated array of keys to include in the clone.
:::
        ''',
        "return_value": '''
Returns a new [`json_t *`](json_t) object containing only the specified keys, or `NULL` on failure.
        '''
    },
    {
        "name": "kw_clone_by_not_keys",
        "description": '''
Create a clone of a JSON object by excluding specified keys. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_clone_by_not_keys(
    json_t      *kw,
    const char  **keys_to_exclude
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to clone.

* - `keys_to_exclude`
  - `const char **`
  - A null-terminated array of keys to exclude from the clone.
:::
        ''',
        "return_value": '''
Returns a new [`json_t *`](json_t) object excluding the specified keys, or `NULL` on failure.
        '''
    },
    {
        "name": "kw_pop",
        "description": '''
Remove a key from a JSON object and return its value. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_pop(
    json_t      *kw,
    const char  *key
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to modify.

* - `key`
  - `const char *`
  - The key to remove from the JSON object.
:::
        ''',
        "return_value": '''
Returns the value associated with the removed key, or `NULL` if the key does not exist.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "kw_match_simple",
        "description": '''
Check if a JSON object matches a simple key-value pair condition. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC BOOL kw_match_simple(
    json_t      *kw,
    const char  *key,
    const char  *value
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to check.

* - `key`
  - `const char *`
  - The key to match in the JSON object.

* - `value`
  - `const char *`
  - The value to match for the specified key.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the key-value pair matches in the JSON object, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "kw_delete_private_keys",
        "description": '''
Remove all private keys (keys starting with "_") from a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int kw_delete_private_keys(
    json_t      *kw
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object from which private keys will be removed.
:::
        ''',
        "return_value": '''
Returns the number of private keys removed, or `0` if none were found.
        '''
    },
    {
        "name": "kw_delete_metadata_keys",
        "description": '''
Remove all metadata keys (keys starting with "__") from a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int kw_delete_metadata_keys(
    json_t      *kw
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object from which metadata keys will be removed.
:::
        ''',
        "return_value": '''
Returns the number of metadata keys removed, or `0` if none were found.
        '''
    },
    {
        "name": "kw_collapse",
        "description": '''
Collapse a JSON object, converting child objects and arrays into string representations. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_collapse(
    json_t      *kw
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to collapse.
:::
        ''',
        "return_value": '''
Returns a new [`json_t *`](json_t) object with collapsed child objects and arrays, or `NULL` on failure.
        '''
    },
    {
        "name": "kw_filter_private",
        "description": '''
Create a new JSON object by filtering out private keys (keys starting with "_"). Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_filter_private(
    json_t      *kw
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to filter.
:::
        ''',
        "return_value": '''
Returns a new [`json_t *`](json_t) object without private keys, or `NULL` on failure.
        '''
    },
    {
        "name": "kw_filter_metadata",
        "description": '''
Create a new JSON object by filtering out metadata keys (keys starting with "__"). Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kw_filter_metadata(
    json_t      *kw
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to filter.
:::
        ''',
        "return_value": '''
Returns a new [`json_t *`](json_t) object without metadata keys, or `NULL` on failure.
        '''
    }
])

functions_documentation.extend([
    {
        "name": "kwjr_get",
        "description": '''
Retrieve a JSON value from a JSON object or array based on a JSON path. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kwjr_get(
    json_t      *kw,
    const char  *path
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object or array to query.

* - `path`
  - `const char *`
  - The JSON path used to retrieve the value.
:::
        ''',
        "return_value": '''
Returns a pointer to the value at the specified path, or `NULL` if the path does not exist.
        '''
    },
    {
        "name": "kwid_get_ids",
        "description": '''
Retrieve the list of IDs from a JSON object containing `kwid` keys. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC json_t *kwid_get_ids(
    json_t      *kw
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object containing `kwid` keys.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) array containing the IDs, or `NULL` on failure.
        '''
    },
    {
        "name": "kw_has_word",
        "description": '''
Check if a word exists in a space-separated string within a JSON object. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC BOOL kw_has_word(
    json_t      *kw,
    const char  *key,
    const char  *word
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `kw`
  - [`json_t *`](json_t)
  - The JSON object to query.

* - `key`
  - `const char *`
  - The key containing the space-separated string.

* - `word`
  - `const char *`
  - The word to search for in the string.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the word exists in the string, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "kwid_match_id",
        "description": '''
Check if a JSON object matches a specific `kwid`. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC BOOL kwid_match_id(
    json_t      *record,
    const char  *kwid
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `record`
  - [`json_t *`](json_t)
  - The JSON object to check.

* - `kwid`
  - `const char *`
  - The `kwid` to match against the record.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the `kwid` matches the `kwid` in the JSON object, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "kwid_match_nid",
        "description": '''
Check if a JSON object matches a specific `nid` (Node ID). Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC BOOL kwid_match_nid(
    json_t      *record,
    const char  *nid
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `record`
  - [`json_t *`](json_t)
  - The JSON object to check.

* - `nid`
  - `const char *`
  - The `nid` to match against the record.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the `nid` matches the `nid` in the JSON object, otherwise returns `FALSE`.
        '''
    }
])


# Loop through the list of names and create a file for each
for fn in functions_documentation:
    # Substitute the variable in the template

    formatted_text = template.substitute(
        _name_          = fn['name'],
        _description_   = fn['description'],
        _prototype_     = fn['prototype'],
        _parameters_    = fn['parameters'],
        _return_value_  = fn['return_value']
    )
    # Create a unique file name for each name
    file_name = f"{fn['name'].lower()}.md"

    # Check if the file already exists
    if os.path.exists(file_name):
        print(f"File {file_name} already exists. =============================> Skipping...")
        continue  # Skip to the next name

    # Write the formatted text to the file
    with open(file_name, "w") as file:
        file.write(formatted_text)

    print(f"File created: {file_name}")
