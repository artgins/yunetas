<!-- ============================================================== -->
(json_str_in_list())=
# `json_str_in_list()`
<!-- ============================================================== -->


This function checks if a given string is present in a JSON list. It takes into account the option to ignore case sensitivity based on the `ignore_case` parameter. The function is useful for validating the existence of specific string values within a JSON array, which can be particularly helpful in scenarios where JSON data is used for configuration or data exchange.


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

BOOL json_str_in_list(
    hgobj   gobj,
    json_t  *jn_list,
    const char *str,
    BOOL    ignore_case
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
  - The handle to the gobj instance, which may be used for logging or context.

* - `jn_list`
  - `json_t *`
  - A pointer to the JSON list (array) in which to search for the string.

* - `str`
  - `const char *`
  - The string to search for within the JSON list.

* - `ignore_case`
  - `BOOL`
  - A boolean flag indicating whether the search should be case insensitive.
:::


---

**Return Value**


Returns `TRUE` if the string is found in the JSON list, otherwise returns `FALSE`. The search behavior is influenced by the `ignore_case` parameter.


**Notes**


This function assumes that the JSON list is properly formatted and that the `gobj` parameter is valid. If the JSON list is empty or `NULL`, the function will return `FALSE`.


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

