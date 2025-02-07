<!-- ============================================================== -->
(set_expected_results)=
# `set_expected_results()`
<!-- ============================================================== -->

`set_expected_results()` initializes the expected test results, including expected errors, expected JSON output, ignored keys, and verbosity settings.

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
void set_expected_results(
    const char  *name,
    json_t      *errors_list,
    json_t      *expected,
    const char  **ignore_keys,
    BOOL        verbose
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `name`
  - `const char *`
  - The name of the test case.

* - `errors_list`
  - `json_t *`
  - A JSON array containing expected error messages.

* - `expected`
  - `json_t *`
  - A JSON object representing the expected test output.

* - `ignore_keys`
  - `const char **`
  - An array of keys to be ignored during JSON comparison.

* - `verbose`
  - `BOOL`
  - Flag indicating whether verbose output should be enabled.
:::

---

**Return Value**

This function does not return a value.

**Notes**

The function resets previously stored expected results before setting new ones.
If `verbose` is enabled, the function prints the test name to the console.
The function initializes `expected_log_messages`, `unexpected_log_messages`, and `expected` as JSON arrays if they are not provided.

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

