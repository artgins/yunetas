<!-- ============================================================== -->
(load_json_from_file())=
# `load_json_from_file()`
<!-- ============================================================== -->


This function loads a JSON file from the specified directory and filename. It reads the content of the file and returns a JSON object representing the data.

If the file cannot be loaded or there is a critical error during the process, the function handles the error based on the specified logging options.


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

json_t *load_json_from_file(
    hgobj gobj,
    const char *directory,
    const char *filename,
    log_opt_t on_critical_error
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
  - The gobj instance associated with the function.
  
* - `directory`
  - `const char *`
  - The directory path where the JSON file is located.
  
* - `filename`
  - `const char *`
  - The name of the JSON file to load.
  
* - `on_critical_error`
  - `log_opt_t`
  - Logging options for critical errors.
:::


---

**Return Value**


A JSON object representing the data loaded from the file. If there is an error during the loading process, it may return NULL.


**Notes**


- This function is designed to handle the loading of JSON files and return the data as a JSON object.
- It is important to ensure that the directory and filename parameters are correctly specified to load the desired JSON file.
- The on_critical_error parameter determines how critical errors are logged during the loading process.


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

