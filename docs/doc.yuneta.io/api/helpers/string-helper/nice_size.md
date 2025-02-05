<!-- ============================================================== -->
(nice_size())=
# `nice_size()`
<!-- ============================================================== -->


The `nice_size` function formats a given size in bytes into a human-readable string representation. It can express the size in either binary (base 1024) or decimal (base 1000) format, depending on the value of the `b1024` parameter. The formatted string is stored in the buffer provided by the `bf` parameter, ensuring that it does not exceed the specified `bfsize`.


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
  - A pointer to the buffer where the formatted size string will be stored.

* - `bfsize`
  - `size_t`
  - The size of the buffer `bf`, which limits the maximum length of the output string.

* - `bytes`
  - `uint64_t`
  - The size in bytes to be formatted.

* - `b1024`
  - `BOOL`
  - A flag indicating whether to use binary (TRUE) or decimal (FALSE) formatting.
:::


---

**Return Value**


The `nice_size` function does not return a value. Instead, it writes the formatted size string directly into the provided buffer.


**Notes**


Ensure that the buffer `bf` is large enough to hold the formatted string, including the null terminator. If the buffer size is insufficient, the output may be truncated.


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

