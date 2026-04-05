<!-- ============================================================== -->
(udpc_open())=
# `udpc_open()`
<!-- ============================================================== -->

`udpc_open()` initializes and opens a UDP client for logging, configuring its buffer size, frame size, and output format.

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
udpc_t udpc_open(
    const char       *url,
    const char       *bindip,
    const char       *if_name,
    size_t            bf_size,
    size_t            udp_frame_size,
    output_format_t   output_format,
    BOOL              exit_on_fail
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `url`
  - `const char *`
  - The destination URL for the UDP client.

* - `bindip`
  - `const char *`
  - The local IP address to bind the socket, or NULL for default.

* - `if_name`
  - `const char *`
  - The network interface name to bind the socket, or NULL for default.

* - `bf_size`
  - `size_t`
  - The buffer size in bytes; 0 defaults to 256 KB.

* - `udp_frame_size`
  - `size_t`
  - The maximum UDP frame size; 0 defaults to 1500 bytes.

* - `output_format`
  - `output_format_t`
  - The output format for logging; defaults to `OUTPUT_FORMAT_YUNETA` if invalid.

* - `exit_on_fail`
  - `BOOL`
  - If `TRUE`, the process exits on failure; otherwise, it continues.
:::

---

**Return Value**

Returns a `udpc_t` handle to the UDP client on success, or `NULL` on failure.

**Notes**

If `url` is empty or invalid, [`udpc_open()`](#udpc_open) returns `NULL`.
Memory is allocated dynamically for the buffer; ensure proper cleanup with [`udpc_close()`](#udpc_close).
If the socket cannot be created, the function logs an error and returns `NULL`.

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
