<!-- ============================================================== -->
(save_json_to_file())=
# `save_json_to_file()`
<!-- ============================================================== -->


This function saves a JSON data structure to a file in the specified directory with the given filename. It provides options for setting permissions, handling errors, and controlling file creation and read-only access.


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
  - The gobj instance.
* - `directory`
  - `const char *`
  - The directory path where the file will be saved.
* - `filename`
  - `const char *`
  - The name of the file to be saved.
* - `xpermission`
  - `int`
  - The execution permission for the file.
* - `rpermission`
  - `int`
  - The read permission for the file.
* - `on_critical_error`
  - `log_opt_t`
  - Log options for critical errors.
* - `create`
  - `BOOL`
  - Flag to create the file if it does not exist or overwrite it.
* - `only_read`
  - `BOOL`
  - Flag to set the file as read-only.
* - `jn_data`
  - `json_t *`
  - The JSON data to be saved to the file.
:::


---

**Return Value**


Returns an integer indicating the success or failure of the operation.


**Notes**


- The function will handle critical errors based on the provided `on_critical_error` parameter.
- The `create` flag determines whether the file should be created if it does not exist or overwritten if it does.
- The `only_read` flag allows setting the file as read-only.


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

