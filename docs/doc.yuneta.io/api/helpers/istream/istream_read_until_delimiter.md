<!-- ============================================================== -->
(istream_read_until_delimiter())=
# `istream_read_until_delimiter()`
<!-- ============================================================== -->

Configures the istream to read data until a specified delimiter is encountered. Once the delimiter is matched, the specified event (`event`) is triggered for the associated GObj.

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
  - The handle to the istream where data will be read.

* - `delimiter`
  - `const char *`
  - A pointer to the delimiter string to match in the incoming data.

* - `delimiter_size`
  - `size_t`
  - The size (in bytes) of the delimiter.

* - `event`
  - `gobj_event_t`
  - The event to be triggered when the delimiter is matched. This must be a unique pointer.
:::

---

**Return Value**

Returns `0` on successful configuration.
Returns `-1` if:
- `istream` is invalid.
- Memory allocation for the delimiter fails.
- `delimiter_size` is `0` or less.

**Notes**

- The function performs the following steps:
  1. Validates the input parameters.
  2. Allocates memory for the `delimiter` and copies its contents.
  3. Configures the `event` to be triggered when the delimiter is matched.
  4. Resets the reading mode and any previously configured `num_bytes`.
- The `event` must be a unique pointer to allow optimized comparison during event dispatching.
- Ensure that the `delimiter` and its `delimiter_size` accurately represent the boundary condition for data parsing.

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
