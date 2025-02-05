<!-- ============================================================== -->
(t2timestamp())=
# `t2timestamp()`
<!-- ============================================================== -->


Converts a given `time_t` value into a human-readable timestamp string. The function formats the timestamp according to the specified local time setting. The output is written into the provided buffer, which must be large enough to hold the resulting string.


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

char *t2timestamp(
    char    *bf,
    int     bfsize,
    time_t  t,
    BOOL    local
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `bf`
  - `char *`
  - A pointer to the buffer where the formatted timestamp will be stored.

* - `bfsize`
  - `int`
  - The size of the buffer `bf`, which must be sufficient to hold the resulting timestamp string.

* - `t`
  - `time_t`
  - The `time_t` value representing the time to be converted.

* - `local`
  - `BOOL`
  - A boolean flag indicating whether to convert the time to local time (TRUE) or to UTC (FALSE).
:::


---

**Return Value**


Returns a pointer to the buffer `bf` containing the formatted timestamp string. If the buffer is too small, the behavior is undefined.


**Notes**


Ensure that the buffer provided is at least 90 bytes in size to accommodate the formatted timestamp string. The function does not perform bounds checking on the buffer size.


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

