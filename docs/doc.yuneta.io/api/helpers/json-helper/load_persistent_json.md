<!-- ============================================================== -->
(load_persistent_json())=
# `load_persistent_json()`
<!-- ============================================================== -->


The `load_persistent_json` function is designed to load a JSON object from a specified file within a given directory. It provides options for handling file access, including the ability to open the file exclusively or to silence error logging. 

This function is particularly useful for applications that require persistent storage of JSON data, allowing for easy retrieval and manipulation of configuration or state information stored in JSON format. If the file cannot be accessed or if there are critical errors during the loading process, the behavior of the function can be controlled via the `on_critical_error` parameter.


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

PUBLIC json_t *load_persistent_json(
    hgobj         gobj,
    const char   *directory,
    const char   *filename,
    log_opt_t     on_critical_error,
    int          *pfd,
    BOOL          exclusive,
    BOOL          silence  // HACK to silence TRUE you MUST set on_critical_error=LOG_NONE
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
  - The handle to the gobj that is requesting the JSON load operation.

* - `directory`
  - `const char *`
  - The path to the directory where the JSON file is located.

* - `filename`
  - `const char *`
  - The name of the JSON file to be loaded.

* - `on_critical_error`
  - `log_opt_t`
  - Specifies the logging behavior in case of critical errors during the loading process.

* - `pfd`
  - `int *`
  - A pointer to an integer where the file descriptor will be stored if the file is opened exclusively.

* - `exclusive`
  - `BOOL`
  - Indicates whether the file should be opened in exclusive mode.

* - `silence`
  - `BOOL`
  - If set to TRUE, suppresses error messages; requires `on_critical_error` to be set to `LOG_NONE`.
:::


---

**Return Value**


Returns a pointer to a `json_t` object representing the loaded JSON data. If the loading fails, it may return NULL, depending on the error handling specified by the `on_critical_error` parameter.


**Notes**


This function has a special behavior when the `silence` parameter is set to TRUE; in this case, the `on_critical_error` must be set to `LOG_NONE` to avoid logging any errors. This allows for silent failure modes in scenarios where error reporting is not desired.


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

