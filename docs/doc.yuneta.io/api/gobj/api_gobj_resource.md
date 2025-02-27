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


```{toctree}
:caption: Resource Functions
:maxdepth: 1

resource/gobj_create_resource
resource/gobj_delete_resource
resource/gobj_get_resource
resource/gobj_list_resource
resource/gobj_save_resource


```
