<!-- ============================================================== -->
(bits2jn_strlist())=
# `bits2jn_strlist()`
<!-- ============================================================== -->


The `bits2jn_strlist()` function converts a bitmask (`bits`) into a JSON list of strings. 
Each bit in the bitmask corresponds to a string in the `strings_table`. The function iterates 
through the bitmask, and for each bit that is set, it adds the corresponding string from 
`strings_table` to the resulting JSON list. The `strings_table` must be a NULL-terminated array of strings.


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

json_t *bits2jn_strlist(
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
  - A NULL-terminated array of strings representing the names of the bits.

* - `bits`
  - `uint64_t`
  - A bitmask where each bit corresponds to an entry in the `strings_table`.

:::


---

**Return Value**


Returns a `json_t *` object representing a JSON array of strings. Each string in the array corresponds 
to a bit set in the `bits` parameter. If no bits are set, the function returns an empty JSON array.


**Notes**


- The `strings_table` must be strictly ordered in ascending bit value, and it must end with a NULL entry.
- The caller is responsible for managing the memory of the returned `json_t *` object, typically by using `json_decref()`.
- This function is useful for converting bitmask representations into human-readable JSON formats.


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

