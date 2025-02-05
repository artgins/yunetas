<!-- ============================================================== -->
(strings2bits())=
# `strings2bits()`
<!-- ============================================================== -->


Converts a string into a bit representation based on a provided table of strings. 
The function interprets the input string by splitting it using specified separators 
and maps each segment to its corresponding bit in the output. The strings table must 
be terminated with a NULL pointer. The resulting bit representation is a 64-bit 
unsigned integer where each bit corresponds to the presence of a string in the 
input string.


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
  - A table of strings that defines the mapping for the bit representation. The table must end with a NULL pointer.

* - `str`
  - `const char *`
  - The input string to be converted into bits. The string is split using the specified separators.

* - `separators`
  - `const char *`
  - A string containing characters that will be used as delimiters to split the input string.
:::


---

**Return Value**


Returns a 64-bit unsigned integer where each bit represents the presence of a corresponding string from the `strings_table` in the input string. If a string is found, its corresponding bit is set to 1; otherwise, it is set to 0.


**Notes**


The function assumes that the input string can be split using the specified separators. If the input string is empty or does not match any strings in the table, the return value will be 0.


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

