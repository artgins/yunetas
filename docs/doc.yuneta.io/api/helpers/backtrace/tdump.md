<!-- ============================================================== -->
(tdump())=
# `tdump()`
<!-- ============================================================== -->


The `tdump` function is used to dump the content of a buffer in a human-readable format with a specified prefix.

When called, it displays the content of the buffer `s` with a length of `len` bytes. The output is formatted with the provided `prefix` string and additional indentation specified by `nivel`. The function uses the `view_fn_t` function pointer to output the formatted data.

This function is commonly used for debugging purposes to visualize the content of binary data in a readable format.


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

void tdump(
    const char *prefix,
    const uint8_t *s,
    size_t len,
    view_fn_t view,
    int nivel
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `prefix`
  - `const char *`
  - The prefix string to be displayed before each line of the dump.

* - `s`
  - `const uint8_t *`
  - Pointer to the buffer containing the data to be dumped.

* - `len`
  - `size_t`
  - The length of the buffer `s` in bytes.

* - `view`
  - `view_fn_t`
  - Function pointer used to output the formatted data.

* - `nivel`
  - `int`
  - Indentation level for the dump output.
:::


---

**Return Value**


This function does not return any value.


**Notes**


- The `tdump` function is commonly used for debugging purposes to visualize binary data in a human-readable format.
- It is important to ensure that the `view_fn_t` function pointer is properly defined to handle the output of the formatted data.


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

