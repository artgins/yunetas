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
        "name": "gobj_start",
        "description": '''
Starts a GObj, transitioning it to the "running" state. The GObj may execute initialization logic during this process.
        ''',
        "prototype": '''
int gobj_start(hgobj gobj);
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
  - Handle to the GObj to be started.

:::
        ''',
        "return_value": '''
- `0`: The GObj was successfully started.  
- `-1`: An error occurred during the start process.
        '''
    },
    {
        "name": "gobj_start_childs",
        "description": '''
Starts all child GObjs of the specified parent GObj, transitioning them to the "running" state.
        ''',
        "prototype": '''
int gobj_start_childs(hgobj gobj);
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
  - Handle to the parent GObj whose child GObjs will be started.

:::
        ''',
        "return_value": '''
- `0`: All child GObjs were successfully started.  
- `-1`: An error occurred during the start process for one or more child GObjs.
        '''
    },
    {
        "name": "gobj_start_tree",
        "description": '''
Recursively starts a GObj and all its descendants, transitioning the entire hierarchy to the "running" state.
        ''',
        "prototype": '''
int gobj_start_tree(hgobj gobj);
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
  - Handle to the root GObj of the tree to be started.

:::
        ''',
        "return_value": '''
- `0`: The GObj tree was successfully started.  
- `-1`: An error occurred during the start process for one or more GObjs in the tree.
        '''
    },
    {
        "name": "gobj_stop",
        "description": '''
Stops a GObj, transitioning it to the "stopped" state. Any active processes or resources associated with the GObj may be released.
        ''',
        "prototype": '''
int gobj_stop(hgobj gobj);
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
  - Handle to the GObj to be stopped.

:::
        ''',
        "return_value": '''
- `0`: The GObj was successfully stopped.  
- `-1`: An error occurred during the stop process.
        '''
    },
    {
        "name": "gobj_stop_childs",
        "description": '''
Stops all child GObjs of the specified parent GObj, transitioning them to the "stopped" state.
        ''',
        "prototype": '''
int gobj_stop_childs(hgobj gobj);
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
  - Handle to the parent GObj whose child GObjs will be stopped.

:::
        ''',
        "return_value": '''
- `0`: All child GObjs were successfully stopped.  
- `-1`: An error occurred during the stop process for one or more child GObjs.
        '''
    }
]

