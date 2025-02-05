<!-- ============================================================== -->
(bin2hex())=
# `bin2hex()`
<!-- ============================================================== -->


The `bin2hex()` function converts a binary data buffer into its hexadecimal string representation. 
The resulting hexadecimal string is stored in the provided buffer `bf`. 
This function is useful for representing binary data in a human-readable format.


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
    char        *bf,
    int         bfsize,
    const uint8_t *bin,
    size_t      bin_len
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
  - Size of the buffer `bf` in bytes. It must be large enough to hold the resulting hexadecimal string and the null terminator.

* - `bin`
  - `const uint8_t *`
  - Pointer to the binary data to be converted.

* - `bin_len`
  - `size_t`
  - Length of the binary data in bytes.
:::


---

**Return Value**


Returns a pointer to the buffer `bf` containing the hexadecimal string representation of the binary data. 
If the buffer size `bfsize` is insufficient, the behavior is undefined.


**Notes**


- The resulting hexadecimal string will be null-terminated.
- Ensure that `bfsize` is at least `(2 * bin_len + 1)` to accommodate the hexadecimal representation and the null terminator.
- This function does not allocate memory; the caller must provide a sufficiently large buffer.


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

