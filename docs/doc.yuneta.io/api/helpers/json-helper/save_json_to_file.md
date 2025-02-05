<!-- ============================================================== -->
(save_json_to_file())=
# `save_json_to_file()`
<!-- ============================================================== -->


The `save_json_to_file()` function saves a JSON object to a file in the specified directory with the given filename. 
It allows for setting file permissions, handling critical errors, and controlling file creation or overwriting behavior. 
The JSON data is passed as an owned object, meaning the caller relinquishes ownership of the JSON object to the function.


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

int save_json_to_file(
    hgobj       gobj,
    const char  *directory,
    const char  *filename,
    int         xpermission,
    int         rpermission,
    log_opt_t   on_critical_error,
    BOOL        create,
    BOOL        only_read,
    json_t      *jn_data
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The gobj context for logging or error handling.

* - `directory`
  - `const char *`
  - The directory where the file will be saved.

* - `filename`
  - `const char *`
  - The name of the file to save the JSON data.

* - `xpermission`
  - `int`
  - The file's execute permissions.

* - `rpermission`
  - `int`
  - The file's read permissions.

* - `on_critical_error`
  - `log_opt_t`
  - Specifies the logging behavior in case of critical errors.

* - `create`
  - `BOOL`
  - If `TRUE`, the function creates the file if it does not exist or overwrites it if it does.

* - `only_read`
  - `BOOL`
  - If `TRUE`, the function ensures the file is only readable.

* - `jn_data`
  - `json_t *`
  - The JSON object to be saved. Ownership is transferred to the function.
:::


---

**Return Value**


Returns `0` on success or `-1` on failure. Errors may occur due to invalid paths, permission issues, or critical errors during file operations.


**Notes**


- The function assumes ownership of the `jn_data` JSON object. The caller should not use or free the object after calling this function.
- If `on_critical_error` is set to `LOG_NONE`, the function will suppress error logging.
- Ensure the directory exists before calling this function, as it does not create directories.


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

