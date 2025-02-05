<!-- ============================================================== -->
(approxidate_careful())=
# `approxidate_careful()`
<!-- ============================================================== -->


The `approxidate_careful()` function interprets a human-readable date string and converts it into a `timestamp_t` value. 
It supports a wide range of date formats, including absolute dates, relative dates, special time keywords, and combinations of date and time. 
The function is designed to handle ambiguous or incomplete input carefully, providing a robust and flexible date parsing mechanism.


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

timestamp_t approxidate_careful(
    const char *date,
    int        *offset
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
  - The human-readable date string to be parsed.

* - `offset`
  - `int *`
  - Pointer to an integer where the function stores the timezone offset in seconds. Can be `NULL` if the offset is not needed.

:::


---

**Return Value**


Returns a `timestamp_t` value representing the parsed date and time. If the input is invalid or cannot be parsed, the return value is undefined.


**Notes**


- The function supports a wide range of date formats, including ISO 8601, relative dates (e.g., "3 days ago"), and special keywords (e.g., "noon today").
- If `offset` is provided, it will contain the timezone offset in seconds relative to UTC.
- Ambiguous or incomplete input is interpreted based on context, such as the current date or a reference point.
- For shorthand dates like `0` or `-1`, the function interprets them as "today" and "yesterday," respectively.
- This function is part of a broader set of utilities for date and time manipulation.


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

