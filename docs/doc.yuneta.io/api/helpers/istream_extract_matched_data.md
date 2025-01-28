<!-- ============================================================== -->
(istream_extract_matched_data())=
# `istream_extract_matched_data()`
<!-- ============================================================== -->

Extracts the matched data from the [`istream_h`](istream_h). The function returns a pointer to the matched data and resets the completion status of the stream.

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
PUBLIC char *istream_extract_matched_data(
    istream_h   istream,
    size_t      *len
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
  - The handle to the istream from which matched data will be extracted.

* - `len`
  - `size_t *`
  - A pointer to a variable where the length of the matched data will be stored. Can be `NULL` if the length is not required.
:::

---

**Return Value**

Returns a pointer to the matched data if successful.
Returns `NULL` if the `istream` is invalid, incomplete, or if there is no [`gbuffer_t *`](gbuffer_t) associated with the istream.

**Notes**
- The matched data is removed from the associated [`gbuffer_t *`](gbuffer_t).
- After extracting the data, the `istream` completion status is reset (`completed = FALSE`).
- This function logs an error if the `istream` is `NULL`, not completed, or has no valid [`gbuffer_t *`](gbuffer_t).

**Example Usage**
This function can be used to retrieve and process data from an istream once it has been matched by a delimiter or a byte threshold.

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
