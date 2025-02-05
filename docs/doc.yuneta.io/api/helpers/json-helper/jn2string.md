<!-- ============================================================== -->
(jn2string())=
# `jn2string()`
<!-- ============================================================== -->


The `jn2string()` function converts a JSON value (`json_t *`) into a string representation. 
The returned string is dynamically allocated and must be freed by the caller using `gbmem_free()`. 
This function is useful for extracting string data from JSON objects or arrays in a safe and consistent manner.


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

char *jn2string(
    json_t *jn_var
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `jn_var`
  - `json_t *`
  - A pointer to the JSON value to be converted into a string. This parameter is not owned by the function, and the caller is responsible for its lifecycle.

:::


---

**Return Value**


A dynamically allocated string containing the string representation of the JSON value. 
The caller must free the returned string using `gbmem_free()`. If the conversion fails, the function may return `NULL`.


**Notes**


- The function only works with JSON values that can be represented as strings.
- Ensure that the returned string is freed using `gbmem_free()` to avoid memory leaks.
- This function is part of a suite of JSON utility functions, such as [`jn2integer()`](#jn2integer) and [`jn2real()`](#jn2real), which provide similar conversions for other data types.


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