functions.extend([
    {
        "name": "gobj_pause",
        "description": '''
Pauses a GObj, transitioning it to the "paused" state. This halts the GObj's activity without stopping it entirely.
        ''',
        "prototype": '''
int gobj_pause(hgobj gobj);
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
  - Handle to the GObj to be paused.

:::
        ''',
        "return_value": '''
- `0`: The GObj was successfully paused.  
- `-1`: An error occurred during the pause process.
        '''
    },
    {
        "name": "gobj_enable",
        "description": '''
Enables a GObj, making it active. An enabled GObj is ready to start or operate.
        ''',
        "prototype": '''
int gobj_enable(hgobj gobj);
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
  - Handle to the GObj to be enabled.

:::
        ''',
        "return_value": '''
- `0`: The GObj was successfully enabled.  
- `-1`: An error occurred during the enable process.
        '''
    },
    {
        "name": "gobj_disable",
        "description": '''
Disables a GObj, marking it as inactive. A disabled GObj cannot be started or operate.
        ''',
        "prototype": '''
int gobj_disable(hgobj gobj);
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
  - Handle to the GObj to be disabled.

:::
        ''',
        "return_value": '''
- `0`: The GObj was successfully disabled.  
- `-1`: An error occurred during the disable process.
        '''
    },
    {
        "name": "gobj_change_parent",
        "description": '''
Changes the parent of a GObj, reassigning it to a new hierarchy under the specified parent.
        ''',
        "prototype": '''
int gobj_change_parent(hgobj gobj, hgobj new_parent);
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
  - Handle to the GObj whose parent is being changed.

* - `new_parent`
  - [`hgobj`](hgobj)
  - Handle to the new parent GObj.

:::
        ''',
        "return_value": '''
- `0`: The GObj's parent was successfully changed.  
- `-1`: An error occurred during the parent change process.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_autostart_services",
        "description": '''
Automatically starts all services marked for autostart within a Yuno. This is typically used during the initialization phase of a Yuno to ensure required services are running.
        ''',
        "prototype": '''
int gobj_autostart_services(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - None
  - -
  - This function does not accept any parameters.

:::
        ''',
        "return_value": '''
- `0`: All autostart services were successfully started.  
- `-1`: An error occurred while starting one or more services.
        '''
    },
    {
        "name": "gobj_autoplay_services",
        "description": '''
Automatically plays all services marked for autoplay within a Yuno. This ensures that the required services transition into an operational state.
        ''',
        "prototype": '''
int gobj_autoplay_services(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - None
  - -
  - This function does not accept any parameters.

:::
        ''',
        "return_value": '''
- `0`: All autoplay services were successfully played.  
- `-1`: An error occurred while playing one or more services.
        '''
    },
    {
        "name": "gobj_stop_autostart_services",
        "description": '''
Stops all services that were marked for autostart within a Yuno. This is typically used during the shutdown phase of a Yuno to gracefully stop running services.
        ''',
        "prototype": '''
int gobj_stop_autostart_services(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - None
  - -
  - This function does not accept any parameters.

:::
        ''',
        "return_value": '''
- `0`: All autostart services were successfully stopped.  
- `-1`: An error occurred while stopping one or more services.
        '''
    },
    {
        "name": "gobj_pause_autoplay_services",
        "description": '''
Pauses all services that were marked for autoplay within a Yuno. This halts their operation without stopping them entirely.
        ''',
        "prototype": '''
int gobj_pause_autoplay_services(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - None
  - -
  - This function does not accept any parameters.

:::
        ''',
        "return_value": '''
- `0`: All autoplay services were successfully paused.  
- `-1`: An error occurred while pausing one or more services.
        '''
    },
    {
        "name": "gobj_command",
        "description": '''
Executes a command on the specified GObj. Commands allow direct interaction with GObjs, enabling specific actions or behaviors.
        ''',
        "prototype": '''
json_t *gobj_command(hgobj gobj, const char *cmd, json_t *kw, hgobj src);
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
  - Handle to the GObj on which the command is executed.

* - `cmd`
  - `const char *`
  - The name of the command to execute.

* - `kw`
  - [`json_t *`](json_t)
  - JSON object containing parameters for the command. Ownership is transferred to the function.

* - `src`
  - [`hgobj`](hgobj)
  - Handle to the source GObj initiating the command.

:::
        ''',
        "return_value": '''
- Returns a JSON object ([`json_t`](json_t)) containing the command's result and data.  
- Returns `NULL` if the command could not be executed.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_stats",
        "description": '''
Retrieves statistics from the specified GObj. Statistics provide insight into the GObj's operational metrics and performance.
        ''',
        "prototype": '''
json_t *gobj_stats(hgobj gobj, const char *stats, json_t *kw, hgobj src);
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
  - Handle to the GObj whose statistics are being retrieved.

* - `stats`
  - `const char *`
  - A string specifying which statistics to retrieve. Can be `NULL` to retrieve all statistics.

* - `kw`
  - [`json_t *`](json_t)
  - JSON object containing additional options or filters for the statistics request. Ownership is transferred to the function.

* - `src`
  - [`hgobj`](hgobj)
  - Handle to the source GObj initiating the request.

:::
        ''',
        "return_value": '''
- Returns a JSON object ([`json_t`](json_t)) containing the requested statistics.  
- Returns `NULL` if the statistics could not be retrieved.
        '''
    },
    {
        "name": "build_command_response",
        "description": '''
Constructs a response for a command executed on a GObj. This function is typically used to standardize the format of command responses.
        ''',
        "prototype": '''
json_t *build_command_response(hgobj gobj, int result, const char *message, json_t *kw);
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
  - Handle to the GObj generating the command response.

* - `result`
  - `int`
  - The result code of the command execution (e.g., `0` for success, negative values for errors).

* - `message`
  - `const char *`
  - A descriptive message associated with the command's result.

* - `kw`
  - [`json_t *`](json_t)
  - JSON object containing additional data for the response. Ownership is transferred to the function.

:::
        ''',
        "return_value": '''
- Returns a JSON object ([`json_t`](json_t)) containing the command response.  
- The response includes keys like `result`, `message`, and additional data.
        '''
    },
    {
        "name": "gobj_set_bottom_gobj",
        "description": '''
Sets the bottom GObj in the hierarchy for the specified GObj. This bottom GObj serves as a delegation point for certain operations or attributes.
        ''',
        "prototype": '''
void gobj_set_bottom_gobj(hgobj gobj, hgobj bottom_gobj);
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
  - Handle to the GObj whose bottom GObj is being set.

* - `bottom_gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj to be set as the bottom.

:::
        ''',
        "return_value": '''
- None.
        '''
    },
    {
        "name": "gobj_last_bottom_gobj",
        "description": '''
Retrieves the last bottom GObj in the hierarchy of the specified GObj. This function traverses the bottom hierarchy to find the deepest delegation point.
        ''',
        "prototype": '''
hgobj gobj_last_bottom_gobj(hgobj gobj);
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
  - Handle to the GObj whose last bottom GObj is being retrieved.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the last bottom GObj in the hierarchy.  
- Returns `NULL` if the GObj has no bottom GObjs.
        '''
    },
    {
        "name": "gobj_bottom_gobj",
        "description": '''
Retrieves the bottom GObj directly associated with the specified GObj. This function does not traverse the entire hierarchy.
        ''',
        "prototype": '''
hgobj gobj_bottom_gobj(hgobj gobj);
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
  - Handle to the GObj whose bottom GObj is being retrieved.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the bottom GObj directly associated with the specified GObj.  
- Returns `NULL` if the GObj does not have a bottom GObj.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_services",
        "description": '''
Lists all service GObjs in the current Yuno. Services are GObjs marked with the service flag, typically representing significant system components.
        ''',
        "prototype": '''
json_t *gobj_services(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - None
  - -
  - This function does not accept any parameters.

:::
        ''',
        "return_value": '''
- Returns a JSON array ([`json_t`](json_t)) of service GObjs, including their metadata.  
- Returns an empty array if no services are registered.
        '''
    },
    {
        "name": "gobj_default_service",
        "description": '''
Retrieves the default service GObj for the current Yuno. The default service is typically the primary operational component of the Yuno.
        ''',
        "prototype": '''
hgobj gobj_default_service(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - None
  - -
  - This function does not accept any parameters.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the default service GObj.  
- Returns `NULL` if no default service is defined.
        '''
    },
    {
        "name": "gobj_find_service",
        "description": '''
Finds a service GObj by its name. This allows direct access to a specific service for interaction or management.
        ''',
        "prototype": '''
hgobj gobj_find_service(const char *service_name);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `service_name`
  - `const char *`
  - The name of the service GObj to find.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the service GObj with the specified name.  
- Returns `NULL` if no matching service is found.
        '''
    },
    {
        "name": "gobj_find_service_by_gclass",
        "description": '''
Finds a service GObj by its GClass. This allows access to the first service matching the specified GClass type.
        ''',
        "prototype": '''
hgobj gobj_find_service_by_gclass(gclass_name_t gclass_name);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gclass_name`
  - [`gclass_name_t`](gclass_name_t)
  - The GClass name of the service GObj to find.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the first service matching the specified GClass.  
- Returns `NULL` if no matching service is found.
        '''
    },
    {
        "name": "gobj_find_gclass_service",
        "description": '''
Finds a service GObj of a specific GClass. Similar to `gobj_find_service_by_gclass()`, but with more flexibility in service discovery.
        ''',
        "prototype": '''
hgobj gobj_find_gclass_service(gclass_name_t gclass_name);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gclass_name`
  - [`gclass_name_t`](gclass_name_t)
  - The GClass name of the service GObj to find.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the service matching the specified GClass.  
- Returns `NULL` if no matching service is found.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_find_gobj",
        "description": '''
Finds a GObj by its name within the hierarchy. This is useful for locating specific GObjs for interaction or management.
        ''',
        "prototype": '''
hgobj gobj_find_gobj(const char *gobj_name);
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
  - The name of the GObj to find.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the GObj with the specified name.  
- Returns `NULL` if no matching GObj is found.
        '''
    },
    {
        "name": "gobj_first_child",
        "description": '''
Retrieves the first child GObj of the specified parent GObj. This is useful for iterating through child GObjs.
        ''',
        "prototype": '''
hgobj gobj_first_child(hgobj gobj);
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
  - Handle to the parent GObj whose first child is being retrieved.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the first child GObj.  
- Returns `NULL` if the parent GObj has no children.
        '''
    },
    {
        "name": "gobj_last_child",
        "description": '''
Retrieves the last child GObj of the specified parent GObj. This is useful for reverse iteration through child GObjs.
        ''',
        "prototype": '''
hgobj gobj_last_child(hgobj gobj);
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
  - Handle to the parent GObj whose last child is being retrieved.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the last child GObj.  
- Returns `NULL` if the parent GObj has no children.
        '''
    },
    {
        "name": "gobj_next_child",
        "description": '''
Retrieves the next sibling GObj of the specified GObj within the same parent hierarchy.
        ''',
        "prototype": '''
hgobj gobj_next_child(hgobj gobj);
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
  - Handle to the GObj whose next sibling is being retrieved.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the next sibling GObj.  
- Returns `NULL` if the GObj has no next sibling.
        '''
    },
    {
        "name": "gobj_prev_child",
        "description": '''
Retrieves the previous sibling GObj of the specified GObj within the same parent hierarchy.
        ''',
        "prototype": '''
hgobj gobj_prev_child(hgobj gobj);
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
  - Handle to the GObj whose previous sibling is being retrieved.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the previous sibling GObj.  
- Returns `NULL` if the GObj has no previous sibling.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_child_by_name",
        "description": '''
Finds a child GObj by its name under the specified parent GObj. This allows for direct access to a child GObj within the hierarchy.
        ''',
        "prototype": '''
hgobj gobj_child_by_name(hgobj gobj, const char *child_name);
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
  - Handle to the parent GObj.

* - `child_name`
  - `const char *`
  - The name of the child GObj to find.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the child GObj with the specified name.  
- Returns `NULL` if no matching child GObj is found.
        '''
    },
    {
        "name": "gobj_child_size",
        "description": '''
Returns the total number of child GObjs under the specified parent GObj. This is useful for assessing the scope of a GObj's hierarchy.
        ''',
        "prototype": '''
int gobj_child_size(hgobj gobj);
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
  - Handle to the parent GObj.

:::
        ''',
        "return_value": '''
- Returns the total number of child GObjs under the specified parent GObj.  
- Returns `0` if the GObj has no children.
        '''
    },
    {
        "name": "gobj_child_size2",
        "description": '''
Returns the total number of child GObjs under the specified parent GObj, with additional filtering options. This allows for more specific queries within the hierarchy.
        ''',
        "prototype": '''
int gobj_child_size2(hgobj gobj, json_t *filter);
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
  - Handle to the parent GObj.

* - `filter`
  - [`json_t *`](json_t)
  - JSON object containing filter criteria for the child GObjs.

:::
        ''',
        "return_value": '''
- Returns the total number of child GObjs under the specified parent GObj that match the filter criteria.  
- Returns `0` if no matching child GObjs are found.
        '''
    },
    {
        "name": "gobj_search_path",
        "description": '''
Searches for a GObj along a specified hierarchical path. This is useful for locating GObjs in complex structures.
        ''',
        "prototype": '''
hgobj gobj_search_path(hgobj gobj, const char *path);
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
  - Handle to the GObj serving as the search root.

* - `path`
  - `const char *`
  - The hierarchical path to search for the GObj.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the GObj located at the specified path.  
- Returns `NULL` if no matching GObj is found.
        '''
    },
    {
        "name": "gobj_match_gobj",
        "description": '''
Checks if a GObj matches specified criteria provided in a filter. This is useful for validating or selecting GObjs based on attributes.
        ''',
        "prototype": '''
BOOL gobj_match_gobj(hgobj gobj, json_t *filter);
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
  - Handle to the GObj to check.

* - `filter`
  - [`json_t *`](json_t)
  - JSON object containing criteria to match.

:::
        ''',
        "return_value": '''
- Returns `TRUE` if the GObj matches the filter criteria.  
- Returns `FALSE` otherwise.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_find_child",
        "description": '''
Finds a child GObj under the specified parent GObj based on specific criteria. This function is useful for advanced child lookups.
        ''',
        "prototype": '''
hgobj gobj_find_child(hgobj gobj, json_t *filter);
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
  - Handle to the parent GObj.

* - `filter`
  - [`json_t *`](json_t)
  - JSON object containing criteria to find the child GObj.

:::
        ''',
        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) of the child GObj matching the criteria.  
- Returns `NULL` if no matching child is found.
        '''
    },
    {
        "name": "gobj_walk_gobj_childs",
        "description": '''
Traverses the immediate child GObjs of the specified parent GObj and applies a user-provided callback function to each.
        ''',
        "prototype": '''
int gobj_walk_gobj_childs(hgobj gobj, int (*cb)(hgobj child, void *user_data), void *user_data);
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
  - Handle to the parent GObj.

* - `cb`
  - `int (*cb)(hgobj, void *)`
  - Callback function to be applied to each child GObj.

* - `user_data`
  - `void *`
  - Pointer to user-defined data passed to the callback function.

:::
        ''',
        "return_value": '''
- Returns `0` if the traversal completes successfully.  
- Returns a negative value if the callback function halts the traversal.
        '''
    },
    {
        "name": "gobj_walk_gobj_childs_tree",
        "description": '''
Traverses the entire hierarchy of child GObjs under the specified parent GObj and applies a user-provided callback function to each.
        ''',
        "prototype": '''
int gobj_walk_gobj_childs_tree(hgobj gobj, int (*cb)(hgobj child, void *user_data), void *user_data);
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
  - Handle to the root GObj of the hierarchy.

* - `cb`
  - `int (*cb)(hgobj, void *)`
  - Callback function to be applied to each GObj in the hierarchy.

* - `user_data`
  - `void *`
  - Pointer to user-defined data passed to the callback function.

:::
        ''',
        "return_value": '''
- Returns `0` if the traversal completes successfully.  
- Returns a negative value if the callback function halts the traversal.
        '''
    },
    {
        "name": "gobj_exec_internal_method",
        "description": '''
Executes an internal method on a GObj. Internal methods are specific to the GClass of the GObj and provide additional functionality.
        ''',
        "prototype": '''
json_t *gobj_exec_internal_method(hgobj gobj, const char *method_name, json_t *kw, hgobj src);
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
  - Handle to the GObj on which the method will be executed.

* - `method_name`
  - `const char *`
  - Name of the internal method to execute.

* - `kw`
  - [`json_t *`](json_t)
  - JSON object containing parameters for the method. Ownership is transferred to the function.

* - `src`
  - [`hgobj`](hgobj)
  - Handle to the source GObj initiating the method execution.

:::
        ''',
        "return_value": '''
- Returns a JSON object ([`json_t`](json_t)) containing the result of the method execution.  
- Returns `NULL` if the method does not exist or an error occurs.
        '''
    }
])

functions.extend([
    {
        "name": "gobj_play",
        "description": '''
Transitions a GObj to the "playing" state. This function ensures that the GObj is in an operational state where it can handle events and perform its intended tasks.
        ''',
        "prototype": '''
int gobj_play(hgobj gobj);
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
  - Handle to the GObj to be transitioned to the "playing" state.

:::
        ''',
        "return_value": '''
- `0`: The GObj successfully transitioned to the "playing" state.  
- `-1`: The transition failed, possibly because the GObj was not in a compatible state.
        '''
    },
    {
        "name": "gobj_stop_tree",
        "description": '''
Stops the specified GObj and its entire hierarchy of child GObjs. This function ensures that all dependent GObjs are gracefully stopped in a bottom-up manner.
        ''',
        "prototype": '''
int gobj_stop_tree(hgobj gobj);
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
  - Handle to the root GObj of the hierarchy to be stopped.

:::
        ''',
        "return_value": '''
- `0`: The GObj and its child hierarchy were successfully stopped.  
- `-1`: An error occurred while stopping one or more GObjs in the hierarchy.
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
