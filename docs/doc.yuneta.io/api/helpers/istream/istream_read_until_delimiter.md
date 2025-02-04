<!-- ============================================================== -->
(istream_read_until_delimiter())=
# `istream_read_until_delimiter()`
<!-- ============================================================== -->


This function reads data from the input stream until a specified delimiter is encountered. It then triggers the specified event with the extracted data.

The function is part of the istream module and is used to handle input stream data efficiently by reading until a delimiter is found.


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
    istream_h   istream,
    const char  *delimiter,
    size_t      delimiter_size,
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
  - Handle to the input stream.

* - `delimiter`
  - `const char *`
  - The delimiter to search for in the input stream.

* - `delimiter_size`
  - `size_t`
  - Size of the delimiter string.

* - `event`
  - `gobj_event_t`
  - Event to trigger when the delimiter is found, passing the extracted data.
:::


---

**Return Value**


The function returns an integer indicating the success of reading until the delimiter:
- Returns the number of bytes consumed if successful.
- Returns -1 if an error occurs during the operation.


**Notes**


- This function is useful for efficiently extracting data from an input stream until a specific delimiter is encountered.
- It is recommended to check the return value to handle errors or to process the extracted data accordingly.


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

