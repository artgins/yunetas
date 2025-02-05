<!-- ============================================================== -->
(approxidate())=
# `approxidate()`
<!-- ============================================================== -->


The `approxidate` macro simplifies the process of parsing human-readable date and time strings into a `timestamp_t` value. It is a wrapper around the `approxidate_careful()` function, which provides robust parsing capabilities for a wide range of date and time formats. This macro assumes no additional context (`NULL` is passed as the second parameter to `approxidate_careful()`).


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

#define approxidate(s) approxidate_careful((s), NULL)

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `s`
  - `const char *`
  - A string representing the date and time to be parsed. It can include absolute dates, relative dates, or combinations of both.

:::


---

**Return Value**


Returns a `timestamp_t` value representing the parsed date and time. If the input string cannot be parsed, the behavior depends on the implementation of `approxidate_careful()`.


**Notes**


- The `approxidate` macro is a shorthand for calling `approxidate_careful()` with a `NULL` context.
- The parsing capabilities include absolute dates (e.g., `2024-09-17`), relative dates (e.g., `3 days ago`), and combinations (e.g., `yesterday 3:00 PM`).
- For more advanced usage or error handling, consider using `approxidate_careful()` directly.


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

