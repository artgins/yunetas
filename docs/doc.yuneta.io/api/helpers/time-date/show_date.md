<!-- ============================================================== -->
(show_date())=
# `show_date()`
<!-- ============================================================== -->


The `show_date()` function formats a given timestamp into a human-readable date string based on the specified timezone and date mode. It is a utility function for converting timestamps into various date formats, such as ISO 8601, RFC 2822, or custom formats defined in the `date_mode` structure.


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

const char *show_date(
    timestamp_t           time,
    int                   timezone,
    const struct date_mode *mode
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `time`
  - `timestamp_t`
  - The timestamp to be formatted.

* - `timezone`
  - `int`
  - The timezone offset in minutes from UTC.

* - `mode`
  - `const struct date_mode *`
  - A pointer to a `date_mode` structure that specifies the desired date format and options.
:::


---

**Return Value**


A pointer to a statically allocated string containing the formatted date. The string is not dynamically allocated, so it should not be freed by the caller.


**Notes**


- The `date_mode` structure allows for flexible formatting, including predefined formats like ISO 8601 and custom formats using `strftime`.
- The returned string is statically allocated, so its content may be overwritten by subsequent calls to `show_date()`.
- This function is useful for logging, debugging, or displaying timestamps in user interfaces.


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

