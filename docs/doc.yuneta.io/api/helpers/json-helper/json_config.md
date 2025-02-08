<!-- ============================================================== -->
(json_config())=
# `json_config()`
<!-- ============================================================== -->

The `json_config` function merges multiple JSON configuration sources into a single JSON string, allowing for variable substitution and expansion. It processes fixed, variable, file-based, and parameter-based configurations in a structured order.

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
    BOOL        print_verbose_config,
    BOOL        print_final_config,
    const char  *fixed_config,
    const char  *variable_config,
    const char  *config_json_file,
    const char  *parameter_config,
    pe_flag_t   quit
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
  - If `TRUE`, prints the verbose configuration and exits.

* - `print_final_config`
  - `BOOL`
  - If `TRUE`, prints the final merged configuration and exits.

* - `fixed_config`
  - `const char *`
  - A fixed JSON configuration string that cannot be modified.

* - `variable_config`
  - `const char *`
  - A JSON configuration string that can be modified.

* - `config_json_file`
  - `const char *`
  - A file path containing JSON configuration data.

* - `parameter_config`
  - `const char *`
  - A JSON string containing additional configuration parameters.

* - `quit`
  - `pe_flag_t`
  - Determines the behavior on error, such as exiting or continuing execution.
:::

---

**Return Value**

Returns a dynamically allocated JSON string containing the merged configuration. The caller must free the returned string using `jsonp_free()`.

**Notes**

['The function processes configurations in the following order: `fixed_config`, `variable_config`, `config_json_file`, and `parameter_config`.', 'If `print_verbose_config` or `print_final_config` is `TRUE`, the function prints the configuration and exits.', 'The function supports variable substitution using the `__json_config_variables__` key.', 'The JSON string can contain one-line comments using `##^`.', 'If an error occurs in JSON parsing, the function may exit based on the `quit` parameter.']

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
