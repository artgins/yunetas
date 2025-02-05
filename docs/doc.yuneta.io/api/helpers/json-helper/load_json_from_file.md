<!-- ============================================================== -->
(load_json_from_file())=
# `load_json_from_file()`
<!-- ============================================================== -->


The `load_json_from_file()` function loads a JSON object from a file located in the specified directory. 
It reads the file content, parses it as JSON, and returns the resulting JSON object. 
If an error occurs during the process, the behavior depends on the `on_critical_error` parameter, which determines how critical errors are handled.


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

PUBLIC json_t *load_json_from_file(
    hgobj           gobj,
    const char      *directory,
    const char      *filename,
    log_opt_t       on_critical_error
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
  - The gobj (generic object) context used for logging or error handling.

* - `directory`
  - `const char *`
  - The directory path where the JSON file is located.

* - `filename`
  - `const char *`
  - The name of the JSON file to be loaded.

* - `on_critical_error`
  - `log_opt_t`
  - Specifies the behavior in case of critical errors (e.g., logging or ignoring errors).
:::


---

**Return Value**


Returns a `json_t *` object representing the parsed JSON data from the file. 
If the file cannot be read or the JSON parsing fails, the function may return `NULL` depending on the `on_critical_error` parameter.


**Notes**


- Ensure that the file exists and is accessible in the specified directory.
- The returned `json_t *` object must be managed properly; remember to free it using appropriate JSON library functions when no longer needed.
- If `on_critical_error` is set to `LOG_NONE`, the function will suppress error messages.


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

