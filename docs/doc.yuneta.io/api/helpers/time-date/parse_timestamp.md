<!-- ============================================================== -->
(parse_timestamp())=
# `parse_timestamp()`
<!-- ============================================================== -->


The `parse_timestamp` macro is defined as an alias for `strtoumax`, 
which converts a string to an unsigned integer of type `uintmax_t`. 
It is typically used to parse timestamp values from string representations.


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

#define parse_timestamp  strtoumax

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `str`
  - `const char *`
  - The string containing the numeric representation of the timestamp.

* - `endptr`
  - `char **`
  - Pointer to a pointer to character, where the function stores the address of the first invalid character.

* - `base`
  - `int`
  - The numerical base to be used for conversion (e.g., 10 for decimal, 16 for hexadecimal).

:::


---

**Return Value**


Returns the converted value as `uintmax_t`. If no valid conversion could be performed, 
zero is returned. If the value is out of range, `UINTMAX_MAX` is returned.


**Notes**


- This macro is a direct alias for `strtoumax`, so all behaviors and limitations of `strtoumax` apply.
- The `endptr` parameter can be used to check if the entire string was successfully parsed.
- The function does not detect signed overflows, as it only works with unsigned integers.


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

