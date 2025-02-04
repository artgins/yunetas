<!-- ============================================================== -->
(load_persistent_json())=
# `load_persistent_json()`
<!-- ============================================================== -->


This function loads a persistent JSON file from the specified directory and filename. It handles exclusive file opening and critical errors.

If the file is successfully loaded, it returns the JSON data. If an error occurs, it handles the error according to the provided options.


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
    hgobj   gobj,
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
  - The gobj handle for the function context.

* - `directory`
  - `const char *`
  - The directory path where the JSON file is located.

* - `filename`
  - `const char *`
  - The name of the JSON file to load.

* - `on_critical_error`
  - `log_opt_t`
  - Log options for critical errors.

* - `pfd`
  - `int *`
  - Pointer to store the file descriptor if exclusive file opening is used.

* - `exclusive`
  - `BOOL`
  - Flag to indicate exclusive file opening.

* - `silence`
  - `BOOL`
  - Flag to silence the function by setting `on_critical_error=LOG_NONE`.
:::


---

**Return Value**


If successful, it returns the loaded JSON data as a `json_t *`. If an error occurs, it returns `NULL`.


**Notes**


- The function handles the loading of persistent JSON files, managing file opening, critical errors, and silencing options.
- It is important to handle the return value appropriately to check for successful loading.


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

