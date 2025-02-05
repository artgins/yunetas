<!-- ============================================================== -->
(tdump())=
# `tdump()`
<!-- ============================================================== -->


The `tdump()` function prints a hexadecimal and ASCII dump of a given binary buffer.
It is useful for debugging purposes, allowing a structured visualization of raw data.
The output format includes both the hexadecimal representation and the corresponding ASCII characters.


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
    const char    *prefix,
    const uint8_t *s,
    size_t        len,
    view_fn_t     view,
    int           nivel
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
  - A string prefix that will be prepended to each line of the dump.

* - `s`
  - `const uint8_t *`
  - Pointer to the binary data to be dumped.

* - `len`
  - `size_t`
  - The length of the binary data in bytes.

* - `view`
  - `view_fn_t`
  - A function pointer used to format and print the output.

* - `nivel`
  - `int`
  - The indentation level for formatting the output.
:::


---

**Return Value**


This function does not return a value.


**Notes**


- The `view` function should be a printf-like function that handles formatted output.
- The function prints both the hexadecimal and ASCII representation of the data.
- Useful for debugging binary data, network packets, or file contents.


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

