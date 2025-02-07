<!-- ============================================================== -->
(save_json_to_file())=
# `save_json_to_file()`
<!-- ============================================================== -->

The function `save_json_to_file()` saves a JSON object to a file, creating the directory if necessary and applying the specified permissions.

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
    hgobj gobj,
    const char *directory,
    const char *filename,
    int xpermission,
    int rpermission,
    log_opt_t on_critical_error,
    BOOL create,
    BOOL only_read,
    json_t *jn_data
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
  - The GObj context, used for logging and error handling.

* - `directory`
  - `const char *`
  - The directory where the JSON file will be saved.

* - `filename`
  - `const char *`
  - The name of the JSON file to be created or overwritten.

* - `xpermission`
  - `int`
  - The permission mode for the directory (e.g., 02770).

* - `rpermission`
  - `int`
  - The permission mode for the file (e.g., 0660).

* - `on_critical_error`
  - `log_opt_t`
  - Logging options for handling critical errors.

* - `create`
  - `BOOL`
  - If `TRUE`, the function will create the directory and file if they do not exist.

* - `only_read`
  - `BOOL`
  - If `TRUE`, the file will be set to read-only mode after writing.

* - `jn_data`
  - `json_t *`
  - The JSON object to be saved to the file. This parameter is owned by the function.
:::

---

**Return Value**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

The function ensures that the directory exists before saving the file. If `only_read` is `TRUE`, the file permissions are set to read-only after writing.

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

