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
        "name": "gobj_authenticate",
        "description": '''
Authenticates a user or session based on the provided parameters. This function validates the user's identity and returns relevant authentication data.
        ''',
        "prototype": '''
json_t *gobj_authenticate(hgobj gobj, json_t *kw, hgobj src);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj performing the authentication.

* - `kw`
  - [`json_t *`](json_t)
  - JSON object containing authentication credentials or parameters. Ownership is transferred to the function.

* - `src`
  - [`hgobj`](hgobj)
  - Handle to the source GObj initiating the authentication.

:::
        ''',
        "return_value": '''
- Returns a JSON object ([`json_t`](json_t)) containing authentication results or metadata.  
- Returns `NULL` if authentication fails.
        '''
    },
    {
        "name": "gobj_authzs",
        "description": '''
Retrieves the authorization rules for the specified GObj, providing a list of all available authorizations.
        ''',
        "prototype": '''
json_t *gobj_authzs(hgobj gobj);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj whose authorization rules are being queried.

:::
        ''',
        "return_value": '''
- Returns a JSON array ([`json_t`](json_t)) containing all available authorization rules.  
- Returns an empty array if no rules are defined.
        '''
    },
    {
        "name": "gobj_authz",
        "description": '''
Checks if a specific authorization rule applies to the specified GObj. This function validates whether a certain action is permitted.
        ''',
        "prototype": '''
BOOL gobj_authz(hgobj gobj, const char *authz, json_t *kw, hgobj src);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj being checked.

* - `authz`
  - `const char *`
  - The name of the authorization rule to check.

* - `kw`
  - [`json_t *`](json_t)
  - JSON object containing additional parameters for the authorization check.

* - `src`
  - [`hgobj`](hgobj)
  - Handle to the source GObj requesting the authorization check.

:::
        ''',
        "return_value": '''
- Returns `TRUE` if the authorization rule applies to the GObj.  
- Returns `FALSE` otherwise.
        '''
    },
    {
        "name": "gobj_user_has_authz",
        "description": '''
Checks if a user associated with the specified GObj has the given authorization. This function validates user permissions.
        ''',
        "prototype": '''
BOOL gobj_user_has_authz(hgobj gobj, const char *authz);
        ''',
        "parameters": '''
:::{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `gobj`
  - [`hgobj`](hgobj)
  - Handle to the GObj associated with the user.

* - `authz`
  - `const char *`
  - The name of the authorization rule to check.

:::
        ''',
        "return_value": '''
- Returns `TRUE` if the user has the specified authorization.  
- Returns `FALSE` otherwise.
        '''
    },
    {
        "name": "gobj_get_global_authz_table",
        "description": '''
Retrieves the global authorization table, which lists all available authorization rules across the system.
        ''',
        "prototype": '''
const sdata_desc_t *gobj_get_global_authz_table(void);
        ''',
        "parameters": '''
(No parameters for this function)
        ''',
        "return_value": '''
- Returns a pointer to the global authorization table ([`sdata_desc_t`](sdata_desc_t)) containing all defined authorization rules.
        '''
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
