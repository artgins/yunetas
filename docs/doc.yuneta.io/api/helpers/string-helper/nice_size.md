<!-- ============================================================== -->
(nice_size())=
# `nice_size()`
<!-- ============================================================== -->


The `nice_size()` function formats a given size in bytes into a human-readable string representation. 
It converts the size into appropriate units (e.g., KB, MB, GB) and stores the result in the provided buffer. 
The function supports both 1024-based (binary) and 1000-based (decimal) unit conversions, depending on the `b1024` parameter.


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

void nice_size(
    char *bf,
    size_t bfsize,
    uint64_t bytes,
    BOOL b1024
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
  - Pointer to the buffer where the formatted size string will be stored.

* - `bfsize`
  - `size_t`
  - Size of the buffer `bf` in bytes.

* - `bytes`
  - `uint64_t`
  - The size in bytes to be formatted.

* - `b1024`
  - `BOOL`
  - If `TRUE`, use 1024-based units (e.g., KiB, MiB). If `FALSE`, use 1000-based units (e.g., KB, MB).
:::


---

**Return Value**


This function does not return a value. The formatted size string is stored in the `bf` buffer.


**Notes**


- Ensure that the `bf` buffer is large enough to hold the formatted string, including the null terminator.
- The function supports both binary (KiB, MiB, GiB) and decimal (KB, MB, GB) unit systems for flexibility.
- The `b1024` parameter determines the unit system used for conversion.


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

