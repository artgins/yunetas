# Resource functions

The following APIs provide a standardized way for Gobjs to manage resources such as records or data objects. These functions delegate their operations to corresponding global methods (`mt_create_resource`, `mt_save_resource`, etc.), which must be implemented by the GClass of the target GObj.

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


```{toctree}
:caption: Publish/Subscription functions
:maxdepth: 2

gobj_create_resource.md
gobj_save_resource.md
gobj_delete_resource.md
gobj_list_resource.md
gobj_get_resource.md


```














## `gobj_create_resource`

### Function Signature
:::c
PUBLIC json_t *gobj_create_resource(
    hgobj gobj_,
    const char *resource,
    json_t *kw,  // owned
    json_t *jn_options // owned
);
:::

### Purpose
Creates a new resource and initializes it using the provided attributes and options.

### Workflow
1. Validates that the GObj is active and not destroyed.
2. Checks if the `mt_create_resource` method is implemented.
3. Invokes `mt_create_resource` to create the resource.

---

## `gobj_save_resource`

### Function Signature
:::c
PUBLIC int gobj_save_resource(
    hgobj gobj_,
    const char *resource,
    json_t *record,  // WARNING NOT owned
    json_t *jn_options // owned
);
:::

### Purpose
Saves changes to an existing resource.

### Workflow
1. Validates the GObj and ensures the `record` parameter is not `NULL`.
2. Checks if the `mt_save_resource` method is implemented.
3. Invokes `mt_save_resource` to save the resource.

---

## `gobj_delete_resource`

### Function Signature
:::c
PUBLIC int gobj_delete_resource(
    hgobj gobj_,
    const char *resource,
    json_t *record,  // owned
    json_t *jn_options // owned
);
:::

### Purpose
Deletes a specific resource.

### Workflow
1. Validates the GObj and ensures the `record` parameter is not `NULL`.
2. Checks if the `mt_delete_resource` method is implemented.
3. Invokes `mt_delete_resource` to delete the resource.

---

## `gobj_list_resource`

### Function Signature
:::c
PUBLIC json_t *gobj_list_resource(
    hgobj gobj_,
    const char *resource,
    json_t *jn_filter,  // owned
    json_t *jn_options // owned
);
:::

### Purpose
Lists all resources matching a given filter.

### Workflow
1. Validates the GObj.
2. Checks if the `mt_list_resource` method is implemented.
3. Invokes `mt_list_resource` to retrieve the resource list.

---

## `gobj_get_resource`

### Function Signature
:::c
PUBLIC json_t *gobj_get_resource(
    hgobj gobj_,
    const char *resource,
    json_t *jn_filter,  // owned
    json_t *jn_options // owned
);
:::

### Purpose
Retrieves a specific resource based on a filter.

### Workflow
1. Validates the GObj.
2. Checks if the `mt_get_resource` method is implemented.
3. Invokes `mt_get_resource` to retrieve the resource.

---

## Notes

- **Error Handling:**
  - If the required global method is not implemented or the GObj is invalid, the function logs an error and returns an appropriate failure result.

- **Memory Management:**
  - Input and output JSON ownership must be handled according to the API's specifications.

- **Modularity:**
  - These APIs provide a generic interface for resource management, while the actual logic is defined within the GClass, allowing for flexible and reusable implementations.

Let me know if additional refinements are needed!
