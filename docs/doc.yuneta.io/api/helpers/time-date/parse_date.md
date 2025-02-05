<!-- ============================================================== -->
(parse_date())=
# `parse_date()`
<!-- ============================================================== -->


The `parse_date()` function parses a date string provided in various formats and converts it into a normalized string representation. 
The normalized date is stored in the `out` buffer. This function supports a wide range of date formats, including absolute, relative, 
and shorthand formats, as well as combinations of date and time. The function ensures that the output is formatted consistently 
for further processing or display.


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

int parse_date(
    const char *date,
    char       *out,
    int         outsize
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `date`
  - `const char *`
  - The input date string to be parsed. It can be in various formats, such as ISO 8601, relative dates, or shorthand notations.

* - `out`
  - `char *`
  - A buffer where the normalized date string will be stored. The buffer must be pre-allocated by the caller.

* - `outsize`
  - `int`
  - The size of the `out` buffer in bytes. This ensures that the function does not write beyond the allocated memory.
:::


---

**Return Value**


Returns `0` on success, indicating that the date string was successfully parsed and normalized. 
Returns `-1` on failure, indicating that the input date string could not be parsed or was invalid.


**Notes**


- The `out` buffer must be large enough to store the normalized date string. If the buffer is too small, the function may fail.
- The function supports a wide range of date formats, including absolute dates (e.g., `2024-09-17`), relative dates (e.g., `3 days ago`), 
  and shorthand notations (e.g., `yesterday`).
- For more advanced parsing, consider using [`approxidate_careful()`](#approxidate_careful) or [`approxidate_relative()`](#approxidate_relative).


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

