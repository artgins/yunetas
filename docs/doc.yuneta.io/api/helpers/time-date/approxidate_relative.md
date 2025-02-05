<!-- ============================================================== -->
(approxidate_relative())=
# `approxidate_relative()`
<!-- ============================================================== -->


The `approxidate_relative()` function interprets a date string in various formats and converts it into a `timestamp_t` value. 
This function is particularly useful for parsing relative dates (e.g., "3 days ago", "next Friday") or absolute dates in standard formats (e.g., "2024-09-17").
It leverages the current date or a reference point to resolve relative terms.


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

timestamp_t approxidate_relative(
    const char *date
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
  - The date string to be parsed. It can include absolute dates, relative dates, or combinations of both.

:::


---

**Return Value**


Returns a `timestamp_t` value representing the parsed date and time. The value is in seconds since the Unix epoch (January 1, 1970).


**Notes**


- The function supports a wide range of date formats, including ISO 8601, US-centric formats, relative phrases (e.g., "yesterday", "3 days ago"), and shorthand notations.
- For more advanced parsing, consider using [`approxidate_careful()`](#approxidate_careful), which allows for additional error handling and context.
- Ambiguities in the input string are resolved based on context, but incorrect or unsupported formats may lead to undefined behavior.


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

