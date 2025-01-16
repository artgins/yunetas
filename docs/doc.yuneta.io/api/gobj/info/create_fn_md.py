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

functions = [
    {
        "name": "gobj_yuno",
        "description": '''
Retrieves the root GObj, also known as the `Yuno`, which serves as the top-level object in the hierarchy.
        ''',
        "prototype": '''
hgobj gobj_yuno(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the root GObj (`Yuno`).
        '''
    },
    {
        "name": "gobj_yuno_name",
        "description": '''
Gets the name of the `Yuno`. The `Yuno` name typically identifies the top-level GObj instance within the application.
        ''',
        "prototype": '''
const char *gobj_yuno_name(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the name of the `Yuno`.
        '''
    },
    {
        "name": "gobj_yuno_role",
        "description": '''
Retrieves the role of the `Yuno`. The `Yuno` role typically describes the function or responsibility of the application.
        ''',
        "prototype": '''
const char *gobj_yuno_role(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the role of the `Yuno`.
        '''
    },
    {
        "name": "gobj_yuno_id",
        "description": '''
Retrieves the unique identifier (ID) of the `Yuno`. This ID is often used to distinguish different instances of the same role or application.
        ''',
        "prototype": '''
const char *gobj_yuno_id(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the unique ID of the `Yuno`.
        '''
    },
    {
        "name": "gobj_yuno_tag",
        "description": '''
Gets the tag associated with the `Yuno`. Tags are optional labels used to group or categorize instances.
        ''',
        "prototype": '''
const char *gobj_yuno_tag(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the tag of the `Yuno`, or `NULL` if no tag is set.
        '''
    },
    {
        "name": "gobj_yuno_role_plus_name",
        "description": '''
Combines the role and name of the `Yuno` into a single string, providing a more descriptive identifier.
        ''',
        "prototype": '''
const char *gobj_yuno_role_plus_name(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a string (`const char *`) in the format `"<role>:<name>"`.
        '''
    },
    {
        "name": "gobj_name",
        "description": '''
Retrieves the name of a specified GObj.
        ''',
        "prototype": '''
const char *gobj_name(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj whose name is being queried.

:::
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the name of the GObj.
        '''
    },
    {
        "name": "gobj_gclass_name",
        "description": '''
Gets the GClass name of a specified GObj. The GClass name represents the type of the GObj.
        ''',
        "prototype": '''
gclass_name_t gobj_gclass_name(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj whose GClass name is being queried.

:::
        ''',
        "return_value": '''
- Returns the GClass name ([`gclass_name_t`](gclass_name_t)) of the specified GObj.
        '''
    },
    {
        "name": "gobj_gclass",
        "description": '''
Retrieves the handle to the GClass associated with a specified GObj.
        ''',
        "prototype": '''
hgclass gobj_gclass(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj whose GClass is being queried.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgclass`](hgclass)) to the GClass of the specified GObj.
        '''
    },
    {
        "name": "gobj_full_name",
        "description": '''
Retrieves the full hierarchical name of a GObj, including its position in the object tree.
        ''',
        "prototype": '''
const char *gobj_full_name(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj whose full name is being queried.

:::
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the full hierarchical name of the GObj.
        '''
    },
    {
        "name": "gobj_priv_data",
        "description": '''
Retrieves the private data of a GObj, which contains instance-specific data defined by the GClass.
        ''',
        "prototype": '''
void *gobj_priv_data(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj whose private data is being queried.

:::
        ''',
        "return_value": '''
- Returns a pointer (`void *`) to the private data of the GObj.
        '''
    }
]

