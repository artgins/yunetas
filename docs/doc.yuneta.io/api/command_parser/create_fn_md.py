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
        "name": "command_parser",
        "description": '''
Parse a command and its arguments based on a predefined command structure.
        ''',
        "prototype": '''
PUBLIC int command_parser(
    const char  *command_line,
    const char  *commands[],
    int         (*callback)(const char *command, const char *args, void *user_data),
    void        *user_data
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `command_line`
  - `const char *`
  - The full command line input to parse.

* - `commands`
  - `const char *[]`
  - An array of valid command strings.

* - `callback`
  - `int (*)(const char *, const char *, void *)`
  - A function pointer to process each parsed command and its arguments.

* - `user_data`
  - `void *`
  - User-defined data passed to the callback function.
:::
        ''',
        "return_value": '''
Returns `0` on success, or a negative value on failure.
        '''
    },
    {
        "name": "gobj_build_cmds_doc",
        "description": '''
Build documentation for all available commands in a GObj.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_build_cmds_doc(
    hgobj       gobj,
    const char *topic
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The GObj to retrieve command documentation from.

* - `topic`
  - `const char *`
  - The topic to filter commands by, or `NULL` for all commands.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) object containing the documentation for the commands, or `NULL` on failure.
        '''
    },
    {
        "name": "authzs_list",
        "description": '''
Retrieve the list of authorization levels.
        ''',
        "prototype": '''
PUBLIC json_t *authzs_list(void);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - (none)
  - (none)
  - This function does not take any parameters.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) array containing the list of authorization levels.
        '''
    },
    {
        "name": "authz_get_level_desc",
        "description": '''
Retrieve the description of a specific authorization level.
        ''',
        "prototype": '''
PUBLIC const char *authz_get_level_desc(
    const char  *level
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `level`
  - `const char *`
  - The authorization level to retrieve the description for.
:::
        ''',
        "return_value": '''
Returns a string containing the description of the authorization level, or `NULL` if the level is not found.
        '''
    },
    {
        "name": "gobj_build_authzs_doc",
        "description": '''
Build documentation for all available authorizations in a GObj.
        ''',
        "prototype": '''
PUBLIC json_t *gobj_build_authzs_doc(
    hgobj       gobj,
    const char *topic
);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The GObj to retrieve authorization documentation from.

* - `topic`
  - `const char *`
  - The topic to filter authorizations by, or `NULL` for all authorizations.
:::
        ''',
        "return_value": '''
Returns a [`json_t *`](json_t) object containing the documentation for the authorizations, or `NULL` on failure.
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
