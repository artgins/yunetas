<!-- ============================================================== -->
(istream_read_until_delimiter())=
# `istream_read_until_delimiter()`
<!-- ============================================================== -->


The `istream_read_until_delimiter()` function reads data from the input stream (`istream`) until the specified delimiter is encountered. 
The function triggers the specified event (`event`) once the delimiter is found. This allows for processing of the data read up to the delimiter.


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

int istream_read_until_delimiter(
    istream_h       istream,
    const char      *delimiter,
    size_t          delimiter_size,
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
  - Handle to the input stream from which data is read.

* - `delimiter`
  - `const char *`
  - Pointer to the delimiter string that marks the end of the data to be read.

* - `delimiter_size`
  - `size_t`
  - Size of the delimiter in bytes.

* - `event`
  - `gobj_event_t`
  - Event to be triggered when the delimiter is found.
:::


---

**Return Value**


Returns `0` on success, or a negative value if an error occurs. The error may indicate issues such as invalid parameters or internal stream errors.


**Notes**


- The function does not consume the delimiter itself; it only reads up to the delimiter.
- Ensure that the `istream` is properly initialized before calling this function.
- The `event` parameter must be a valid event that can handle the data read from the stream.


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

