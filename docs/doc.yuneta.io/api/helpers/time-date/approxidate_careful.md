<!-- ============================================================== -->
(approxidate_careful())=
# `approxidate_careful()`
<!-- ============================================================== -->


The `approxidate_careful` function interprets a variety of date formats and returns a timestamp representing the parsed date. It can handle both absolute and relative date formats, including ISO 8601, English date formats, and shorthand notations. The function is designed to be robust against ambiguities and can interpret contextual phrases related to time, such as "tomorrow" or "next week". 

In addition to parsing standard date formats, it can also interpret relative dates based on the current date, allowing for phrases like "3 days ago" or "in 2 months". The function is particularly useful for applications that require flexible date input from users.

Error handling is incorporated, with the function returning a timestamp and optionally setting an error code if parsing fails.


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
    int *error
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
  - A string representing the date to be parsed. This can be in various formats, including absolute and relative dates.

* - `error`
  - `int *`
  - A pointer to an integer where the function can store an error code if parsing fails. This parameter can be NULL if error reporting is not needed.

:::


---

**Return Value**


The function returns a `timestamp_t` representing the parsed date. If the date cannot be parsed, the return value may indicate an error, and the error code can be checked via the `error` parameter.


**Notes**


The function is capable of interpreting a wide range of date formats, including:

1. Absolute Dates:
   - ISO 8601 format (e.g., YYYY-MM-DD)
   - Basic English formats (e.g., "Sep 17 2024")
   - US-centric formats (e.g., "09/17/2024")

2. Relative Dates:
   - Phrases like "3 days ago" or "next Friday".

3. Special Time Keywords:
   - Keywords like "noon" or "midnight".

4. Shorthand Dates:
   - Shortcuts like "0" for today or "-1" for yesterday.

The function attempts to resolve ambiguities in date formats based on context.


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

