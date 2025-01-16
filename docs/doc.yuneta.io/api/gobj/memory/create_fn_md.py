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
        "name": "gobj_set_allocators",
        "description": "Sets custom memory allocation functions.",
        "prototype": """
PUBLIC int gobj_set_allocators(
    sys_malloc_fn_t malloc_func,
    sys_realloc_fn_t realloc_func,
    sys_calloc_fn_t calloc_func,
    sys_free_fn_t free_func
);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `malloc_func`
  - `sys_malloc_fn_t`
  - Pointer to the custom malloc function.

* - `realloc_func`
  - `sys_realloc_fn_t`
  - Pointer to the custom realloc function.

* - `calloc_func`
  - `sys_calloc_fn_t`
  - Pointer to the custom calloc function.

* - `free_func`
  - `sys_free_fn_t`
  - Pointer to the custom free function.
:::
        """,
        "return_value": """
- `0`: Success.
        """
    },
    {
        "name": "gobj_get_allocators",
        "description": "Retrieves the current memory allocation functions.",
        "prototype": """
PUBLIC int gobj_get_allocators(
    sys_malloc_fn_t *malloc_func,
    sys_realloc_fn_t *realloc_func,
    sys_calloc_fn_t *calloc_func,
    sys_free_fn_t *free_func
);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `malloc_func`
  - `sys_malloc_fn_t *`
  - Pointer to store the current malloc function.

* - `realloc_func`
  - `sys_realloc_fn_t *`
  - Pointer to store the current realloc function.

* - `calloc_func`
  - `sys_calloc_fn_t *`
  - Pointer to store the current calloc function.

* - `free_func`
  - `sys_free_fn_t *`
  - Pointer to store the current free function.
:::
        """,
        "return_value": """
- `0`: Success.
        """
    },
    {
        "name": "gobj_malloc_func",
        "description": "Returns the current malloc function.",
        "prototype": """
PUBLIC sys_malloc_fn_t gobj_malloc_func(void);
        """,
        "parameters": "None.",
        "return_value": """
- Returns the current malloc function.
        """
    },
    {
        "name": "gobj_realloc_func",
        "description": "Returns the current realloc function.",
        "prototype": """
PUBLIC sys_realloc_fn_t gobj_realloc_func(void);
        """,
        "parameters": "None.",
        "return_value": """
- Returns the current realloc function.
        """
    },
    {
        "name": "gobj_calloc_func",
        "description": "Returns the current calloc function.",
        "prototype": """
PUBLIC sys_calloc_fn_t gobj_calloc_func(void);
        """,
        "parameters": "None.",
        "return_value": """
- Returns the current calloc function.
        """
    },
    {
        "name": "gobj_free_func",
        "description": "Returns the current free function.",
        "prototype": """
PUBLIC sys_free_fn_t gobj_free_func(void);
        """,
        "parameters": "None.",
        "return_value": """
- Returns the current free function.
        """
    },
    {
        "name": "gobj_strndup",
        "description": "Duplicates a string with a maximum size.",
        "prototype": """
PUBLIC char *gobj_strndup(const char *str, size_t size);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `str`
  - `const char *`
  - The string to duplicate.

* - `size`
  - `size_t`
  - The maximum number of characters to copy.
:::
        """,
        "return_value": """
- Returns a pointer to the duplicated string or `NULL` on failure.
        """
    },
    {
        "name": "gobj_strdup",
        "description": "Duplicates a string.",
        "prototype": """
PUBLIC char *gobj_strdup(const char *string);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `string`
  - `const char *`
  - The string to duplicate.
:::
        """,
        "return_value": """
- Returns a pointer to the duplicated string or `NULL` on failure.
        """
    },
    {
        "name": "gobj_get_maximum_block",
        "description": "Returns the maximum block size for memory allocation.",
        "prototype": """
PUBLIC size_t gobj_get_maximum_block(void);
        """,
        "parameters": "None.",
        "return_value": """
- Returns the maximum memory block size in bytes.
        """
    },
    {
        "name": "print_track_mem",
        "description": "Prints a list of tracked memory blocks for debugging.",
        "prototype": """
PUBLIC void print_track_mem(void);
        """,
        "parameters": "None.",
        "return_value": "None."
    },
    {
        "name": "set_memory_check_list",
        "description": "Sets a list of memory references to check for tracking.",
        "prototype": """
PUBLIC void set_memory_check_list(unsigned long *memory_check_list);
        """,
        "parameters": """
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `memory_check_list`
  - `unsigned long *`
  - Pointer to a list of memory references to check.
:::
        """,
        "return_value": "None."
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
