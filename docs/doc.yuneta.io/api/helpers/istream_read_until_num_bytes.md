<!-- ============================================================== -->
(istream_read_until_num_bytes())=
# `istream_read_until_num_bytes()`
<!-- ============================================================== -->

Configures the istream to read data until a specified number of bytes (`num_bytes`) is accumulated. Once the condition is met, the specified event (`event`) will be triggered for the associated GObj.

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
* - Key
  - Type
  - Description

* - `istream`
  - `istream_h`
  - The handle to the istream where data will be read.

* - `num_bytes`
  - `size_t`
  - The number of bytes to accumulate before marking the reading operation as complete.

* - `event`
  - `gobj_event_t`
  - The event to be triggered when the specified number of bytes is reached.
:::

---

**Return Value**

Returns `0` on successful configuration.
Returns `-1` if the `istream` handle is invalid.

**Notes**

- This function resets the delimiter configuration and sets the reading mode to accumulate data based on `num_bytes`.
- Once the specified number of bytes is accumulated, the istream triggers the specified event (`event`) for its associated GObj.
- The `event` name must be a unique pointer for optimized comparison during event dispatching.

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
