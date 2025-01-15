#!/usr/bin/python

from string import Template
import os

template = Template("""

<!-- ============================================================== -->
($_name_())=
# `$_name_()`
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
    "gobj_start(hgobj gobj",
    "gobj_start_childs",
    "gobj_start_tree",
    "gobj_stop",
    "gobj_stop_childs",
    "gobj_stop_tree",
    "gobj_play",
    "gobj_pause",
    "gobj_enable",
    "gobj_disable",
    "gobj_change_parent",
    "gobj_autostart_services",
    "gobj_autoplay_services",
    "gobj_stop_autostart_services",
    "gobj_pause_autoplay_services",
    "gobj_command",
    "gobj_stats",
    "build_command_response",
    "gobj_set_bottom_gobj",
    "gobj_last_bottom_gobj",
    "gobj_bottom_gobj",
    "gobj_services",
    "gobj_default_service",
    "gobj_find_service",
    "gobj_find_service_by_gclass",
    "gobj_find_gclass_service",
    "gobj_find_gobj",
    "gobj_first_child",
    "gobj_last_child",
    "gobj_next_child",
    "gobj_prev_child",
    "gobj_child_by_name",
    "gobj_child_size",
    "gobj_child_size2",
    "gobj_search_path",
    "gobj_match_gobj",
    "gobj_find_child",
    "gobj_walk_gobj_childs",
    "gobj_walk_gobj_childs_tree",
    "gobj_exec_internal_method",
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
