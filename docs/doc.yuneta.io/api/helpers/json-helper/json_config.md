<!-- ============================================================== -->
(json_config())=
# `json_config()`
<!-- ============================================================== -->


This function generates a final JSON configuration string by merging various input parameters. It takes fixed and variable configuration strings, as well as optional configuration files and parameters, to produce a comprehensive JSON output. If any JSON formatting errors occur, the function will terminate based on the `quit` parameter, printing the error message. Additionally, if `print_verbose_config` or `print_final_config` is set to TRUE, the function will print the resulting configuration and exit with a success status.


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

char *json_config(
    BOOL print_verbose_config,
    BOOL print_final_config,
    const char *fixed_config,
    const char *variable_config,
    const char *config_json_file,
    const char *parameter_config,
    pe_flag_t quit
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `print_verbose_config`
  - `BOOL`
  - Indicates whether to print verbose configuration output and exit.

* - `print_final_config`
  - `BOOL`
  - Indicates whether to print the final configuration output and exit.

* - `fixed_config`
  - `const char *`
  - A string representing fixed configuration that is not writable.

* - `variable_config`
  - `const char *`
  - A string representing writable variable configuration.

* - `config_json_file`
  - `const char *`
  - A string representing the path to a configuration JSON file that can overwrite the variable configuration.

* - `parameter_config`
  - `const char *`
  - A string representing additional parameters that can overwrite the variable configuration.

* - `quit`
  - `pe_flag_t`
  - A flag that determines the behavior of the function upon encountering an error.
:::


---

**Return Value**


The function returns a pointer to a dynamically allocated string containing the final JSON configuration. It is important to free this string using `jsonp_free()` to avoid memory leaks.


**Notes**


The JSON input can include one-line comments using the combination `#^^`. Additionally, the function supports expanding a dictionary of JSON data within a specified range using `{^^ ^^}` syntax. A special key `__json_config_variables__` can be used to replace strings within parentheses with corresponding values from a global variable dictionary.


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

