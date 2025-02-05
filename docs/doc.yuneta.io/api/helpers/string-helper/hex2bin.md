<!-- ============================================================== -->
(hex2bin())=
# `hex2bin()`
<!-- ============================================================== -->


The `hex2bin()` function converts a hexadecimal string representation into its binary equivalent.

It takes a hexadecimal string `hex` of length `hex_len` and converts it into binary form, storing the result in `bf`. 
The function ensures that the output does not exceed `bfsize` bytes. 
If `out_len` is not `NULL`, it stores the length of the resulting binary data.

The function returns `bf` on success.


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
    char        *bf,
    int         bfsize,
    const char  *hex,
    size_t      hex_len,
    size_t      *out_len
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
  - Buffer to store the binary output.

* - `bfsize`
  - `int`
  - Size of the `bf` buffer in bytes.

* - `hex`
  - `const char *`
  - Input hexadecimal string to be converted.

* - `hex_len`
  - `size_t`
  - Length of the hexadecimal string.

* - `out_len`
  - `size_t *`
  - Pointer to store the length of the resulting binary data (optional).

:::


---

**Return Value**


Returns a pointer to `bf` containing the binary representation of the input hexadecimal string.


**Notes**


- The function does not allocate memory; the caller must provide a sufficiently large buffer `bf`.
- The hexadecimal string `hex` should contain only valid hexadecimal characters (`0-9`, `a-f`, `A-F`).
- If `out_len` is provided, it will be set to the number of bytes written to `bf`.


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

