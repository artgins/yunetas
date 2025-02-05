<!-- ============================================================== -->
(bits2gbuffer())=
# `bits2gbuffer()`
<!-- ============================================================== -->


The `bits2gbuffer` function converts a bitmask represented by the `bits` parameter into a `gbuffer_t` structure. The conversion is based on a provided table of strings, `strings_table`, which must be terminated by a NULL pointer. Each bit in the `bits` value corresponds to an index in the `strings_table`, allowing for the creation of a buffer that contains the concatenated strings for the bits that are set.


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
  - A table of strings where each string corresponds to a specific bit position. The table must end with a NULL pointer.

* - `bits`
  - `uint64_t`
  - A bitmask where each bit indicates whether the corresponding string in `strings_table` should be included in the resulting gbuffer.

:::


---

**Return Value**


Returns a pointer to a `gbuffer_t` structure containing the concatenated strings corresponding to the set bits in the `bits` parameter. The caller is responsible for freeing the returned gbuffer when it is no longer needed.


**Notes**


Ensure that the `strings_table` is properly terminated with a NULL pointer to avoid undefined behavior. The function allocates memory for the gbuffer, which must be freed by the caller.


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

