<!-- ============================================================== -->
(json_config())=
# `json_config()`
<!-- ============================================================== -->


The `json_config()` function generates a final JSON configuration string by merging multiple input configurations in a specific order. 
It supports fixed, variable, file-based, and parameter-based configurations, and allows for verbose and final output printing. 
The function also handles JSON format validation and can terminate the program based on the `quit` parameter if errors occur.


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
    BOOL        print_verbose_config,  // WARNING, if true will exit(0)
    BOOL        print_final_config,    // WARNING, if true will exit(0)
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
  - If `TRUE`, prints the verbose configuration and exits the program.

* - `print_final_config`
  - `BOOL`
  - If `TRUE`, prints the final configuration and exits the program.

* - `fixed_config`
  - `const char *`
  - A non-writable JSON string used as the base configuration.

* - `variable_config`
  - `const char *`
  - A writable JSON string that can be modified by subsequent configurations.

* - `config_json_file`
  - `const char *`
  - A file or list of files containing JSON configurations that overwrite `variable_config`.

* - `parameter_config`
  - `const char *`
  - A JSON string containing parameters that overwrite `variable_config`.

* - `quit`
  - `pe_flag_t`
  - Determines the program's behavior on JSON format errors. If set, the program exits on error.
:::


---

**Return Value**


Returns a dynamically allocated string containing the final merged JSON configuration. 
The caller must free the returned string using `jsonp_free()`.


**Notes**


- The function processes configurations in the following order:
  1. `fixed_config` (non-writable).
  2. `variable_config` (writable).
  3. `config_json_file` (overwrites `variable_config`).
  4. `parameter_config` (overwrites `variable_config`).

- The function supports one-line comments in JSON strings using the `#^^` syntax.

- Special JSON expansion syntax `{^^ ^^}` is supported for generating ranges and variable substitutions.

- A special key `__json_config_variables__` allows for replacing placeholders `( ^^ ^^ )` in the JSON string with values from a dictionary.

- If `print_verbose_config` or `print_final_config` is `TRUE`, the function prints the respective configuration and exits the program.

- Ensure to free the returned string using `jsonp_free()` to avoid memory leaks.


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

