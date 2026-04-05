# Resource Functions

The following APIs provide a standardized way for Gobjs to manage resources such as records or data objects. These functions delegate their operations to corresponding global methods (`mt_create_resource`, `mt_save_resource`, etc.), which must be implemented by the GClass of the target GObj.

Source code in:
- [gobj.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)
- [gobj.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)

---

## How These APIs Work

1. **Delegation to GClass Methods:**
    - Each function invokes a specific method in the GClass:
        - `gobj_create_resource` → `mt_create_resource`
        - `gobj_save_resource` → `mt_save_resource`
        - `gobj_delete_resource` → `mt_delete_resource`
        - `gobj_list_resource` → `mt_list_resource`
        - `gobj_get_resource` → `mt_get_resource`

2. **Requirement for Implementation:**
    - The GObj must belong to a GClass that implements the respective methods. If the methods are not defined, an error is logged, and the function returns a failure.

3. **Validation:**
    - The APIs validate the state of the GObj to ensure it is not `NULL` or destroyed (`obflag_destroyed`). If validation fails, an error is logged.

4. **Input and Output Handling:**
    - JSON objects passed as parameters are either owned or not owned by the caller, depending on the API. This ownership must be managed correctly to avoid memory issues.

(gobj_create_resource)=
## `gobj_create_resource()`

The function `gobj_create_resource()` creates a new resource in the specified GObj, using the provided JSON parameters and options.

```C
json_t *gobj_create_resource(
    hgobj gobj,
    const char *resource,
    json_t *kw,
    json_t *jn_options
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj in which the resource will be created. |
| `resource` | `const char *` | The name of the resource to be created. |
| `kw` | `json_t *` | A JSON object containing the attributes and data for the new resource. This parameter is owned by the function. |
| `jn_options` | `json_t *` | A JSON object containing additional options for resource creation. This parameter is owned by the function. |

**Returns**

Returns a JSON object representing the created resource. If the operation fails, returns NULL.

**Notes**

The function requires that the GObj has the `mt_create_resource` method implemented. If the method is not defined, an error is logged, and the function returns NULL.

---

(gobj_delete_resource)=
## `gobj_delete_resource()`

`gobj_delete_resource()` deletes a resource associated with a given `hgobj`.

```C
int gobj_delete_resource(
    hgobj gobj,
    const char *resource,
    json_t *record,
    json_t *jn_options
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance managing the resource. |
| `resource` | `const char *` | The name of the resource to be deleted. |
| `record` | `json_t *` | A JSON object containing the record to be deleted. This parameter is owned and must be freed by the function. |
| `jn_options` | `json_t *` | A JSON object containing additional options for deletion. This parameter is owned and must be freed by the function. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

If `gobj` is `NULL` or destroyed, an error is logged and `-1` is returned.
If `record` is `NULL`, an error is logged and `-1` is returned.
If the `gobj`'s class does not implement `mt_delete_resource`, an error is logged and `-1` is returned.
The function ensures that `record` and `jn_options` are properly freed before returning.

---

(gobj_get_resource)=
## `gobj_get_resource()`

Retrieves a resource from the specified `hgobj` object, using the given resource name and filter criteria.

```C
json_t *gobj_get_resource(
    hgobj gobj,
    const char *resource,
    json_t *jn_filter,
    json_t *jn_options
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which the resource is retrieved. |
| `resource` | `const char *` | The name of the resource to retrieve. |
| `jn_filter` | `json_t *` | A JSON object containing filter criteria for selecting the resource. Owned by the caller. |
| `jn_options` | `json_t *` | A JSON object containing additional options for retrieving the resource. Owned by the caller. |

**Returns**

A JSON object representing the requested resource. The returned object is not owned by the caller and should not be modified or freed.

**Notes**

If the `gobj` is `NULL` or destroyed, an error is logged and `NULL` is returned. If the `mt_get_resource` method is not defined in the `hgobj`'s gclass, an error is logged and `NULL` is returned.

---

(gobj_list_resource)=
## `gobj_list_resource()`

`gobj_list_resource()` retrieves a list of resources from the specified `hgobj` object, applying optional filtering and additional options.

```C
json_t *gobj_list_resource(
    hgobj gobj,
    const char *resource,
    json_t *jn_filter,  // owned
    json_t *jn_options  // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance from which to retrieve the resource list. |
| `resource` | `const char *` | The name of the resource to be listed. |
| `jn_filter` | `json_t *` | A JSON object containing filtering criteria. Owned by the caller. |
| `jn_options` | `json_t *` | A JSON object containing additional options for listing resources. Owned by the caller. |

**Returns**

Returns a JSON object containing the list of resources. The caller is responsible for managing the returned JSON object.

**Notes**

If the `gobj` does not implement `mt_list_resource`, an error is logged and `NULL` is returned.

---

(gobj_save_resource)=
## `gobj_save_resource()`

Saves a resource associated with the given `gobj`. The function updates the specified resource with the provided `record` data and applies any additional options from `jn_options`.

```C
int gobj_save_resource(
    hgobj gobj,
    const char *resource,
    json_t *record,
    json_t *jn_options
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance managing the resource. |
| `resource` | `const char *` | The name of the resource to be saved. |
| `record` | `json_t *` | The JSON object containing the data to be saved. This parameter is not owned by the function. |
| `jn_options` | `json_t *` | Additional options for saving the resource. This parameter is owned by the function. |

**Returns**

Returns 0 on success, or -1 if an error occurs.

**Notes**

If the GObj does not implement the `mt_save_resource` method, an error is logged and the function returns -1.

---
