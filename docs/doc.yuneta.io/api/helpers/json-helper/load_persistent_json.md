<!-- ============================================================== -->
(load_persistent_json())=
# `load_persistent_json()`
<!-- ============================================================== -->


The `load_persistent_json()` function loads a JSON object from a file located in the specified directory and filename. 
It provides options for handling file access, error logging, and exclusive file locking. If the `exclusive` parameter 
is set to `TRUE`, the file remains open and its file descriptor is returned via `pfd`. If `silence` is set to `TRUE`, 
the function suppresses error messages, but this requires `on_critical_error` to be set to `LOG_NONE`. 
The function is designed to handle persistent JSON data efficiently.


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
    hgobj       gobj,
    const char  *directory,
    const char  *filename,
    log_opt_t   on_critical_error,
    int         *pfd,
    BOOL        exclusive,
    BOOL        silence
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
  - The gobj (generic object) that invokes the function.

* - `directory`
  - `const char *`
  - The directory path where the JSON file is located.

* - `filename`
  - `const char *`
  - The name of the JSON file to load.

* - `on_critical_error`
  - `log_opt_t`
  - Logging option to handle critical errors during file operations.

* - `pfd`
  - `int *`
  - Pointer to an integer where the file descriptor will be stored if `exclusive` is `TRUE`.

* - `exclusive`
  - `BOOL`
  - If `TRUE`, the file remains open and its file descriptor is returned.

* - `silence`
  - `BOOL`
  - If `TRUE`, suppresses error messages. Requires `on_critical_error` to be `LOG_NONE`.

:::


---

**Return Value**


Returns a `json_t *` object representing the loaded JSON data. If the file cannot be loaded or parsed, 
the function returns `NULL`.


**Notes**


- If `exclusive` is `TRUE`, remember to close the file descriptor stored in `pfd` when it is no longer needed.
- If `silence` is set to `TRUE`, ensure that `on_critical_error` is set to `LOG_NONE` to avoid conflicts.
- The function is designed to handle persistent JSON data, making it suitable for scenarios where JSON configuration 
  or state needs to be retained across sessions.


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

