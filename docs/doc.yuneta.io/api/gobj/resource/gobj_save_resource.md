(gobj_save_resource())=
# `gobj_save_resource`

## Function Signature
```c
PUBLIC int gobj_save_resource(
    hgobj       gobj_,
    const char  *resource,
    json_t      *record,  // WARNING NOT owned
    json_t      *jn_options // owned
);
```

## Description
The `gobj_save_resource` function saves changes to an existing resource associated with a GObj. The resource to be saved is specified via the `record` parameter, and additional options can be provided using `jn_options`. This function delegates its operation to the `mt_save_resource` method of the GObj's GClass.

If the `mt_save_resource` method is not implemented in the GClass, the function logs an error and returns `-1`.

## Parameters
::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - **Parameter**
  - **Type**
  - **Description**
* - `gobj_`
  - `hgobj`
  - The GObj instance associated with the resource.
* - `resource`
  - `const char *`
  - The name of the resource to save.
* - `record`
  - `json_t *`
  - JSON object representing the resource's data (not owned).
* - `jn_options`
  - `json_t *`
  - Additional options for saving the resource (owned).
:::

## Return Value
- Returns `0` on success.
- Returns `-1` if the GObj is invalid, the `record` parameter is `NULL`, or the `mt_save_resource` method is not implemented.

## Notes
- **Ownership:**
  - The `record` parameter is not owned by the function and must be managed by the caller.
  - The `jn_options` parameter is owned by the function and will be decremented internally.
- **Error Handling:**
  - If the GObj is `NULL` or destroyed, the function logs an error and returns `-1`.
  - If the `mt_save_resource` method is missing, the function logs an error and returns `-1`.
