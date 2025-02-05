<!-- ============================================================== -->
(hex2bin())=
# `hex2bin()`
<!-- ============================================================== -->


The `hex2bin` function converts a hexadecimal string representation into its binary form. It takes a buffer to store the binary data and its size, along with the hexadecimal string and its length. The function returns the buffer containing the binary data.


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

char *hex2bin(
    char    *bf,
    int     bfsize,
    const char *hex,
    size_t  hex_len,
    size_t  *out_len
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
  - A pointer to the buffer where the binary data will be stored.

* - `bfsize`
  - `int`
  - The size of the buffer `bf`.

* - `hex`
  - `const char *`
  - A pointer to the hexadecimal string that needs to be converted.

* - `hex_len`
  - `size_t`
  - The length of the hexadecimal string.

* - `out_len`
  - `size_t *`
  - A pointer to a variable where the length of the output binary data will be stored.
:::


---

**Return Value**


The function returns a pointer to the buffer `bf` containing the binary data. If the conversion fails, it may return NULL.


**Notes**


Ensure that the buffer `bf` is large enough to hold the converted binary data. The function does not perform bounds checking on the input hexadecimal string.


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

