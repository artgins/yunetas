<!-- ============================================================== -->
(load_persistent_json)=
# `load_persistent_json()`
<!-- ============================================================== -->

The function `load_persistent_json()` loads a JSON object from a file in a specified directory. It supports exclusive access by keeping the file open if requested.

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
json_t *load_persistent_json(
    hgobj gobj,
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error,
    int *pfd,
    BOOL exclusive,
    BOOL silence
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
  - A handle to the gobj (generic object) that is requesting the JSON load.

* - `directory`
  - `const char *`
  - The directory where the JSON file is located.

* - `filename`
  - `const char *`
  - The name of the JSON file to be loaded.

* - `on_critical_error`
  - `log_opt_t`
  - Logging options to handle critical errors during file operations.

* - `pfd`
  - `int *`
  - Pointer to an integer where the file descriptor will be stored if `exclusive` is `TRUE`. If `exclusive` is `FALSE`, the file is closed after reading.

* - `exclusive`
  - `BOOL`
  - If `TRUE`, the file remains open and its descriptor is stored in `pfd`. If `FALSE`, the file is closed after reading.

* - `silence`
  - `BOOL`
  - If `TRUE`, suppresses error logging when the file does not exist.
:::

---

**Return Value**

Returns a `json_t *` representing the loaded JSON object. Returns `NULL` if the file does not exist or if an error occurs.

**Notes**

If `exclusive` is `TRUE`, the caller is responsible for closing the file descriptor stored in `pfd`. The function logs errors unless `silence` is `TRUE` and `on_critical_error` is set to `LOG_NONE`.

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
