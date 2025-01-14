#!/usr/bin/python

from string import Template
import os

template = Template("""

<!-- ============================================================== -->
({$_name_}())=
# `{$_name_}()`
<!-- ============================================================== -->



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
```

**Parameters**

:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description
* - `gclass`
  - [`hgclass`](hgclass)
  - Handle to the GClass to which the state is being added.
* - `state_name`
  - [`gobj_state_t`](gobj_state_t)
  - The name of the state to add to the FSM.
:::

---

**Return Value**

- `0`: The state was successfully added.
- `-1`: The state could not be added (e.g., if it already exists).

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
names = [
    "gobj_save_persistent_attrs",
    "gobj_remove_persistent_attrs",
    "gobj_list_persistent_attrs",
    "gclass_attr_desc",
    "gobj_attr_desc",
    "gobj_attr_type",
    "gobj_hsdata",
    "gclass_authz_desc",
    "gclass_has_attr",
    "gobj_has_attr",
    "gobj_is_readable_attr",
    "gobj_is_writable_attr",
    "gobj_reset_volatil_attrs",
    "gobj_reset_rstats_attrs",
    "gobj_read_attr",
    "gobj_read_attrs",
    "gobj_read_user_data",
    "gobj_write_attr",
    "gobj_write_attrs",
    "gobj_write_user_data",
    "gobj_hsdata2",
    "gobj_has_bottom_attr",
    "gobj_read_str_attr",
    "gobj_read_bool_attr",
    "gobj_read_integer_attr",
    "gobj_read_real_attr",
    "gobj_read_json_attr",
    "gobj_read_pointer_attr",
    "gobj_write_str_attr",
    "gobj_write_bool_attr",
    "gobj_write_integer_attr",
    "gobj_write_real_attr",
    "gobj_write_json_attr",
    "gobj_write_new_json_attr",
    "gobj_write_pointer_attr",

]

# Loop through the list of names and create a file for each
for name in names:
    # Substitute the variable in the template
    formatted_text = template.substitute(_name_=name)

    # Create a unique file name for each name
    file_name = f"{name.lower()}.md"

    # Check if the file already exists
    if os.path.exists(file_name):
        print(f"File {file_name} already exists. Skipping...")
        continue  # Skip to the next name

    # Write the formatted text to the file
    with open(file_name, "w") as file:
        file.write(formatted_text)

    print(f"File created: {file_name}")
