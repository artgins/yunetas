<!-- ============================================================== -->
(istream_read_until_delimiter())=
# `istream_read_until_delimiter()`
<!-- ============================================================== -->


Reads data from the input stream until the specified delimiter is encountered. The function will fill the input stream buffer with the data read and will trigger the specified event once the delimiter is found. This is useful for processing data streams where messages or data segments are separated by specific delimiters.


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
    istream_h istream,
    const char *delimiter,
    size_t delimiter_size,
    gobj_event_t event
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
  - Handle to the input stream from which data is to be read.

* - `delimiter`
  - `const char *`
  - Pointer to the delimiter string that marks the end of the data to be read.

* - `delimiter_size`
  - `size_t`
  - Size of the delimiter string.

* - `event`
  - `gobj_event_t`
  - Event to be triggered once the delimiter is encountered.
:::


---

**Return Value**


Returns an integer indicating the success or failure of the read operation. A return value of 0 typically indicates success, while a negative value indicates an error.


**Notes**


Ensure that the delimiter provided is valid and that the input stream is properly initialized before calling this function. The function may block if the delimiter is not found, depending on the implementation of the input stream.


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

