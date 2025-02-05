<!-- ============================================================== -->
(save_json_to_file())=
# `save_json_to_file()`
<!-- ============================================================== -->


The `save_json_to_file` function saves a JSON object to a specified file. It allows for setting file permissions and can create the file if it does not exist. The function takes ownership of the JSON data, meaning it is responsible for its memory management.


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
    BOOL create,
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
  - The handle to the gobj that is calling this function.

* - `directory`
  - `const char *`
  - The directory where the file will be saved.

* - `filename`
  - `const char *`
  - The name of the file to which the JSON data will be saved.

* - `xpermission`
  - `int`
  - The permission settings for the file.

* - `rpermission`
  - `int`
  - The permission settings for the directory.

* - `on_critical_error`
  - `log_opt_t`
  - Logging options for critical errors that may occur during the operation.

* - `create`
  - `BOOL`
  - Indicates whether to create the file if it does not exist or overwrite it if it does.

* - `only_read`
  - `BOOL`
  - If TRUE, the function will only read the file if it exists and not create a new one.

* - `jn_data`
  - `json_t *`
  - The JSON data to be saved, which is owned by the function.
:::


---

**Return Value**


The function returns an integer indicating the success or failure of the operation. A return value of 0 typically indicates success, while a negative value indicates an error occurred.


**Notes**


This function takes ownership of the `jn_data` parameter, meaning that it is responsible for freeing the memory associated with it after use. Ensure that the JSON object is not used after this function call unless it is reallocated.


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

