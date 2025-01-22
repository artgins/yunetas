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


**Notes**

$_notes_


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

functions = [
    {
        "name": "gobj_create_resource",
        "description": '''
The `gobj_create_resource` function creates a new resource associated with a GObj. The resource is initialized using the provided attributes (`kw`) and options (`jn_options`). This function delegates its operation to the `mt_create_resource` method of the GObj's GClass.

If the `mt_create_resource` method is not implemented in the GClass, the function logs an error and returns `NULL`.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_create_resource(
    hgobj       gobj,
    const char  *resource,
    json_t      *kw,  // owned
    json_t      *jn_options // owned
);
        ''',
        "parameters": '''
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

        ''',
        "return_value": '''
- Returns a JSON object representing the created resource (not owned by the caller).
- If the GObj is invalid or the `mt_create_resource` method is not defined, the function logs an error and returns `NULL`.
        ''',
        "notes": '''
- **Ownership:**
  - The `kw` and `jn_options` parameters are owned by the function and will be decremented internally.
  - The returned JSON object is not owned by the caller and must not be decremented.
- **Error Handling:**
  - If the GObj is `NULL` or destroyed, the function logs an error and returns `NULL`.
  - If the `mt_create_resource` method is missing, the function logs an error and returns `NULL`.
        '''
    },

    {
        "name": "gobj_delete_resource",
        "description": '''
The `gobj_delete_resource` function deletes a specific resource associated with a GObj. The resource to be deleted is identified using the `record` parameter, and additional options can be provided via `jn_options`. This function delegates its operation to the `mt_delete_resource` method of the GObj's GClass.

If the `mt_delete_resource` method is not implemented in the GClass, the function logs an error and returns `-1`.

        ''',
        "prototype": '''
PUBLIC int gobj_delete_resource(
    hgobj       gobj_,
    const char  *resource,
    json_t      *record,  // owned
    json_t      *jn_options // owned
);
        ''',
        "parameters": '''
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
  - The name of the resource to delete.

* - `record`
  - `json_t *`
  - JSON object representing the resource to delete (owned).

* - `jn_options`
  - `json_t *`
  - Additional options for deleting the resource (owned).
:::

        ''',
        "return_value": '''
- Returns `0` on success.
- Returns `-1` if the GObj is invalid, the `record` parameter is `NULL`, or the `mt_delete_resource` method is not implemented.
        ''',
        "notes": '''
- **Ownership:**
  - The `record` and `jn_options` parameters are owned by the function and will be decremented internally.
- **Error Handling:**
  - If the GObj is `NULL` or destroyed, the function logs an error and returns `-1`.
  - If the `mt_delete_resource` method is missing, the function logs an error and returns `-1`.
        '''
    },

    {
        "name": "gobj_get_resource",
        "description": '''
The `gobj_get_resource` function retrieves a specific resource associated with a GObj that matches the given filter. Additional options for retrieving the resource can be provided via `jn_options`. This function delegates its operation to the `mt_get_resource` method of the GObj's GClass.

If the `mt_get_resource` method is not implemented in the GClass, the function logs an error and returns `NULL`.

        ''',
        "prototype": '''
PUBLIC json_t *gobj_get_resource(
    hgobj       gobj_,
    const char  *resource,
    json_t      *jn_filter,  // owned
    json_t      *jn_options // owned
);
        ''',
        "parameters": '''
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
  - The name of the resource to retrieve.

* - `jn_filter`
  - `json_t *`
  - JSON object defining the filter criteria for identifying the resource (owned).

* - `jn_options`
  - `json_t *`
  - Additional options for retrieving the resource (owned).
:::
        ''',
        "return_value": '''
- Returns a JSON object representing the resource (not owned by the caller).
- Returns `NULL` if the GObj is invalid or the `mt_get_resource` method is not implemented.
        ''',
        "notes": '''
- **Ownership:**
  - The `jn_filter` and `jn_options` parameters are owned by the function and will be decremented internally.
  - The returned JSON object is not owned by the caller and must not be decremented.
- **Error Handling:**
  - If the GObj is `NULL` or destroyed, the function logs an error and returns `NULL`.
  - If the `mt_get_resource` method is missing, the function logs an error and returns `NULL`.
        '''
    },

    {
        "name": "gobj_list_resource",
        "description": '''
The `gobj_list_resource` function retrieves a list of resources associated with a GObj that match the specified filter. Additional options for listing resources can be provided via `jn_options`. This function delegates its operation to the `mt_list_resource` method of the GObj's GClass.

If the `mt_list_resource` method is not implemented in the GClass, the function logs an error and returns `NULL`.

        ''',
        "prototype": '''
PUBLIC json_t *gobj_list_resource(
    hgobj       gobj_,
    const char  *resource,
    json_t      *jn_filter,  // owned
    json_t      *jn_options // owned
);
        ''',
        "parameters": '''
::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - **Parameter**
  - **Type**
  - **Description**

* - `gobj_`
  - `hgobj`
  - The GObj instance associated with the resources.

* - `resource`
  - `const char *`
  - The name of the resource type to list.

* - `jn_filter`
  - `json_t *`
  - JSON object defining the filter criteria for the resources (owned).

* - `jn_options`
  - `json_t *`
  - Additional options for listing the resources (owned).
:::

        ''',
        "return_value": '''
- Returns a JSON object containing the list of resources (owned by the caller).
- Returns `NULL` if the GObj is invalid or the `mt_list_resource` method is not implemented.
        ''',
        "notes": '''
- **Ownership:**
  - The `jn_filter` and `jn_options` parameters are owned by the function and will be decremented internally.
  - The returned JSON object is owned by the caller and must be freed when no longer needed.
- **Error Handling:**
  - If the GObj is `NULL` or destroyed, the function logs an error and returns `NULL`.
  - If the `mt_list_resource` method is missing, the function logs an error and returns `NULL`.
        '''
    },

    {
        "name": "gobj_save_resource",
        "description": '''
The `gobj_save_resource` function saves changes to an existing resource associated with a GObj. The resource to be saved is specified via the `record` parameter, and additional options can be provided using `jn_options`. This function delegates its operation to the `mt_save_resource` method of the GObj's GClass.

If the `mt_save_resource` method is not implemented in the GClass, the function logs an error and returns `-1`.
        ''',
        "prototype": '''
PUBLIC int gobj_save_resource(
    hgobj       gobj_,
    const char  *resource,
    json_t      *record,  // WARNING NOT owned
    json_t      *jn_options // owned
);
        ''',
        "parameters": '''
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

        ''',
        "return_value": '''
- Returns `0` on success.
- Returns `-1` if the GObj is invalid, the `record` parameter is `NULL`, or the `mt_save_resource` method is not implemented.
        ''',
        "notes": '''
- **Ownership:**
  - The `record` parameter is not owned by the function and must be managed by the caller.
  - The `jn_options` parameter is owned by the function and will be decremented internally.
- **Error Handling:**
  - If the GObj is `NULL` or destroyed, the function logs an error and returns `-1`.
  - If the `mt_save_resource` method is missing, the function logs an error and returns `-1`.
        '''
    },


    # {
    #     "name": "",
    #     "description": '''
    #     ''',
    #     "prototype": '''
    #     ''',
    #     "parameters": '''
    #     ''',
    #     "return_value": '''
    #     ''',
    #     "notes": '''
    #     '''
    # },

]


# Loop through the list of names and create a file for each
for fn in functions:
    # Substitute the variable in the template

    formatted_text = template.substitute(
        _name_          = fn['name'],
        _description_   = fn['description'],
        _prototype_     = fn['prototype'],
        _parameters_    = fn['parameters'],
        _return_value_  = fn['return_value'],
        _notes_         = fn['notes'],
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
