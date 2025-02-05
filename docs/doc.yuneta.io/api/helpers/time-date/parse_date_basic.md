<!-- ============================================================== -->
(parse_date_basic())=
# `parse_date_basic()`
<!-- ============================================================== -->


The `parse_date_basic` function is designed to interpret a date string and convert it into a timestamp. It supports various date formats, including absolute and relative dates, and can handle special time keywords. The function modifies the provided timestamp and offset pointers to reflect the parsed date and any timezone offset, respectively.


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
    const char *date,
    timestamp_t *timestamp,
    int *offset
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
  - A string representing the date to be parsed. It can be in various formats, including ISO 8601, English date formats, and relative dates.

* - `timestamp`
  - `timestamp_t *`
  - A pointer to a `timestamp_t` variable where the parsed timestamp will be stored.

* - `offset`
  - `int *`
  - A pointer to an integer where the timezone offset will be stored.
:::


---

**Return Value**


The function returns an integer indicating the success or failure of the parsing operation. A return value of 0 typically indicates success, while a non-zero value indicates an error in parsing the date string.


**Notes**


This function is capable of interpreting a wide range of date formats, including both absolute and relative dates. It is important to ensure that the provided date string is in a recognizable format to avoid parsing errors.


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

