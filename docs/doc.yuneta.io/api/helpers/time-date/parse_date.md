<!-- ============================================================== -->
(parse_date())=
# `parse_date()`
<!-- ============================================================== -->


The `parse_date` function is designed to interpret and convert a variety of date formats from a string representation into a standardized output format. It supports multiple date formats, including absolute dates (like ISO 8601), relative dates (like "3 days ago"), and shorthand notations (like "today"). The function modifies the output string to reflect the parsed date, ensuring that it fits within the specified output size.


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
    char *out,
    int outsize
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

* - `out`
  - `char *`
  - A buffer where the parsed date will be stored as a string.

* - `outsize`
  - `int`
  - The size of the output buffer to ensure that the parsed date does not exceed this limit.

:::


---

**Return Value**


Returns an integer indicating the success or failure of the parsing operation. A return value of 0 typically indicates success, while a negative value indicates an error in parsing the date.


**Notes**


The function is capable of handling a wide range of date formats, including but not limited to ISO 8601, basic English formats, and relative dates. It is important to ensure that the output buffer is sufficiently sized to accommodate the formatted date string.


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

