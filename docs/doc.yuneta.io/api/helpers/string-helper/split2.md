<!-- ============================================================== -->
(split2())=
# `split2()`
<!-- ============================================================== -->


The `split2()` function splits a given string `str` into a list of substrings based on the specified delimiter `delim`. 
It returns an array of strings, excluding any empty substrings. The size of the resulting list is stored in `list_size` 
if it is not null. The returned list must be freed using [`split_free2()`](#split_free2) to avoid memory leaks.


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

const char **split2(
    const char *str,
    const char *delim,
    int        *list_size
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
  - The delimiter string used to split the input string.

* - `list_size`
  - `int *`
  - Pointer to an integer where the size of the resulting list will be stored. Can be null.

:::


---

**Return Value**


A pointer to an array of strings containing the substrings of `str` split by `delim`. 
The array does not include empty substrings. The caller must free the returned array using [`split_free2()`](#split_free2).


**Notes**


- The function does not include empty substrings in the resulting list.
- The returned array must be freed using [`split_free2()`](#split_free2) to avoid memory leaks.
- If `list_size` is null, the size of the resulting list will not be stored.


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

