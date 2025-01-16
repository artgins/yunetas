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
    "glog_init",
    "glog_end",
    "gobj_log_register_handler",
    "gobj_log_exist_handler",
    "gobj_log_add_handler",
    "gobj_log_del_handler",
    "gobj_log_list_handlers",
    "gobj_log_set_global_handler_option",
    "stdout_write",
    "stdout_fwrite",
    "gobj_log_alert",
    "gobj_log_critical",
    "gobj_log_error",
    "gobj_log_warning",
    "gobj_log_info",
    "gobj_log_debug",
    "gobj_get_log_priority_name",
    "gobj_get_log_data",
    "gobj_log_clear_counters",
    "gobj_log_last_message",
    "gobj_log_set_last_message",
    "set_show_backtrace_fn",
    "print_backtrace",
    "trace_vjson",
    "gobj_trace_msg",
    "gobj_info_msg",
    "gobj_trace_json",
    "gobj_trace_buffer",
    "gobj_trace_dump",
    "print_error",
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
