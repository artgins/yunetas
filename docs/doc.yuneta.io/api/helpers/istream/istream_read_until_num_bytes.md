<!-- ============================================================== -->
(istream_read_until_num_bytes())=
# `istream_read_until_num_bytes()`
<!-- ============================================================== -->


The `istream_read_until_num_bytes()` function reads data from the input stream until the specified number of bytes (`num_bytes`) is available. Once the required number of bytes is read, the function triggers the specified event (`event`) to notify the caller. This function is useful for processing streams where a fixed amount of data needs to be read before further processing.


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

int istream_read_until_num_bytes(
    istream_h       istream,
    size_t          num_bytes,
    gobj_event_t    event
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
  - Handle to the input stream from which data is being read.

* - `num_bytes`
  - `size_t`
  - The number of bytes to read from the input stream before triggering the event.

* - `event`
  - `gobj_event_t`
  - The event to be triggered once the specified number of bytes is read.

:::


---

**Return Value**


Returns `0` on success, or a negative value if an error occurs during the operation.


**Notes**


- The function does not consume the data from the stream; it only ensures that the specified number of bytes is available for reading.
- If the stream does not contain enough data, the function will wait until the required amount is available.
- Ensure that the `istream` handle is valid and properly initialized before calling this function.


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

