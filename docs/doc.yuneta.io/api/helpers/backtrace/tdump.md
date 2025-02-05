<!-- ============================================================== -->
(tdump())=
# `tdump()`
<!-- ============================================================== -->


The `tdump` function is designed to output a formatted dump of a binary data buffer. It takes a prefix string that is prepended to each line of output, allowing for easy identification of the data being dumped. The function also accepts a pointer to the data buffer, its length, a callback function for custom viewing of the data, and a level of indentation (nivel) to control the formatting of the output. This is particularly useful for debugging purposes, where a clear representation of binary data is required.


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
  - A string that will be prefixed to each line of the output.

* - `s`
  - `const uint8_t *`
  - A pointer to the binary data buffer to be dumped.

* - `len`
  - `size_t`
  - The length of the data buffer.

* - `view`
  - `view_fn_t`
  - A function pointer for custom viewing of the data.

* - `nivel`
  - `int`
  - An integer representing the level of indentation for the output.
:::


---

**Return Value**


This function does not return a value. It outputs the formatted dump directly to the specified output stream or console.


**Notes**


The `view` function should be defined by the user to customize how the data is viewed. The `nivel` parameter allows for hierarchical representation of the data, which can be useful when dealing with nested structures.


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

