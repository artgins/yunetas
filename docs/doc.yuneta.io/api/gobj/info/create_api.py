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


---

**Return Value**


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
    "gobj_yuno",
    "gobj_yuno_name",
    "gobj_yuno_role",
    "gobj_yuno_id",
    "gobj_yuno_tag",
    "gobj_yuno_role_plus_name",
    "gobj_yuno_realm_id",
    "gobj_yuno_realm_owner",
    "gobj_yuno_realm_role",
    "gobj_yuno_realm_name",
    "gobj_yuno_realm_env",
    "gobj_yuno_node_owner",
    "gobj_name",
    "gobj_gclass_name",
    "gobj_gclass",
    "gobj_full_name",
    "gobj_short_name",
    "gobj_global_variables",
    "gobj_priv_data",
    "gobj_parent",
    "gobj_is_destroying",
    "gobj_is_running",
    "gobj_is_playing",
    "gobj_is_service",
    "gobj_is_disabled",
    "gobj_is_volatil",
    "gobj_set_volatil",
    "gobj_is_pure_child",

    "gobj_typeof_gclass",
    "gobj_typeof_inherited_gclass",
    "get_max_system_memory",
    "get_cur_system_memory",

    "gclass_command_desc",
    "gobj_command_desc",

    "get_sdata_flag_table",
    "get_sdata_flag_desc",

    "get_attrs_schema",

    "gclass2json",

    "gobj2json",
    "view_gobj_tree",

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
