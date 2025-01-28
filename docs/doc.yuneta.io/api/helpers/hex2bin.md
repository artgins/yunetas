

<!-- ============================================================== -->
(hex2bin())=
# `hex2bin()`
<!-- ============================================================== -->

The `hex2bin` function converts a hexadecimal string (`hex`) into its binary representation, storing the result in a provided buffer (`bf`). It uses a decoding table to map hexadecimal characters to their corresponding values and processes the input string two characters at a time to produce binary bytes.

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
PUBLIC char *hex2bin(
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

* - **Parameter**
  - **Type**
  - **Description**

* - `bf`
  - `char *`
  - Pointer to the output buffer where the binary data will be stored.

* - `bfsize`
  - `int`
  - The size of the output buffer in bytes.

* - `hex`
  - `const char *`
  - The input hexadecimal string to be converted.

* - `hex_len`
  - `size_t`
  - The length of the input hexadecimal string.

* - `out_len`
  - `size_t *`
  - Pointer to a variable where the length of the binary output will be stored. Can be `NULL` if not needed.
:::
---

**Return Value**

- Returns a pointer to the output buffer `bf` containing the binary data.
- Returns `NULL` if an error occurs (e.g., invalid hex character or buffer overflow).

**Notes**

- **Hexadecimal Decoding:**
  - The function uses a decoding table (`base16_decoding_table1`) to map valid hexadecimal characters (`0-9`, `a-f`, `A-F`) to their respective values.
  - If an invalid character is encountered, the function stops processing.
- **Buffer Size Check:**
  - Ensures that the binary data does not exceed the size of the output buffer (`bfsize`).
- **Even/Odd Logic:**
  - Even indices of the hexadecimal input are processed as the high nibble (4 bits) of the binary byte, and odd indices as the low nibble.
- **Output Length:**
  - The total number of bytes written to the output buffer is stored in `out_len` if it is not `NULL`.

**Example**
```c
char buffer[32];
size_t out_len;
const char *hex_string = "48656c6c6f20576f726c64"; // "Hello World" in hex
hex2bin(buffer, sizeof(buffer), hex_string, strlen(hex_string), &out_len);
// buffer now contains the binary representation of "Hello World"
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
