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
        "name": "capture_log_write",
        "description": '''
Write a message to the capture log.
        ''',
        "prototype": '''
PUBLIC int capture_log_write(
    const char  *message,
    const char  *log_level
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `message`
  - `const char *`
  - The message to write to the capture log.

* - `log_level`
  - `const char *`
  - The log level of the message (e.g., `info`, `error`).
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "set_expected_results",
        "description": '''
Set the expected results for a test case.
        ''',
        "prototype": '''
PUBLIC void set_expected_results(
    const char  *test_name,
    const char  *expected_result
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `test_name`
  - `const char *`
  - The name of the test case.

* - `expected_result`
  - `const char *`
  - The expected result for the test case.
:::
        ''',
        "return_value": '''
No return value. This function sets the expected results for the specified test case.
        '''
    },
    {
        "name": "test_json_file",
        "description": '''
Test if a JSON file meets specific conditions. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int test_json_file(
    const char  *path,
    int          verbose
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path to the JSON file.

* - `verbose`
  - `int`
  - The verbosity level for the test output.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "test_json",
        "description": '''
Test if a JSON object meets specific conditions. Works with [`json_t *`](json_t).
        ''',
        "prototype": '''
PUBLIC int test_json(
    json_t      *json,
    int          verbose
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `json`
  - [`json_t *`](json_t)
  - The JSON object to test.

* - `verbose`
  - `int`
  - The verbosity level for the test output.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "test_directory_permission",
        "description": '''
Test if a directory has the specified permissions.
        ''',
        "prototype": '''
PUBLIC int test_directory_permission(
    const char  *path,
    int          mode
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path to the directory.

* - `mode`
  - `int`
  - The permission mode to check (e.g., readable, writable).
:::
        ''',
        "return_value": '''
Returns `0` if the directory has the specified permissions, or a negative value otherwise.
        '''
    },
    {
        "name": "test_file_permission_and_size",
        "description": '''
Test if a file has the specified permissions and meets size requirements.
        ''',
        "prototype": '''
PUBLIC int test_file_permission_and_size(
    const char  *path,
    int          mode,
    size_t       min_size,
    size_t       max_size
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path to the file.

* - `mode`
  - `int`
  - The permission mode to check (e.g., readable, writable).

* - `min_size`
  - `size_t`
  - The minimum allowed size of the file in bytes.

* - `max_size`
  - `size_t`
  - The maximum allowed size of the file in bytes.
:::
        ''',
        "return_value": '''
Returns `0` if the file meets the specified permissions and size requirements, or a negative value otherwise.
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
