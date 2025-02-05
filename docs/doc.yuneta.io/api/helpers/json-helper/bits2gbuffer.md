<!-- ============================================================== -->
(bits2gbuffer())=
# `bits2gbuffer()`
<!-- ============================================================== -->


The `bits2gbuffer()` function converts a set of bit flags represented by a 64-bit integer (`bits`) into a `gbuffer_t` object containing a string representation of the flags. The function uses a `strings_table` to map each bit position to its corresponding string. The resulting string in the `gbuffer_t` object contains the flag names separated by the default delimiters (`|`).

This function is useful for translating bit-based flags into human-readable string representations.


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
    uint64_t    bits
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
  - A null-terminated array of strings representing the names of the bit flags. Each string corresponds to a specific bit position.

* - `bits`
  - `uint64_t`
  - A 64-bit integer representing the set of bit flags to be converted.

:::


---

**Return Value**


Returns a pointer to a `gbuffer_t` object containing the string representation of the bit flags. The caller is responsible for managing the memory of the returned `gbuffer_t` object.


**Notes**


- The `strings_table` must be null-terminated, and the strings must correspond to strict ascending bit values (e.g., 0x0001, 0x0002, 0x0004, etc.).
- If no bits are set, the resulting `gbuffer_t` will contain an empty string.
- Ensure that the `gbuffer_t` object is properly freed after use to avoid memory leaks.


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

