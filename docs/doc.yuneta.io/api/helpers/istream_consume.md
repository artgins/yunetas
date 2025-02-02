<!-- ============================================================== -->
(istream_consume())=
# `istream_consume()`
<!-- ============================================================== -->

Consumes data from the provided buffer (`bf`) and processes it into the istream. The function stops when the specified amount of data (`len`) is consumed or when the delimiter or required number of bytes is matched, triggering the configured event if applicable.

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
PUBLIC size_t istream_consume(
    istream_h   istream,
    char        *bf,
    size_t      len
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_h`
  - The handle to the istream where the data will be consumed.

* - `bf`
  - `char *`
  - The input buffer containing the data to process.

* - `len`
  - `size_t`
  - The length of the input buffer (`bf`) in bytes.
:::

---

**Return Value**


Returns the number of bytes successfully consumed from the buffer.

**Notes**
- If the istream is configured to read until a specified number of bytes (`num_bytes`):
  1. Data is appended to the internal gbuffer until the required number of bytes is reached.
  2. Once the requirement is met, the associated event is triggered.

- If the istream is configured to read until a delimiter:
  1. Data is appended byte-by-byte to the internal gbuffer.
  2. The delimiter is matched against the end of the gbuffer's data.
  3. If the delimiter is found, the associated event is triggered.

- The function performs the following checks:
  - If `len` is `0`, the function immediately returns `0`.
  - If the internal gbuffer is `NULL`, the function logs an error and returns `0`.
  - If the internal gbuffer becomes full while appending data, an error is logged, and the process stops.

- When the event is triggered:
  1. The current gbuffer is passed to the event handler.
  2. A new gbuffer is created to continue processing subsequent data.

- The `event` must be preconfigured using functions such as [`istream_read_until_delimiter`](#istream_read_until_delimiter) or [`istream_read_until_num_bytes`](#istream_read_until_num_bytes).

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
