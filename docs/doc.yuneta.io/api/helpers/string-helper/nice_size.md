<!-- ============================================================== -->
(nice_size())=
# `nice_size()`
<!-- ============================================================== -->


The `nice_size` function formats a given number of bytes into a human-readable size representation. It converts the number of bytes into a more readable format, such as KB, MB, GB, etc., based on the size. The function allows choosing between using base 1024 (binary) or base 1000 (decimal) for size conversions.


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
  - Buffer to store the formatted size string.
  
* - `bfsize`
  - `size_t`
  - Size of the buffer `bf`.
  
* - `bytes`
  - `uint64_t`
  - Number of bytes to convert into a human-readable size.
  
* - `b1024`
  - `BOOL`
  - Flag to indicate whether to use base 1024 (TRUE) or base 1000 (FALSE) for size conversions.
:::


---

**Return Value**


This function does not return a value. It populates the buffer `bf` with the human-readable size representation of the input number of bytes.


**Notes**


- The function formats the size using binary prefixes (KiB, MiB, GiB, etc.) if `b1024` is TRUE, and decimal prefixes (KB, MB, GB, etc.) if `b1024` is FALSE.
- The formatted size string is stored in the buffer `bf`, so ensure that `bf` has enough space to accommodate the formatted size.


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

