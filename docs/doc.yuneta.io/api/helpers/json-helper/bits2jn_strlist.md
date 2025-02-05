<!-- ============================================================== -->
(bits2jn_strlist())=
# `bits2jn_strlist()`
<!-- ============================================================== -->


This function converts a bitmask represented by `bits` into a JSON list of strings. The strings are derived from the provided `strings_table`, which must be terminated by a NULL pointer. Each bit in the `bits` value corresponds to an index in the `strings_table`. If a bit is set (i.e., it is 1), the corresponding string from the `strings_table` will be included in the resulting JSON list. The function returns a pointer to a JSON object that contains the list of selected strings.


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

PUBLIC json_t *bits2jn_strlist(
    const char **strings_table,
    uint64_t bits
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `strings_table`
  - `const char **`
  - A NULL-terminated array of strings from which the JSON list will be created.

* - `bits`
  - `uint64_t`
  - A bitmask indicating which strings from `strings_table` should be included in the JSON list.
:::


---

**Return Value**


Returns a pointer to a `json_t` object representing the list of strings corresponding to the set bits in the `bits` parameter. The caller is responsible for managing the memory of the returned JSON object.


**Notes**


The function assumes that the `strings_table` is properly terminated with a NULL pointer. If `bits` contains values that exceed the bounds of the `strings_table`, those bits will be ignored. Ensure to free the returned JSON object when it is no longer needed.


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

