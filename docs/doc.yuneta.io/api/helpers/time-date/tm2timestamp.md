<!-- ============================================================== -->
(tm2timestamp())=
# `tm2timestamp()`
<!-- ============================================================== -->


Converts a `struct tm` representation of time into a timestamp string format. 
The resulting string is stored in the buffer provided by the user, which must 
be large enough to hold the formatted timestamp. The function returns a pointer 
to the buffer containing the timestamp string.


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

char *tm2timestamp(
    char    *bf,
    int     bfsize,
    struct tm *tm
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
  - A pointer to the buffer where the resulting timestamp string will be stored.

* - `bfsize`
  - `int`
  - The size of the buffer `bf` in bytes. This should be sufficient to hold the formatted timestamp.

* - `tm`
  - `struct tm *`
  - A pointer to a `struct tm` that contains the broken-down time representation to be converted.
:::


---

**Return Value**


Returns a pointer to the buffer `bf` containing the formatted timestamp string. 
If the buffer is not large enough to hold the formatted string, the behavior is 
undefined.


**Notes**


Ensure that the buffer `bf` is at least 90 bytes in size to accommodate the 
formatted timestamp. The function does not perform any checks on the validity 
of the `struct tm` data.


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

