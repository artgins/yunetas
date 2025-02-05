<!-- ============================================================== -->
(bits2gbuffer())=
# `bits2gbuffer()`
<!-- ============================================================== -->


Converts a bitmask represented by `bits` into a `gbuffer_t` containing a list of strings. The strings are derived from the provided `strings_table`, which must be terminated with a NULL pointer. Each bit in `bits` corresponds to an index in the `strings_table`, where a bit set to 1 indicates that the corresponding string should be included in the resulting `gbuffer_t`. This function is useful for generating a dynamic list of string representations based on bitmask values.


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

gbuffer_t *bits2gbuffer(
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
  - A NULL-terminated array of strings that represent the possible values corresponding to the bits.

* - `bits`
  - `uint64_t`
  - A bitmask where each bit represents the inclusion of the corresponding string from `strings_table`.
:::


---

**Return Value**


Returns a pointer to a `gbuffer_t` containing the selected strings based on the bitmask. The caller is responsible for freeing the returned `gbuffer_t` when it is no longer needed.


**Notes**


Ensure that the `strings_table` is properly terminated with a NULL pointer to avoid undefined behavior. The function assumes that the bits are strictly within the bounds of the provided string table.


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

