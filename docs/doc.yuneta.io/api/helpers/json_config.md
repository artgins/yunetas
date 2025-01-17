

<!-- ============================================================== -->
(json_config())=
# `json_config()`
<!-- ============================================================== -->

The `json_config()` function merges multiple JSON configurations into a single final configuration string, following a specific order of precedence. It also includes features like comment handling, variable substitution, and range-based expansion for flexibility and advanced use cases.

See [`Settings`](settings)


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

typedef enum {
    PEF_CONTINUE    = 0,
    PEF_EXIT        = -1,
    PEF_ABORT       = -2,
    PEF_SYSLOG      = -3,
} pe_flag_t;

PUBLIC char *json_config(
    BOOL print_verbose_config,     // Print intermediate configurations and exit(0) if TRUE.
    BOOL print_final_config,       // Print final configuration and exit(0) if TRUE.
    const char *fixed_config,      // Fixed JSON string (not writable).
    const char *variable_config,   // Writable JSON string.
    const char *config_json_file,  // Path to JSON file(s) for overwriting variable_config.
    const char *parameter_config,  // JSON string for parameterized overwrites.
    pe_flag_t quit                 // Behavior flag on error (e.g., exit or continue).
);

```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - **Parameter**
  - **Type**
  - **Description**

* - `print_verbose_config`
  - `BOOL`
  - If `TRUE`, prints intermediate configurations and exits the program (`exit(0)`).

* - `print_final_config`
  - `BOOL`
  - If `TRUE`, prints the final configuration and exits the program (`exit(0)`).

* - `fixed_config`
  - `const char *`
  - A fixed JSON string that cannot be modified during merging.

* - `variable_config`
  - `const char *`
  - A writable JSON string that can be overwritten by subsequent inputs.

* - `config_json_file`
  - `const char *`
  - Path(s) to JSON file(s) whose contents overwrite `variable_config`.

* - `parameter_config`
  - `const char *`
  - A JSON string for overwriting specific parameters in the final configuration.

* - `quit`
  - `pe_flag_t`
  - Controls program behavior on JSON parsing errors. For example, it can determine whether to exit the program or continue.
:::

---

**Return Value**

## Return Value

- Returns a dynamically allocated string containing the final merged JSON configuration.

```{warning}
**Important:** The returned string must be freed using `jsonp_free()` to avoid memory leaks.

```


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
