<!-- ============================================================== -->
(strings2bits())=
# `strings2bits()`
<!-- ============================================================== -->


The `strings2bits()` function converts a string containing multiple substrings, separated by specified delimiters, into a 64-bit bitmask. Each substring is matched against a table of strings (`strings_table`), and the corresponding bit in the bitmask is set if a match is found.

The function is useful for managing bit-based flags where each flag is represented by a string in the `strings_table`. By default, the separators are `|`, `,`, and space (` `), but custom separators can also be provided.


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

uint64_t strings2bits(
    const char **strings_table,
    const char *str,
    const char *separators
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
  - A null-terminated array of strings representing the mapping of substrings to bit positions. Each string corresponds to a specific bit in the resulting bitmask.

* - `str`
  - `const char *`
  - The input string containing substrings to be converted into bits. Substrings are separated by the specified delimiters.

* - `separators`
  - `const char *`
  - A string containing the characters used as delimiters to split the input string. If `NULL`, the default separators (`|`, `,`, and space) are used.

:::


---

**Return Value**


The function returns a 64-bit unsigned integer (`uint64_t`) representing the bitmask. Each bit corresponds to a substring in the input string that matches an entry in the `strings_table`. If no matches are found, the function returns `0`.


**Notes**


- The `strings_table` must be strictly null-terminated.
- The function does not include empty substrings in the bitmask calculation.
- If a substring in the input string does not match any entry in the `strings_table`, it is ignored.
- The function assumes that the `strings_table` contains unique strings and that the table is properly ordered.


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

