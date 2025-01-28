

<!-- ============================================================== -->
(istream_read_until_num_bytes())=
# `istream_read_until_num_bytes()`
<!-- ============================================================== -->

The `istream_read_until_num_bytes` function configures an input stream (`istream`) to read a specific number of bytes before triggering a designated event.

<!------------------------------------------------------------>
<!--                    Prototypes                          -->
<!------------------------------------------------------------>

``````{tab-set}

`````{tab-item} C

<!--====================================================-->
<!--                    Tab C                           -->
<!--====================================================-->

**Prototype**
PUBLIC int istream_read_until_num_bytes(
    istream_h       istream,
    size_t          num_bytes,
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

* - `num_bytes`
  - `size_t`
  - The number of bytes to read before triggering the event.

* - `event`
  - `gobj_event_t`
  - The event name (as a unique pointer) to trigger once the specified number of bytes is read.
:::

---

**Return Value**

- Returns `0` on success.

**Notes**

- **State Initialization:**
  - The `num_bytes` attribute is set to the specified value, determining how much data the input stream should process before the event is triggered.
  - The `event_name` attribute is set to the provided `event`, allowing fast pointer comparisons.
  - The `completed` attribute is reset to `FALSE` to indicate that the stream is not yet finished processing.
  - The `delimiter` attribute is set to `NULL` as this function does not utilize a delimiter.

**Use Case**

This function is useful when an exact amount of data needs to be processed from an input stream, such as reading fixed-length records or packetized data.

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
