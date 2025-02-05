<!-- ============================================================== -->
(istream_new_gbuffer())=
# `istream_new_gbuffer()`
<!-- ============================================================== -->


The `istream_new_gbuffer()` function creates a new [`gbuffer_t *`](#gbuffer_t) instance and associates it with the given `istream_h` object. 
This function is useful for resetting or initializing the internal buffer of an input stream (`istream`) with specified size constraints.


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

int istream_new_gbuffer(
    istream_h   istream,
    size_t      data_size,
    size_t      max_size
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
  - The input stream handle where the new gbuffer will be created.

* - `data_size`
  - `size_t`
  - The initial size of the data buffer to allocate.

* - `max_size`
  - `size_t`
  - The maximum size the buffer can grow to.
:::


---

**Return Value**


Returns `0` on success, or a negative value if an error occurs during the creation of the new gbuffer.


**Notes**


- The function will overwrite any existing gbuffer associated with the `istream`.
- Ensure that the `istream` handle is valid before calling this function.
- This function is typically used in scenarios where the buffer needs to be reinitialized with new size constraints.


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

