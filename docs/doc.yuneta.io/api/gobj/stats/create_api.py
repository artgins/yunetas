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

# List of names
xnames = [
    "gobj_set_stat",
    "gobj_incr_stat",
    "gobj_decr_stat",
    "gobj_get_stat",
    "gobj_jn_stats",
]

functions = [
    {
        "name": "gobj_set_stat",
        "description": '''
Set stat
        ''',
        "prototype": '''
int gobj_set_stat(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `name`
  - `const char *`
  - The name of the GObj to be created.

:::
        ''',

        "return_value": '''
- Returns the handle ([`hgobj`](hgobj)) to the created GObj.  
- Returns `NULL` if the creation fails.
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
        print(f"File {file_name} already exists. Skipping...")
        continue  # Skip to the next name

    # Write the formatted text to the file
    with open(file_name, "w") as file:
        file.write(formatted_text)

    print(f"File created: {file_name}")
