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
        "name": "gobj_set_stat",
        "description": '''
Set a specific statistical value for a GObj.
        ''',
        "prototype": '''
PUBLIC json_int_t gobj_set_stat(hgobj gobj, const char *path, json_int_t value);
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
  - Handle to the GObj.

* - `path`
  - `const char *`
  - Path to the statistic to set.

* - `value`
  - `json_int_t`
  - The value to set for the specified statistic.

:::
        ''',
        "return_value": '''
- Returns the old value of the statistic as `json_int_t`.
        '''
    },
    {
        "name": "gobj_incr_stat",
        "description": '''
Increment a specific statistical value for a GObj.
        ''',
        "prototype": '''
PUBLIC json_int_t gobj_incr_stat(hgobj gobj, const char *path, json_int_t value);
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
  - Handle to the GObj.

* - `path`
  - `const char *`
  - Path to the statistic to increment.

* - `value`
  - `json_int_t`
  - The value to add to the specified statistic.

:::
        ''',
        "return_value": '''
- Returns the new value of the statistic as `json_int_t`.
        '''
    },
    {
        "name": "gobj_decr_stat",
        "description": '''
Decrement a specific statistical value for a GObj.
        ''',
        "prototype": '''
PUBLIC json_int_t gobj_decr_stat(hgobj gobj, const char *path, json_int_t value);
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
  - Handle to the GObj.

* - `path`
  - `const char *`
  - Path to the statistic to decrement.

* - `value`
  - `json_int_t`
  - The value to subtract from the specified statistic.

:::
        ''',
        "return_value": '''
- Returns the new value of the statistic as `json_int_t`.
        '''
    },
    {
        "name": "gobj_get_stat",
        "description": '''
Retrieve a specific statistical value for a GObj.
        ''',
        "prototype": '''
PUBLIC json_int_t gobj_get_stat(hgobj gobj, const char *path);
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
  - Handle to the GObj.

* - `path`
  - `const char *`
  - Path to the statistic to retrieve.

:::
        ''',
        "return_value": '''
- Returns the value of the statistic as `json_int_t`.
        '''
    },
    {
        "name": "gobj_jn_stats",
        "description": '''
Retrieve the JSON representation of all statistics for a GObj.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_jn_stats(hgobj gobj);
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
  - Handle to the GObj.

:::
        ''',
        "return_value": '''
- Returns a pointer to a JSON object ([`json_t`](json_t)) containing all statistics.  
  **Note:** The returned JSON object is not owned by the caller.
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
