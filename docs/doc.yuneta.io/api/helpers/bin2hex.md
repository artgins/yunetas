

<!-- ============================================================== -->
(bin2hex())=
# `bin2hex()`
<!-- ============================================================== -->

The `bin2hex` function converts a binary buffer (`bin`) into a hexadecimal string representation and stores the result in a provided buffer (`bf`).

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
PUBLIC char *bin2hex(
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

* - **Parameter**
  - **Type**
  - **Description**

* - `bf`
  - `char *`
  - Pointer to the output buffer where the hexadecimal string will be stored.

* - `bfsize`
  - `int`
  - The size of the output buffer in bytes. Must be large enough to hold the hexadecimal representation of `bin` (2 characters per byte plus the null terminator).

* - `bin`
  - `const uint8_t *`
  - The input binary data to be converted to a hexadecimal string.

* - `bin_len`
  - `size_t`
  - The length of the input binary data in bytes.
:::

---

**Return Value**

- Returns a pointer to the output buffer `bf` containing the hexadecimal string.

**Notes**
- **Output Buffer Size:**
  - Ensure the `bfsize` is at least `2 * bin_len + 1` to accommodate the full hexadecimal representation and the null terminator.
- **Conversion Logic:**
  - Each byte in the input binary data is converted into two hexadecimal characters using the helper function `byte_to_strhex`.
- **Termination:**
  - The output buffer is null-terminated to produce a valid C string.

**Example**
```c
char hex_buffer[64];
uint8_t binary_data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello" in binary
bin2hex(hex_buffer, sizeof(hex_buffer), binary_data, sizeof(binary_data));
// hex_buffer now contains "48656c6c6f"
```

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
