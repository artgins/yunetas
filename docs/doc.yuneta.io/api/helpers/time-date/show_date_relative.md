<!-- ============================================================== -->
(show_date_relative())=
# `show_date_relative()`
<!-- ============================================================== -->


The `show_date_relative()` function formats a given timestamp into a human-readable relative date string. 
It calculates the relative time difference (e.g., "2 days ago", "3 hours from now") and stores the result 
in the provided buffer. This function is useful for displaying time differences in a user-friendly format.


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

void show_date_relative(
    timestamp_t  time,
    char        *timebuf,
    int          timebufsize
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
  - The timestamp to be converted into a relative date.

* - `timebuf`
  - `char *`
  - A pointer to the buffer where the formatted relative date string will be stored.

* - `timebufsize`
  - `int`
  - The size of the buffer `timebuf` in bytes, ensuring the function does not write beyond its bounds.

:::


---

**Return Value**


This function does not return a value. The formatted relative date string is stored in the `timebuf` buffer.


**Notes**


- Ensure that the `timebuf` buffer is large enough to hold the resulting string, as specified by `timebufsize`.
- The function uses relative time calculations, so the result depends on the current system time.
- This function is particularly useful for displaying user-friendly time differences in logs or UI elements.


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

