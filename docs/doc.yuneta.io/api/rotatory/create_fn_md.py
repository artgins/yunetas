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

functions_documentation = [
]
functions_documentation.extend([
    {
        "name": "rotatory_start_up",
        "description": '''
Initialize the rotatory logging system.
        ''',
        "prototype": '''
PUBLIC int rotatory_start_up(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "rotatory_end",
        "description": '''
Shut down the rotatory logging system.
        ''',
        "prototype": '''
PUBLIC void rotatory_end(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `-`
  - `-`
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
No return value. This function shuts down and cleans up resources used by the rotatory logging system.
        '''
    },
    {
        "name": "rotatory_open",
        "description": '''
Open a rotatory log file.
        ''',
        "prototype": '''
PUBLIC int rotatory_open(
    const char  *base_path,
    size_t       max_size,
    int          max_files
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `base_path`
  - `const char *`
  - The base path for the log files.

* - `max_size`
  - `size_t`
  - The maximum size of each log file.

* - `max_files`
  - `int`
  - The maximum number of log files to keep.
:::
        ''',
        "return_value": '''
Returns a handle to the rotatory log on success, or a negative value on failure.
        '''
    },
    {
        "name": "rotatory_close",
        "description": '''
Close a rotatory log file.
        ''',
        "prototype": '''
PUBLIC void rotatory_close(
    int handle
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `handle`
  - `int`
  - The handle of the rotatory log to close.
:::
        ''',
        "return_value": '''
No return value. This function closes the specified rotatory log file.
        '''
    },
    {
        "name": "rotatory_write",
        "description": '''
Write data to a rotatory log file.
        ''',
        "prototype": '''
PUBLIC int rotatory_write(
    int          handle,
    const void  *data,
    size_t       size
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `handle`
  - `int`
  - The handle of the rotatory log to write to.

* - `data`
  - `const void *`
  - The data to write.

* - `size`
  - `size_t`
  - The size of the data in bytes.
:::
        ''',
        "return_value": '''
Returns the number of bytes successfully written, or a negative value on failure.
        '''
    },
    {
        "name": "rotatory_fwrite",
        "description": '''
Write the contents of a file to a rotatory log file.
        ''',
        "prototype": '''
PUBLIC int rotatory_fwrite(
    int          handle,
    FILE        *file
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `handle`
  - `int`
  - The handle of the rotatory log to write to.

* - `file`
  - `FILE *`
  - The file to write to the rotatory log.
:::
        ''',
        "return_value": '''
Returns the number of bytes successfully written, or a negative value on failure.
        '''
    },
    {
        "name": "rotatory_trunk",
        "description": '''
Truncate a rotatory log file, clearing its contents.
        ''',
        "prototype": '''
PUBLIC int rotatory_trunk(
    int handle
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `handle`
  - `int`
  - The handle of the rotatory log to truncate.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "rotatory_flush",
        "description": '''
Flush the data of a rotatory log file to disk.
        ''',
        "prototype": '''
PUBLIC int rotatory_flush(
    int handle
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `handle`
  - `int`
  - The handle of the rotatory log to flush.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "rotatory_path",
        "description": '''
Get the path of the currently active file in a rotatory log.
        ''',
        "prototype": '''
PUBLIC const char *rotatory_path(
    int handle
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `handle`
  - `int`
  - The handle of the rotatory log to query.
:::
        ''',
        "return_value": '''
Returns a string containing the path of the active file, or `NULL` on failure.
        '''
    }
])


# Loop through the list of names and create a file for each
for fn in functions_documentation:
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
