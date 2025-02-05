<!-- ============================================================== -->
(split3())=
# `split3()`
<!-- ============================================================== -->


The `split3()` function splits the input string `str` using the delimiter characters specified in `delim`.
It returns an array of strings, where each element is a substring extracted from `str`.
Unlike [`split2()`](#split2), this function includes empty strings in the resulting list.
The caller must free the returned list using [`split_free3()`](#split_free3).


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

const char **split3(
    const char *str,
    const char *delim,
    int        *plist_size
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `str`
  - `const char *`
  - The input string to be split.

* - `delim`
  - `const char *`
  - A string containing delimiter characters used to split `str`.

* - `plist_size`
  - `int *`
  - A pointer to an integer that will be set to the number of elements in the returned list. Can be `NULL`.
:::


---

**Return Value**


A pointer to an array of strings (`const char **`), where each element is a substring extracted from `str`.
The last element of the array is `NULL` to indicate the end of the list.
The caller must free the returned list using [`split_free3()`](#split_free3).


**Notes**


- Unlike [`split2()`](#split2), `split3()` includes empty strings in the result.
- The returned list must be freed using [`split_free3()`](#split_free3).


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

