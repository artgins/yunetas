<!-- ============================================================== -->
(json_str_in_list())=
# `json_str_in_list()`
<!-- ============================================================== -->


The `json_str_in_list()` function checks if a given string `str` exists within a JSON list `jn_list`. 
It returns `TRUE` if the string is found, and `FALSE` otherwise. The comparison can be case-sensitive 
or case-insensitive based on the value of the `ignore_case` parameter.


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

PUBLIC BOOL json_str_in_list(
    hgobj       gobj,
    json_t      *jn_list,
    const char  *str,
    BOOL        ignore_case
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
  - A handle to the GObj (generic object) that may be used for logging or context.

* - `jn_list`
  - `json_t *`
  - A JSON array containing the list of strings to search within.

* - `str`
  - `const char *`
  - The string to search for in the JSON list.

* - `ignore_case`
  - `BOOL`
  - If `TRUE`, the comparison is case-insensitive; otherwise, it is case-sensitive.

:::


---

**Return Value**


Returns `TRUE` if the string `str` is found in the JSON list `jn_list`, otherwise returns `FALSE`.


**Notes**


- The `jn_list` parameter must be a valid JSON array; otherwise, the behavior is undefined.
- Use this function to efficiently check membership of a string in a JSON list.
- If `ignore_case` is set to `TRUE`, the comparison ignores differences in letter casing.


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