functions.extend([
    {
        "name": "gobj_short_name",
        "description": '''
Retrieves the short name of a GObj, which is typically its local identifier without hierarchical context.
        ''',
        "prototype": '''
const char *gobj_short_name(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj whose short name is being queried.

:::
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the short name of the GObj.
        '''
    },
    {
        "name": "gobj_global_variables",
        "description": '''
Retrieves a JSON object containing the global variables defined for the GObj system.
        ''',
        "prototype": '''
json_t *gobj_global_variables(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a JSON object ([`json_t`](json_t)) containing all global variables.
        '''
    },
    {
        "name": "gobj_parent",
        "description": '''
Retrieves the parent GObj of the specified GObj, reflecting the hierarchical structure.
        ''',
        "prototype": '''
hgobj gobj_parent(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj whose parent is being queried.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) to the parent GObj.  
- Returns `NULL` if the GObj has no parent (e.g., the root GObj).
        '''
    },
    {
        "name": "gobj_is_destroying",
        "description": '''
Checks if the specified GObj is in the process of being destroyed.
        ''',
        "prototype": '''
BOOL gobj_is_destroying(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj being checked.

:::
        ''',
        "return_value": '''
- Returns `TRUE` if the GObj is being destroyed.  
- Returns `FALSE` otherwise.
        '''
    },
    {
        "name": "gobj_is_running",
        "description": '''
Checks if the specified GObj is currently in a "running" state.
        ''',
        "prototype": '''
BOOL gobj_is_running(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj being checked.

:::
        ''',
        "return_value": '''
- Returns `TRUE` if the GObj is running.  
- Returns `FALSE` otherwise.
        '''
    },
    {
        "name": "gobj_is_service",
        "description": '''
Checks if the specified GObj is marked as a service.
        ''',
        "prototype": '''
BOOL gobj_is_service(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj being checked.

:::
        ''',
        "return_value": '''
- Returns `TRUE` if the GObj is a service.  
- Returns `FALSE` otherwise.
        '''
    },
    {
        "name": "gobj_view_tree",
        "description": '''
Generates a JSON representation of the GObj tree, including attributes, states, and runtime details such as whether each GObj is running or playing.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_view_tree(
    hgobj gobj,      // The root GObj to start from.
    json_t *jn_filter // Optional filter to refine the view.
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj tree root from where the view should begin.

* - `jn_filter`
  - [`json_t`](json_t)
  - A JSON filter to customize the output. This can include attributes like `__state__` or `__gclass_name__`.
:::
        ''',
        "return_value": '''
- Returns a JSON object ([`json_t`](json_t)) representing the GObj hierarchy. The returned JSON contains details of attributes, runtime states, and child objects.
- If an error occurs, the function returns `NULL`.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_is_playing",
        "description": '''
Checks if the specified GObj is currently in a "playing" state.
        ''',
        "prototype": '''
BOOL gobj_is_playing(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj being checked.

:::
        ''',
        "return_value": '''
- Returns `TRUE` if the GObj is playing.  
- Returns `FALSE` otherwise.
        '''
    },
    {
        "name": "gobj_is_disabled",
        "description": '''
Checks if the specified GObj is currently disabled.
        ''',
        "prototype": '''
BOOL gobj_is_disabled(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj being checked.

:::
        ''',
        "return_value": '''
- Returns `TRUE` if the GObj is disabled.  
- Returns `FALSE` otherwise.
        '''
    },
    {
        "name": "gobj_is_volatil",
        "description": '''
Checks if the specified GObj is marked as "volatile". Volatile GObjs are transient and typically destroyed after completing their purpose.
        ''',
        "prototype": '''
BOOL gobj_is_volatil(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj being checked.

:::
        ''',
        "return_value": '''
- Returns `TRUE` if the GObj is volatile.  
- Returns `FALSE` otherwise.
        '''
    },
    {
        "name": "gobj_set_volatil",
        "description": '''
Marks the specified GObj as "volatile", making it transient and intended for temporary use.
        ''',
        "prototype": '''
void gobj_set_volatil(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj to be marked as volatile.

:::
        ''',
        "return_value": '''
- Returns `void`.
        '''
    },
    {
        "name": "gobj_is_pure_child",
        "description": '''
Checks if the specified GObj is a "pure child". Pure child GObjs have limited independence and depend on their parent GObj.
        ''',
        "prototype": '''
BOOL gobj_is_pure_child(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj being checked.

:::
        ''',
        "return_value": '''
- Returns `TRUE` if the GObj is a pure child.  
- Returns `FALSE` otherwise.
        '''
    },
    {
        "name": "gobj_typeof_gclass",
        "description": '''
Retrieves the GClass of the specified GObj, allowing the identification of its type.
        ''',
        "prototype": '''
hgclass gobj_typeof_gclass(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj whose GClass is being queried.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgclass`](hgclass)) of the GClass associated with the GObj.
        '''
    },
    {
        "name": "gobj_typeof_inherited_gclass",
        "description": '''
Retrieves the inherited GClass of the specified GObj. This is used when a GObj derives behavior from another GClass.
        ''',
        "prototype": '''
hgclass gobj_typeof_inherited_gclass(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj whose inherited GClass is being queried.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgclass`](hgclass)) of the inherited GClass associated with the GObj.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_yuno_realm_id",
        "description": '''
Retrieves the realm ID of the `Yuno`. This represents a unique identifier for the realm in which the `Yuno` operates.
        ''',
        "prototype": '''
const char *gobj_yuno_realm_id(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the realm ID of the `Yuno`.
        '''
    },
    {
        "name": "gobj_yuno_realm_owner",
        "description": '''
Gets the owner of the `Yuno` realm. The owner represents the entity responsible for the realm.
        ''',
        "prototype": '''
const char *gobj_yuno_realm_owner(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the owner of the `Yuno` realm.
        '''
    },
    {
        "name": "gobj_yuno_realm_role",
        "description": '''
Retrieves the role of the `Yuno` realm. This role describes the purpose or responsibility of the realm.
        ''',
        "prototype": '''
const char *gobj_yuno_realm_role(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the role of the `Yuno` realm.
        '''
    },
    {
        "name": "gobj_yuno_realm_name",
        "description": '''
Gets the name of the `Yuno` realm. The name typically provides a human-readable identifier for the realm.
        ''',
        "prototype": '''
const char *gobj_yuno_realm_name(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the name of the `Yuno` realm.
        '''
    },
    {
        "name": "gobj_yuno_realm_env",
        "description": '''
Retrieves the environment of the `Yuno` realm. The environment specifies the context in which the `Yuno` operates, such as "production" or "testing".
        ''',
        "prototype": '''
const char *gobj_yuno_realm_env(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the environment of the `Yuno` realm.
        '''
    },
    {
        "name": "gobj_yuno_node_owner",
        "description": '''
Gets the owner of the node in the `Yuno` system. This owner represents the entity responsible for the node.
        ''',
        "prototype": '''
const char *gobj_yuno_node_owner(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the owner of the node.
        '''
    },
    {
        "name": "get_max_system_memory",
        "description": '''
Retrieves the maximum system memory available for use.
        ''',
        "prototype": '''
size_t get_max_system_memory(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns the maximum system memory (`size_t`) available.
        '''
    },
    {
        "name": "get_cur_system_memory",
        "description": '''
Gets the current system memory usage.
        ''',
        "prototype": '''
size_t get_cur_system_memory(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns the current system memory usage (`size_t`).
        '''
    },
    {
        "name": "gclass_command_desc",
        "description": '''
Retrieves a description of the available commands in a GClass.
        ''',
        "prototype": '''
json_t *gclass_command_desc(hgclass gclass);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gclass`
  - [`hgclass`](hgclass)
  - Handle to the GClass whose commands are being described.

:::
        ''',
        "return_value": '''
- Returns a JSON object ([`json_t`](json_t)) describing the commands available in the GClass.
        '''
    },
    {
        "name": "gobj_command_desc",
        "description": '''
Retrieves a description of the commands available in a specific GObj.
        ''',
        "prototype": '''
json_t *gobj_command_desc(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj whose commands are being described.

:::
        ''',
        "return_value": '''
- Returns a JSON object ([`json_t`](json_t)) describing the commands available in the GObj.
        '''
    },
    {
        "name": "get_sdata_flag_table",
        "description": '''
Retrieves the table of data flags used in the GObj system.
        ''',
        "prototype": '''
json_t *get_sdata_flag_table(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a JSON object ([`json_t`](json_t)) containing the data flag table.
        '''
    },
    {
        "name": "get_sdata_flag_desc",
        "description": '''
Retrieves the description of a specific data flag.
        ''',
        "prototype": '''
const char *get_sdata_flag_desc(int flag);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `flag`
  - `int`
  - The data flag whose description is being retrieved.

:::
        ''',
        "return_value": '''
- Returns a string (`const char *`) containing the description of the specified data flag.
        '''
    }
])

functions.extend([
    {
        "name": "get_attrs_schema",
        "description": '''
Retrieves the attribute schema of a specified GClass or GObj. This schema describes the attributes, their types, and characteristics.
        ''',
        "prototype": '''
json_t *get_attrs_schema(hgclass gclass);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gclass`
  - [`hgclass`](hgclass)
  - Handle to the GClass whose attribute schema is being queried.

:::
        ''',
        "return_value": '''
- Returns a JSON object ([`json_t`](json_t)) containing the attribute schema of the specified GClass.
        '''
    },
    {
        "name": "gclass2json",
        "description": '''
Converts a GClass to its JSON representation. This representation includes metadata, such as methods, attributes, and trace levels.
        ''',
        "prototype": '''
json_t *gclass2json(hgclass gclass, int verbose);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gclass`
  - [`hgclass`](hgclass)
  - Handle to the GClass to be converted to JSON.

* - `verbose`
  - `int`
  - Verbosity level for the JSON output:
    - **`0`**: Basic metadata.
    - **`1`**: Includes additional details.

:::
        ''',
        "return_value": '''
- Returns a JSON object ([`json_t`](json_t)) representing the specified GClass.
        '''
    },
    {
        "name": "gobj2json",
        "description": '''
Converts a GObj to its JSON representation. The representation includes information such as attributes, state, and hierarchy.
        ''',
        "prototype": '''
json_t *gobj2json(hgobj gobj, int verbose);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj to be converted to JSON.

* - `verbose`
  - `int`
  - Verbosity level for the JSON output:
    - **`0`**: Basic metadata.
    - **`1`**: Includes additional details.

:::
        ''',
        "return_value": '''
- Returns a JSON object ([`json_t`](json_t)) representing the specified GObj.
        '''
    }
])

# Loop through the list of names and create a file for each
for fn in functions:
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
