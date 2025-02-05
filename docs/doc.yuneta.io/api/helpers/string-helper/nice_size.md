<!-- ============================================================== -->
(nice_size())=
# `nice_size()`
<!-- ============================================================== -->


The `nice_size` function formats a given size in bytes into a human-readable string representation. It converts the byte size into a more understandable format, such as KB, MB, GB, etc., depending on the value of the `b1024` parameter. The formatted string is stored in the buffer provided by the `bf` parameter, which must be large enough to hold the resulting string.


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
    char*    bf,
    size_t   bfsize,
    uint64_t bytes,
    BOOL     b1024
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
  - `char*`
  - A pointer to the buffer where the formatted size will be stored.

* - `bfsize`
  - `size_t`
  - The size of the buffer `bf`, which indicates the maximum number of characters that can be written to it.

* - `bytes`
  - `uint64_t`
  - The size in bytes that needs to be formatted.

* - `b1024`
  - `BOOL`
  - A boolean flag that determines whether to use base 1024 (binary) or base 1000 (decimal) for the size conversion.
:::


---

**Return Value**


This function does not return a value. Instead, it modifies the buffer pointed to by `bf` to contain the formatted size string.


**Notes**


Ensure that the buffer `bf` is sufficiently sized to accommodate the formatted string. The function does not check for buffer overflow, so it is the caller's responsibility to provide an adequately sized buffer.


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

