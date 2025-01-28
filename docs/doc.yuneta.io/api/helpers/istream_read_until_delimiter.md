<!-- ============================================================== -->
(istream_read_until_delimiter())=
# `istream_read_until_delimiter()`
<!-- ============================================================== -->

The `istream_read_until_delimiter` function configures the input stream to read data until a specified delimiter is encountered. It initializes and sets the delimiter and the event name associated with the operation.

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
PUBLIC int istream_read_until_delimiter(
    istream_h       istream,
    const char     *delimiter,
    size_t          delimiter_size,
    gobj_event_t    event
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - **Parameter**
  - **Type**
  - **Description**

* - `istream_h`
  - `istream`
  - Handle to the input stream instance.

* - `delimiter`
  - `const char *`
  - Pointer to the delimiter string.

* - `delimiter_size`
  - `size_t`
  - Size of the delimiter string in bytes. Must be greater than `0`.

* - `event`
  - `gobj_event_t`
  - Name of the event (as a unique pointer) to trigger once the delimiter is found.
:::

---

**Return Value**

- Returns `0` on success.
- Returns `-1` on failure due to:
  - `delimiter_size` being `0` or less.
  - Memory allocation failures for `delimiter`.

**Notes**

- **Memory Management:**
  - Frees any previously allocated memory for `delimiter` before setting new values.
  - Dynamically allocates memory for the `delimiter`.
- **Validation:**
  - Validates that `delimiter_size` is greater than `0`. If not, an error is logged, and the function returns `-1`.
- **Event Name Handling:**
  - The `event` parameter is stored as a `gobj_event_t`, allowing fast comparisons using pointers instead of string matching.
- **Stream State:**
  - Resets the `num_bytes` counter and sets the `completed` flag to `FALSE`.


**Use Case**

This function is typically used to set up a stream to monitor incoming data and trigger an event when a specific delimiter sequence is detected.

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
