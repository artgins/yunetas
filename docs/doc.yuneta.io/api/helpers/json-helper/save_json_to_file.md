<!-- ============================================================== -->
(save_json_to_file())=
# `save_json_to_file()`
<!-- ============================================================== -->


The `save_json_to_file` function is responsible for saving a JSON object to a specified file within a given directory. It allows for the specification of file permissions and provides options for file creation and read-only access. The function takes ownership of the JSON data, meaning that it will manage the memory associated with it, and the caller should not attempt to free it after the function call.


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
    hgobj   gobj,
    const char *directory,
    const char *filename,
    int xpermission,
    int rpermission,
    log_opt_t on_critical_error,
    BOOL create,        // Create file if not exists or overwrite.
    BOOL only_read,
    json_t *jn_data     // owned
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
  - The handle to the gobj that is performing the operation.

* - `directory`
  - `const char *`
  - The path to the directory where the file will be saved.

* - `filename`
  - `const char *`
  - The name of the file to which the JSON data will be saved.

* - `xpermission`
  - `int`
  - The permission settings for the file being created.

* - `rpermission`
  - `int`
  - The permission settings for the file's read access.

* - `on_critical_error`
  - `log_opt_t`
  - The logging option to use in case of critical errors during the operation.

* - `create`
  - `BOOL`
  - Indicates whether to create the file if it does not exist or to overwrite it if it does.

* - `only_read`
  - `BOOL`
  - Specifies if the file should be opened in read-only mode.

* - `jn_data`
  - `json_t *`
  - The JSON data to be saved, which is owned by this function.
:::


---

**Return Value**


The function returns an integer value indicating the success or failure of the operation. A return value of 0 typically indicates success, while a negative value indicates an error occurred during the file save process.


**Notes**


This function will log errors based on the `on_critical_error` parameter. If the `create` parameter is set to TRUE, the function will create the file if it does not already exist. If the file exists and `create` is FALSE, the function may overwrite the existing file based on the specified permissions.


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

