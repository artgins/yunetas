<!-- ============================================================== -->
(cmp_two_simple_json())=
# `cmp_two_simple_json()`
<!-- ============================================================== -->


The `cmp_two_simple_json()` function compares two simple JSON values (`jn_var1` and `jn_var2`) and determines their relative order. 
It is designed to handle basic JSON types such as strings, integers, real numbers, and booleans. 
Complex JSON types (e.g., objects or arrays) are treated as equal if their structure matches.

The function returns a value indicating whether the first JSON value is less than, equal to, or greater than the second, 
similar to the behavior of `strcmp` for strings.


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

int cmp_two_simple_json(
    json_t *jn_var1,    // NOT owned
    json_t *jn_var2     // NOT owned
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `jn_var1`
  - `json_t *`
  - The first JSON value to compare. This parameter is not owned by the function.

* - `jn_var2`
  - `json_t *`
  - The second JSON value to compare. This parameter is not owned by the function.

:::


---

**Return Value**


The function returns an integer value:

- `-1`: If `jn_var1` is less than `jn_var2`.
- `0`: If `jn_var1` is equal to `jn_var2`.
- `1`: If `jn_var1` is greater than `jn_var2`.


**Notes**


- This function only compares simple JSON types: strings, integers, real numbers, and booleans.
- Complex JSON types such as objects or arrays are treated as equal if their structure matches.
- The input JSON values are not modified or owned by the function.


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

