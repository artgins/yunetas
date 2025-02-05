<!-- ============================================================== -->
(formatdate())=
# `formatdate()`
<!-- ============================================================== -->


The `formatdate` function formats a given timestamp into a string representation based on the specified format. It takes a `time_t` value, which represents the time to be formatted, and converts it into a human-readable string according to the provided format string. The result is stored in the buffer provided by the caller.

The function is useful for displaying dates and times in various formats, allowing for customization based on the needs of the application.


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

char *formatdate(
    time_t  t,
    char   *bf,
    int     bfsize,
    const char *format
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `t`
  - `time_t`
  - The timestamp to be formatted.

* - `bf`
  - `char *`
  - A buffer where the formatted date string will be stored.

* - `bfsize`
  - `int`
  - The size of the buffer `bf` to ensure that it does not overflow.

* - `format`
  - `const char *`
  - A format string that specifies how the date should be formatted.
:::


---

**Return Value**


Returns a pointer to the buffer containing the formatted date string. If the formatting fails, it may return NULL.


**Notes**


Ensure that the buffer provided (`bf`) is large enough to hold the formatted date string to avoid buffer overflows. The function does not perform checks on the format string, so it is the caller's responsibility to provide a valid format.


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

