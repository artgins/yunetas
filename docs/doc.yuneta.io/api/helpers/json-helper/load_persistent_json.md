<!-- ============================================================== -->
(load_persistent_json())=
# `load_persistent_json()`
<!-- ============================================================== -->


The `load_persistent_json` function is designed to load a JSON object from a specified file within a directory. It allows for options to handle file locking and error logging. The function can open the file exclusively or just read it, depending on the `exclusive` parameter. If an error occurs during the loading process, the behavior is determined by the `on_critical_error` parameter.


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
  - The handle to the gobj that is requesting the JSON load.

* - `directory`
  - `const char *`
  - The path to the directory where the JSON file is located.

* - `filename`
  - `const char *`
  - The name of the JSON file to be loaded.

* - `on_critical_error`
  - `log_opt_t`
  - Logging option that determines the behavior on critical errors.

* - `pfd`
  - `int *`
  - Pointer to an integer where the file descriptor will be stored if opened.

* - `exclusive`
  - `BOOL`
  - If TRUE, the file will be opened exclusively.

* - `silence`
  - `BOOL`
  - If TRUE, suppresses error logging (requires `on_critical_error` to be set to `LOG_NONE`).

:::


---

**Return Value**


Returns a pointer to a `json_t` object representing the loaded JSON data. If the loading fails, it returns NULL.


**Notes**


If the `silence` parameter is set to TRUE, the function will not log errors, which can lead to silent failures if not handled properly.


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

