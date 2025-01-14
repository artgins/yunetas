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
    "gbuffer_create",
    "gbuffer_remove",
    "gbuffer_incref",
    "gbuffer_decref",
    "gbuffer_cur_rd_pointer",
    "gbuffer_reset_rd",
    "gbuffer_set_rd_offset",
    "gbuffer_ungetc",
    "gbuffer_get_rd_offset",
    "gbuffer_get",
    "gbuffer_getchar",
    "gbuffer_chunk",
    "gbuffer_getline",
    "gbuffer_cur_wr_pointer",
    "gbuffer_reset_wr",
    "gbuffer_set_wr",
    "gbuffer_append",
    "gbuffer_append_string",
    "gbuffer_append_char",
    "gbuffer_append_gbuf",
    "gbuffer_printf",
    "gbuffer_vprintf",
    "gbuffer_head_pointer",
    "gbuffer_clear",
    "gbuffer_leftbytes",
    "gbuffer_totalbytes",
    "gbuffer_freebytes",
    "gbuffer_setlabel",
    "gbuffer_getlabel",
    "gbuffer_setmark",
    "gbuffer_getmark",
    "gbuffer_serialize",
    "gbuffer_deserialize",
    "gbuffer_string_to_base64",
    "gbuffer_base64_to_string",
    "json2gbuf",
    "gbuf2json",
    "gobj_trace_dump_gbuf",
    "gobj_trace_dump_full_gbuf",

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
