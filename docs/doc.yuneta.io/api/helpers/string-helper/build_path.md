<!-- ============================================================== -->
(build_path())=
# `build_path()`
<!-- ============================================================== -->


The `build_path()` function constructs a file path by concatenating multiple string segments. 
It takes a buffer `bf` and its size `bfsize` as inputs, along with a variable number of string arguments. 
The resulting path is stored in the provided buffer `bf`. 
This function ensures that the constructed path does not exceed the buffer size.


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

char *build_path(
    char    *bf,
    size_t  bfsize,
    ...
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `bf`
  - `char *`
  - Pointer to the buffer where the constructed path will be stored.

* - `bfsize`
  - `size_t`
  - Size of the buffer `bf` in bytes.

* - `...`
  - `variadic`
  - Variable number of string arguments to be concatenated to form the path.

:::


---

**Return Value**


A pointer to the buffer `bf` containing the constructed path. 
If the buffer size is insufficient, the resulting path may be truncated.


**Notes**


- Ensure that the buffer `bf` is large enough to hold the constructed path, including the null terminator.
- The function does not allocate memory; it relies on the caller to provide a pre-allocated buffer.
- The variadic arguments must be null-terminated strings, and the list of arguments must end with a `NULL` pointer.


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

