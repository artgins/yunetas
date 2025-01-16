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
        "name": "udpc_start_up",
        "description": '''
Initialize the UDP communication system.
        ''',
        "prototype": '''
PUBLIC int udpc_start_up(void);
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
        "name": "udpc_end",
        "description": '''
Shut down the UDP communication system.
        ''',
        "prototype": '''
PUBLIC void udpc_end(void);
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
No return value. This function shuts down and cleans up resources used by the UDP communication system.
        '''
    },
    {
        "name": "udpc_open",
        "description": '''
Open a UDP communication channel.
        ''',
        "prototype": '''
PUBLIC int udpc_open(
    const char  *address,
    int          port
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `address`
  - `const char *`
  - The IP address to bind or connect to.

* - `port`
  - `int`
  - The port number for the UDP communication.
:::
        ''',
        "return_value": '''
Returns a handle to the UDP channel on success, or a negative value on failure.
        '''
    },
    {
        "name": "udpc_close",
        "description": '''
Close a UDP communication channel.
        ''',
        "prototype": '''
PUBLIC void udpc_close(
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
  - The handle of the UDP channel to close.
:::
        ''',
        "return_value": '''
No return value. This function closes the specified UDP channel.
        '''
    },
    {
        "name": "udpc_write",
        "description": '''
Send data through a UDP communication channel.
        ''',
        "prototype": '''
PUBLIC int udpc_write(
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
  - The handle of the UDP channel to write to.

* - `data`
  - `const void *`
  - The data to send.

* - `size`
  - `size_t`
  - The size of the data in bytes.
:::
        ''',
        "return_value": '''
Returns the number of bytes successfully sent, or a negative value on failure.
        '''
    },
    {
        "name": "udpc_fwrite",
        "description": '''
Send the contents of a file through a UDP communication channel.
        ''',
        "prototype": '''
PUBLIC int udpc_fwrite(
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
  - The handle of the UDP channel to write to.

* - `file`
  - `FILE *`
  - The file to send through the UDP channel.
:::
        ''',
        "return_value": '''
Returns the number of bytes successfully sent, or a negative value on failure.
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
