<!-- ============================================================== -->
(parse_expiry_date())=
# `parse_expiry_date()`
<!-- ============================================================== -->


The `parse_expiry_date()` function parses a date string and converts it into a `timestamp_t` value. 
This function is designed to handle various date formats and interpret them into a timestamp representation. 
The parsed timestamp is stored in the `timestamp` parameter. 
The function is useful for converting human-readable date formats into machine-readable timestamps.


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

PUBLIC int parse_expiry_date(
    const char      *date,
    timestamp_t     *timestamp
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
  - The input date string to be parsed. It can be in various formats, including absolute or relative dates.

* - `timestamp`
  - `timestamp_t *`
  - A pointer to a `timestamp_t` variable where the parsed timestamp will be stored.

:::


---

**Return Value**


Returns `0` on success if the date string is successfully parsed and the timestamp is stored in the provided pointer. 
Returns `-1` on failure if the date string is invalid or cannot be interpreted.


**Notes**


- The `date` parameter supports a wide range of formats, including ISO 8601, relative dates (e.g., "3 days ago"), and shorthand notations.
- Ensure that the `timestamp` pointer is valid and points to allocated memory before calling this function.
- This function is part of the time utilities and is commonly used for expiration date calculations.


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

