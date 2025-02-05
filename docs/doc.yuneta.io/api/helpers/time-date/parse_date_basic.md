<!-- ============================================================== -->
(parse_date_basic())=
# `parse_date_basic()`
<!-- ============================================================== -->


The `parse_date_basic()` function parses a date string in a simplified format and converts it into a timestamp and an optional timezone offset. 
This function is designed to handle basic date formats and is less flexible compared to other date-parsing utilities like `approxidate_careful()`.

The function extracts the timestamp from the provided `date` string and stores it in the `timestamp` parameter. 
If a timezone offset is present in the date string, it is stored in the `offset` parameter.


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

int parse_date_basic(
    const char      *date,
    timestamp_t     *timestamp,
    int             *offset
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
  - The input date string to be parsed.

* - `timestamp`
  - `timestamp_t *`
  - Pointer to a variable where the parsed timestamp will be stored.

* - `offset`
  - `int *`
  - Pointer to a variable where the timezone offset will be stored. Can be `NULL` if the offset is not needed.

:::


---

**Return Value**


Returns `0` on success, or `-1` if the date string cannot be parsed.


**Notes**


- The `timestamp` output is in seconds since the Unix epoch (January 1, 1970).
- The `offset` is optional and represents the timezone offset in seconds from UTC.
- This function is intended for basic date parsing and may not handle complex or relative date formats. For more advanced parsing, consider using [`approxidate_careful()`](#approxidate_careful).


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

