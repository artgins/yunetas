<!-- ============================================================== -->
(bin2hex())=
# `bin2hex()`
<!-- ============================================================== -->


Converts a binary data buffer into its hexadecimal string representation. The function takes a buffer where the hexadecimal string will be stored, the size of that buffer, a pointer to the binary data, and the length of the binary data. The resulting hexadecimal string will be null-terminated.


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

char *bin2hex(
    char      *bf,
    int       bfsize,
    const uint8_t *bin,
    size_t    bin_len
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
  - Pointer to the buffer where the hexadecimal string will be stored.

* - `bfsize`
  - `int`
  - Size of the buffer `bf` in bytes.

* - `bin`
  - `const uint8_t *`
  - Pointer to the binary data that needs to be converted to hexadecimal.

* - `bin_len`
  - `size_t`
  - Length of the binary data in bytes.

:::


---

**Return Value**


Returns a pointer to the buffer containing the hexadecimal string. If the buffer is not large enough to hold the resulting string, the behavior is undefined.


**Notes**


Ensure that the buffer `bf` is large enough to hold the hexadecimal representation of the binary data, which is twice the size of the binary data plus one for the null terminator. If `bfsize` is less than `2 * bin_len + 1`, the function may not behave as expected.


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

