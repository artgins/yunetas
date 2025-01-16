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
        "name": "gobj_send_event",
        "description": '''
Sends an event to a GObj, optionally transferring ownership of the associated data (kw). This is the primary mechanism for triggering actions or changes in GObjs.
        ''',
        "prototype": '''
int gobj_send_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj src);
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
  - Handle to the target GObj receiving the event.

* - `event`
  - [`gobj_event_t`](gobj_event_t)
  - The event to send to the target GObj.

* - `kw`
  - [`json_t *`](json_t)
  - JSON object containing additional data for the event. Ownership is transferred to the function.

* - `src`
  - [`hgobj`](hgobj)
  - Handle to the source GObj sending the event.

:::
        ''',
        "return_value": '''
- `0`: The event was successfully processed.  
- `-1`: An error occurred while processing the event.
        '''
    },
    {
        "name": "gobj_change_state",
        "description": '''
Changes the current state of a GObj to the specified new state. This transition is a critical part of the GObj's state machine behavior.
        ''',
        "prototype": '''
int gobj_change_state(hgobj gobj, gobj_state_t new_state);
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
  - Handle to the GObj whose state is being changed.

* - `new_state`
  - [`gobj_state_t`](gobj_state_t)
  - The name of the new state to transition to.

:::
        ''',
        "return_value": '''
- `0`: The state was successfully changed.  
- `-1`: The state transition failed.
        '''
    },
    {
        "name": "gobj_current_state",
        "description": '''
Retrieves the current state of the specified GObj.
        ''',
        "prototype": '''
gobj_state_t gobj_current_state(hgobj gobj);
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
  - Handle to the GObj whose current state is being queried.

:::
        ''',
        "return_value": '''
- Returns the current state ([`gobj_state_t`](gobj_state_t)) of the GObj.  
- Returns `NULL` if the GObj has no state defined.
        '''
    },
    {
        "name": "gobj_in_this_state",
        "description": '''
Checks whether the GObj is currently in the specified state.
        ''',
        "prototype": '''
BOOL gobj_in_this_state(hgobj gobj, gobj_state_t state);
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

* - `state`
  - [`gobj_state_t`](gobj_state_t)
  - The state to check against the GObj's current state.

:::
        ''',
        "return_value": '''
- Returns `TRUE` if the GObj is in the specified state.  
- Returns `FALSE` otherwise.
        '''
    },
    {
        "name": "gobj_has_event",
        "description": '''
Checks whether the GObj supports the specified event. This is useful for validating event handling capabilities.
        ''',
        "prototype": '''
BOOL gobj_has_event(hgobj gobj, gobj_event_t event);
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

* - `event`
  - [`gobj_event_t`](gobj_event_t)
  - The event to check.

:::
        ''',
        "return_value": '''
- Returns `TRUE` if the GObj supports the specified event.  
- Returns `FALSE` otherwise.
        '''
    },
    {
        "name": "gobj_has_output_event",
        "description": '''
Checks whether the GObj has the specified event marked as an output event. This is useful for verifying publishable events.
        ''',
        "prototype": '''
BOOL gobj_has_output_event(hgobj gobj, gobj_event_t event);
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

* - `event`
  - [`gobj_event_t`](gobj_event_t)
  - The event to check.

:::
        ''',
        "return_value": '''
- Returns `TRUE` if the GObj has the event as an output event.  
- Returns `FALSE` otherwise.
        '''
    },
    {
        "name": "gobj_event_type",
        "description": '''
Retrieves the type of the specified event. This provides metadata about the event's characteristics.
        ''',
        "prototype": '''
int gobj_event_type(hgobj gobj, gobj_event_t event);
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
  - Handle to the GObj associated with the event.

* - `event`
  - [`gobj_event_t`](gobj_event_t)
  - The event whose type is being queried.

:::
        ''',
        "return_value": '''
- Returns an integer representing the event type.  
- Returns `-1` if the event type could not be determined.
        '''
    },
    {
        "name": "gobj_event_type_by_name",
        "description": '''
Retrieves the type of the event by its name. This is useful for querying metadata about events when their name is known.
        ''',
        "prototype": '''
int gobj_event_type_by_name(hgobj gobj, const char *event_name);
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
  - Handle to the GObj associated with the event.

* - `event_name`
  - `const char *`
  - The name of the event whose type is being queried.

:::
        ''',
        "return_value": '''
- Returns an integer representing the event type.  
- Returns `-1` if the event type could not be determined.
        '''
    }
]


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
