<!-- ============================================================== -->
(json_config())=
# `json_config()`
<!-- ============================================================== -->


Return a malloc'ed string with the final configuration by joining the json format input string parameters. Handles various configurations and loads them in a specific order. Can expand a dict of json data in a range. Allows for one-line comments in the json string.


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
  - If true, prints the result and exits(0).

* - `print_final_config`
  - `BOOL`
  - If true, prints the result and exits(0).

* - `fixed_config`
  - `const char *`
  - String config that is not writable.

* - `variable_config`
  - `const char *`
  - Writable string config.

* - `config_json_file`
  - `const char *`
  - File of file's list, overwriting variable_config.

* - `parameter_config`
  - `const char *`
  - String overwriting variable_config.

* - `quit`
  - `pe_flag_t`
  - Parameter to handle errors in json format.
:::


---

**Return Value**


A malloc'ed string with the final configuration. Remember to free the returned string with `jsonp_free()`.


**Notes**


- Handles various configurations and loads them in a specific order.
- Can expand a dict of json data in a range.
- Allows for one-line comments in the json string.


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

