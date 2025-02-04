<!-- ============================================================== -->
(parse_expiry_date())=
# `parse_expiry_date()`
<!-- ============================================================== -->


The `parse_expiry_date` function parses the expiry date string and converts it into a timestamp value.

The function takes a date string as input and fills the provided `timestamp` pointer with the corresponding timestamp value.

The date string can be in various formats, including absolute dates, relative dates, special time keywords, combinations of date and time, and more. The function interprets the date string carefully to determine the correct timestamp value.


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

int parse_expiry_date(
    const char *date,
    timestamp_t *timestamp
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
  - The date string to be parsed.
  
* - `timestamp`
  - `timestamp_t *`
  - Pointer to a timestamp variable where the parsed timestamp value will be stored.
:::


---

**Return Value**


The function returns an integer value indicating the success or failure of the parsing operation:
- Returns the number of bytes consumed if successful.
- Returns -1 if an error occurs during parsing.


**Notes**


The `parse_expiry_date` function is capable of interpreting various date formats, including absolute dates, relative dates, special time keywords, and combinations of date and time. It carefully handles ambiguities and incomplete information in the date string to determine the correct timestamp value.

The function is designed to provide flexibility in parsing date strings and converting them into timestamp values accurately.


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

