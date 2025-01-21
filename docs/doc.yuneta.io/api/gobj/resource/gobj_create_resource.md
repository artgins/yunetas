(gobj_create_resource())=
# `gobj_create_resource`


## Function Signature
```c
PUBLIC json_t *gobj_create_resource(
    hgobj       gobj,
    const char  *resource,
    json_t      *kw,  // owned
    json_t      *jn_options // owned
);
```

## Description
The `gobj_create_resource` function creates a new resource associated with a GObj. The resource is initialized using the provided attributes (`kw`) and options (`jn_options`). This function delegates its operation to the `mt_create_resource` method of the GObj's GClass.

If the `mt_create_resource` method is not implemented in the GClass, the function logs an error and returns `NULL`.

## Parameters
::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - **Parameter**
  - **Type**
  - **Description**
* - `gobj_`
  - `hgobj`
  - The GObj instance where the resource will be created.
* - `resource`
  - `const char *`
  - The name of the resource to create.
* - `kw`
  - `json_t *`
  - JSON object containing the attributes for the resource (owned).
* - `jn_options`
  - `json_t *`
  - Additional options for creating the resource (owned).
:::

## Return Value
- Returns a JSON object representing the created resource (not owned by the caller).
- If the GObj is invalid or the `mt_create_resource` method is not defined, the function logs an error and returns `NULL`.

## Notes
- **Ownership:**
  - The `kw` and `jn_options` parameters are owned by the function and will be decremented internally.
  - The returned JSON object is not owned by the caller and must not be decremented.
- **Error Handling:**
  - If the GObj is `NULL` or destroyed, the function logs an error and returns `NULL`.
  - If the `mt_create_resource` method is missing, the function logs an error and returns `NULL`.
