<!-- ============================================================== -->
(istream_is_completed())=
# `istream_is_completed()`
<!-- ============================================================== -->

Checks if the given [`istream_h`](istream_h) has completed its current reading operation. Completion is determined based on whether the defined criteria (e.g., reading up to a delimiter or a specific number of bytes) have been met.

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
PUBLIC BOOL istream_is_completed(
    istream_h   istream
);
```

**Parameters**

:::list-table
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description

* - `istream`
  - `istream_h`
  - The handle to the istream to check for completion status.
:::

---

**Return Value**

- Returns `TRUE` if the istream's operation (reading until a delimiter or a specific number of bytes) is complete.
- Returns `FALSE` if the operation is not yet complete or if the `istream` is invalid.

**Notes**
- The function checks the `completed` flag in the internal structure of the istream.
- Logs an error if the provided `istream` is `NULL`.

**Example Usage**
This function is typically used to verify whether an operation, such as reading a specific number of bytes or until a delimiter, has successfully completed before further processing.

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
