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
]
functions.extend([
    {
        "name": "gobj_repr_global_trace_levels",
        "description": '''
Retrieve a string representation of the global trace levels.
        ''',
        "prototype": '''
PUBLIC const char *gobj_repr_global_trace_levels(
    void
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `void`
  - `void`
  - This function takes no arguments.

:::
        ''',
        "return_value": '''
Returns a string representation of all global trace levels currently set.
        '''
    },
    {
        "name": "gobj_repr_gclass_trace_levels",
        "description": '''
Retrieve a string representation of the trace levels for a specific GClass.
        ''',
        "prototype": '''
PUBLIC const char *gobj_repr_gclass_trace_levels(
    hgclass     gclass
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gclass`
  - `hgclass`
  - The handle of the GClass whose trace levels are being represented.
:::
        ''',
        "return_value": '''
Returns a string representation of the trace levels for the specified GClass.
        '''
    },
    {
        "name": "gobj_trace_level_list",
        "description": '''
Retrieve a list of all trace levels.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_trace_level_list(
    void
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `void`
  - `void`
  - This function takes no arguments.

:::
        ''',
        "return_value": '''
Returns a JSON array of all trace levels available in the system.
        '''
    },
    {
        "name": "gobj_get_global_trace_level",
        "description": '''
Get the value of a specific global trace level.
        ''',
        "prototype": '''
PUBLIC BOOL gobj_get_global_trace_level(
    const char  *level
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `level`
  - `const char *`
  - The name of the global trace level to retrieve.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the specified trace level is enabled, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "gobj_get_gclass_trace_level",
        "description": '''
Get the value of a specific trace level for a GClass.
        ''',
        "prototype": '''
PUBLIC BOOL gobj_get_gclass_trace_level(
    hgclass     gclass,
    const char  *level
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gclass`
  - `hgclass`
  - The handle of the GClass whose trace level is being retrieved.

* - `level`
  - `const char *`
  - The name of the trace level to retrieve.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the specified trace level is enabled for the GClass, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "gobj_get_gclass_trace_no_level",
        "description": '''
Check if a specific trace level is disabled for a GClass.
        ''',
        "prototype": '''
PUBLIC BOOL gobj_get_gclass_trace_no_level(
    hgclass     gclass,
    const char  *level
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gclass`
  - `hgclass`
  - The handle of the GClass whose trace level is being checked.

* - `level`
  - `const char *`
  - The name of the trace level to check.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the specified trace level is disabled for the GClass, otherwise returns `FALSE`.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_get_gobj_trace_level",
        "description": '''
Get the value of a specific trace level for a GObj.
        ''',
        "prototype": '''
PUBLIC BOOL gobj_get_gobj_trace_level(
    hgobj       gobj,
    const char  *level
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
  - `hgobj`
  - The GObj whose trace level is being retrieved.

* - `level`
  - `const char *`
  - The name of the trace level to retrieve.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the specified trace level is enabled for the GObj, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "gobj_get_gobj_trace_no_level",
        "description": '''
Check if a specific trace level is disabled for a GObj.
        ''',
        "prototype": '''
PUBLIC BOOL gobj_get_gobj_trace_no_level(
    hgobj       gobj,
    const char  *level
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
  - `hgobj`
  - The GObj whose trace level is being checked.

* - `level`
  - `const char *`
  - The name of the trace level to check.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the specified trace level is disabled for the GObj, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "gobj_get_gclass_trace_level_list",
        "description": '''
Retrieve a list of all enabled trace levels for a GClass.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_get_gclass_trace_level_list(
    hgclass     gclass
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gclass`
  - `hgclass`
  - The handle of the GClass whose enabled trace levels are being retrieved.
:::
        ''',
        "return_value": '''
Returns a JSON array containing all enabled trace levels for the GClass.
        '''
    },
    {
        "name": "gobj_get_gclass_trace_no_level_list",
        "description": '''
Retrieve a list of all disabled trace levels for a GClass.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_get_gclass_trace_no_level_list(
    hgclass     gclass
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gclass`
  - `hgclass`
  - The handle of the GClass whose disabled trace levels are being retrieved.
:::
        ''',
        "return_value": '''
Returns a JSON array containing all disabled trace levels for the GClass.
        '''
    },
    {
        "name": "gobj_get_gobj_trace_level_tree",
        "description": '''
Retrieve a hierarchical tree of all enabled trace levels for a GObj and its children.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_get_gobj_trace_level_tree(
    hgobj       gobj
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
  - `hgobj`
  - The GObj whose enabled trace levels tree is being retrieved.
:::
        ''',
        "return_value": '''
Returns a JSON object representing the hierarchical tree of enabled trace levels for the GObj and its children.
        '''
    },
    {
        "name": "gobj_get_gobj_trace_no_level_tree",
        "description": '''
Retrieve a hierarchical tree of all disabled trace levels for a GObj and its children.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_get_gobj_trace_no_level_tree(
    hgobj       gobj
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
  - `hgobj`
  - The GObj whose disabled trace levels tree is being retrieved.
:::
        ''',
        "return_value": '''
Returns a JSON object representing the hierarchical tree of disabled trace levels for the GObj and its children.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_trace_level",
        "description": '''
Enable a specific trace level for a GObj.
        ''',
        "prototype": '''
PUBLIC int gobj_trace_level(
    hgobj       gobj,
    const char  *level,
    BOOL        set
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
  - `hgobj`
  - The GObj for which the trace level is being modified.

* - `level`
  - `const char *`
  - The name of the trace level to enable or disable.

* - `set`
  - `BOOL`
  - If `TRUE`, the trace level will be enabled; if `FALSE`, it will be disabled.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    },
    {
        "name": "gobj_trace_no_level",
        "description": '''
Disable a specific trace level for a GObj.
        ''',
        "prototype": '''
PUBLIC int gobj_trace_no_level(
    hgobj       gobj,
    const char  *level,
    BOOL        unset
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
  - `hgobj`
  - The GObj for which the trace level is being modified.

* - `level`
  - `const char *`
  - The name of the trace level to disable or enable.

* - `unset`
  - `BOOL`
  - If `TRUE`, the trace level will be disabled; if `FALSE`, it will be enabled.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    },
    {
        "name": "is_level_tracing",
        "description": '''
Check if a specific trace level is enabled.
        ''',
        "prototype": '''
PUBLIC BOOL is_level_tracing(
    hgobj       gobj,
    const char  *level
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
  - `hgobj`
  - The GObj to check for the trace level.

* - `level`
  - `const char *`
  - The name of the trace level to check.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the specified trace level is enabled, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "is_level_not_tracing",
        "description": '''
Check if a specific trace level is disabled.
        ''',
        "prototype": '''
PUBLIC BOOL is_level_not_tracing(
    hgobj       gobj,
    const char  *level
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
  - `hgobj`
  - The GObj to check for the trace level.

* - `level`
  - `const char *`
  - The name of the trace level to check.
:::
        ''',
        "return_value": '''
Returns `TRUE` if the specified trace level is disabled, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "gobj_set_gobj_trace",
        "description": '''
Set the trace levels for a specific GObj.
        ''',
        "prototype": '''
PUBLIC int gobj_set_gobj_trace(
    hgobj       gobj,
    const char  *trace_levels
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
  - `hgobj`
  - The GObj for which the trace levels are being set.

* - `trace_levels`
  - `const char *`
  - A comma-separated string of trace levels to enable for the GObj.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    },
    {
        "name": "gobj_set_gclass_trace",
        "description": '''
Set the trace levels for a specific GClass.
        ''',
        "prototype": '''
PUBLIC int gobj_set_gclass_trace(
    hgclass     gclass,
    const char  *trace_levels
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gclass`
  - `hgclass`
  - The GClass for which the trace levels are being set.

* - `trace_levels`
  - `const char *`
  - A comma-separated string of trace levels to enable for the GClass.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_set_deep_tracing",
        "description": '''
Enable or disable deep tracing for a GObj.
        ''',
        "prototype": '''
PUBLIC int gobj_set_deep_tracing(
    hgobj       gobj,
    BOOL        enable
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
  - `hgobj`
  - The GObj for which deep tracing is being set.

* - `enable`
  - `BOOL`
  - If `TRUE`, enables deep tracing; if `FALSE`, disables it.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    },
    {
        "name": "gobj_get_deep_tracing",
        "description": '''
Check if deep tracing is enabled for a GObj.
        ''',
        "prototype": '''
PUBLIC BOOL gobj_get_deep_tracing(
    hgobj       gobj
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
  - `hgobj`
  - The GObj to check for deep tracing.
:::
        ''',
        "return_value": '''
Returns `TRUE` if deep tracing is enabled, otherwise returns `FALSE`.
        '''
    },
    {
        "name": "gobj_set_global_trace",
        "description": '''
Enable a global trace level across all GObjs.
        ''',
        "prototype": '''
PUBLIC int gobj_set_global_trace(
    const char  *level
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `level`
  - `const char *`
  - The name of the global trace level to enable.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    },
    {
        "name": "gobj_set_global_no_trace",
        "description": '''
Disable a global trace level across all GObjs.
        ''',
        "prototype": '''
PUBLIC int gobj_set_global_no_trace(
    const char  *level
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `level`
  - `const char *`
  - The name of the global trace level to disable.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    },
    {
        "name": "gobj_load_trace_filter",
        "description": '''
Load trace filters from a JSON object and apply them to the GObjs.
        ''',
        "prototype": '''
PUBLIC int gobj_load_trace_filter(
    json_t      *jn_trace_filter
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `jn_trace_filter`
  - `json_t *`
  - JSON object containing the trace filters to apply.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    },
    {
        "name": "gobj_add_trace_filter",
        "description": '''
Add a specific trace filter for GObjs.
        ''',
        "prototype": '''
PUBLIC int gobj_add_trace_filter(
    const char  *gobj_name,
    const char  *trace_level
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj_name`
  - `const char *`
  - The name of the GObj to which the trace filter will be applied.

* - `trace_level`
  - `const char *`
  - The trace level to enable for the specified GObj.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_remove_trace_filter",
        "description": '''
Remove a specific trace filter from a GObj.
        ''',
        "prototype": '''
PUBLIC int gobj_remove_trace_filter(
    const char  *gobj_name,
    const char  *trace_level
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj_name`
  - `const char *`
  - The name of the GObj from which the trace filter will be removed.

* - `trace_level`
  - `const char *`
  - The trace level to remove from the specified GObj.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    },
    {
        "name": "gobj_get_trace_filter",
        "description": '''
Retrieve the currently applied trace filters as a JSON object.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_get_trace_filter(
    void
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `void`
  - `void`
  - This function takes no arguments.
:::
        ''',
        "return_value": '''
Returns a JSON object containing the currently applied trace filters.
        '''
    },
    {
        "name": "gobj_set_gclass_no_trace",
        "description": '''
Disable specific trace levels for a GClass.
        ''',
        "prototype": '''
PUBLIC int gobj_set_gclass_no_trace(
    hgclass     gclass,
    const char  *trace_levels
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gclass`
  - `hgclass`
  - The GClass for which the trace levels are being disabled.

* - `trace_levels`
  - `const char *`
  - A comma-separated string of trace levels to disable for the GClass.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    },
    {
        "name": "gobj_set_gobj_no_trace",
        "description": '''
Disable specific trace levels for a GObj.
        ''',
        "prototype": '''
PUBLIC int gobj_set_gobj_no_trace(
    hgobj       gobj,
    const char  *trace_levels
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
  - `hgobj`
  - The GObj for which the trace levels are being disabled.

* - `trace_levels`
  - `const char *`
  - A comma-separated string of trace levels to disable for the GObj.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on error.
        '''
    },
    {
        "name": "trace_machine",
        "description": '''
Log trace information for a state machine.
        ''',
        "prototype": '''
PUBLIC void trace_machine(
    const char  *machine_name,
    const char  *event_name,
    const char  *state_name,
    const char  *next_state_name
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `machine_name`
  - `const char *`
  - The name of the state machine being traced.

* - `event_name`
  - `const char *`
  - The name of the event triggering the trace.

* - `state_name`
  - `const char *`
  - The current state of the state machine.

* - `next_state_name`
  - `const char *`
  - The next state of the state machine after the event.
:::
        ''',
        "return_value": '''
No return value. This function logs trace information.
        '''
    },
    {
        "name": "tab",
        "description": '''
Provide a tab character or an indented string for formatting purposes.
        ''',
        "prototype": '''
PUBLIC const char *tab(
    int         count
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `count`
  - `int`
  - The number of tab characters to generate or apply to the string.
:::
        ''',
        "return_value": '''
Returns a string containing the specified number of tab characters for formatting purposes.
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
