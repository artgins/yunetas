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
        "name": "gobj_subscribe_event",
        "description": '''
Subscribes a GObj to a specific event. Subscriptions allow GObjs to listen to and handle events published by other GObjs.
        ''',
        "prototype": '''
int gobj_subscribe_event(hgobj gobj, gobj_event_t event, json_t *kw, hgobj subscriber);
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
  - Handle to the GObj that will publish the event.

* - `event`
  - [`gobj_event_t`](gobj_event_t)
  - The event to subscribe to.

* - `kw`
  - [`json_t *`](json_t)
  - JSON object containing subscription options or filters. Ownership is transferred to the function.

* - `subscriber`
  - [`hgobj`](hgobj)
  - Handle to the GObj that will receive the subscribed event.

:::
        ''',
        "return_value": '''
- `0`: The subscription was successfully added.  
- `-1`: The subscription failed, possibly due to invalid parameters.
        '''
    },
    {
        "name": "gobj_unsubscribe_event",
        "description": '''
Unsubscribes a GObj from a specific event. This stops the GObj from receiving the specified event.
        ''',
        "prototype": '''
int gobj_unsubscribe_event(hgobj gobj, gobj_event_t event, hgobj subscriber);
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
  - Handle to the GObj that was publishing the event.

* - `event`
  - [`gobj_event_t`](gobj_event_t)
  - The event to unsubscribe from.

* - `subscriber`
  - [`hgobj`](hgobj)
  - Handle to the GObj that was receiving the event.

:::
        ''',
        "return_value": '''
- `0`: The subscription was successfully removed.  
- `-1`: The unsubscription failed, possibly because the subscription did not exist.
        '''
    },
    {
        "name": "gobj_unsubscribe_list",
        "description": '''
Unsubscribes a GObj from a list of events. This allows batch unsubscription from multiple events at once.
        ''',
        "prototype": '''
int gobj_unsubscribe_list(hgobj gobj, json_t *event_list, hgobj subscriber);
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
  - Handle to the GObj that was publishing the events.

* - `event_list`
  - [`json_t *`](json_t)
  - JSON array containing the list of events to unsubscribe from.

* - `subscriber`
  - [`hgobj`](hgobj)
  - Handle to the GObj that was receiving the events.

:::
        ''',
        "return_value": '''
- `0`: The subscriptions were successfully removed.  
- `-1`: The unsubscriptions failed, possibly because one or more subscriptions did not exist.
        '''
    },
    {
        "name": "gobj_find_subscriptions",
        "description": '''
Finds all subscriptions for a specific event published by a GObj. This is useful for debugging or monitoring subscriptions.
        ''',
        "prototype": '''
json_t *gobj_find_subscriptions(hgobj gobj, gobj_event_t event);
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
  - Handle to the GObj publishing the event.

* - `event`
  - [`gobj_event_t`](gobj_event_t)
  - The event to find subscriptions for.

:::
        ''',
        "return_value": '''
- Returns a JSON array ([`json_t`](json_t)) containing all subscriptions for the specified event.  
- Returns an empty array if no subscriptions are found.
        '''
    },
    {
        "name": "gobj_find_subscribings",
        "description": '''
Finds all events that a GObj is currently subscribed to. This is useful for debugging or monitoring.
        ''',
        "prototype": '''
json_t *gobj_find_subscribings(hgobj gobj);
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
  - Handle to the GObj whose subscriptions are being queried.

:::
        ''',
        "return_value": '''
- Returns a JSON array ([`json_t`](json_t)) containing all events the GObj is subscribed to.  
- Returns an empty array if the GObj is not subscribed to any events.
        '''
    },
    {
        "name": "gobj_list_subscriptions",
        "description": '''
Lists all subscriptions of a GObj, providing details about the subscribed events and their filters.
        ''',
        "prototype": '''
json_t *gobj_list_subscriptions(hgobj gobj);
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
  - Handle to the GObj whose subscriptions are being listed.

:::
        ''',
        "return_value": '''
- Returns a JSON array ([`json_t`](json_t)) containing details of all subscriptions for the GObj.  
- Returns an empty array if the GObj has no subscriptions.
        '''
    },
    {
        "name": "gobj_publish_event",
        "description": '''
Publishes an event from a GObj to all its subscribers. This is the primary mechanism for broadcasting events to multiple GObjs.
        ''',
        "prototype": '''
int gobj_publish_event(hgobj gobj, gobj_event_t event, json_t *kw);
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
  - Handle to the GObj publishing the event.

* - `event`
  - [`gobj_event_t`](gobj_event_t)
  - The event to publish.

* - `kw`
  - [`json_t *`](json_t)
  - JSON object containing data associated with the event. Ownership is transferred to the function.

:::
        ''',
        "return_value": '''
- `0`: The event was successfully published to all subscribers.  
- `-1`: An error occurred while publishing the event.
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
