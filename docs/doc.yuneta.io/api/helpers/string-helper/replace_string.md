<!-- ============================================================== -->
(replace_string())=
# `replace_string()`
<!-- ============================================================== -->


The `replace_string()` function searches for occurrences of the substring `old` 
within the string `str` and replaces them with the substring `snew`. 
The function returns a newly allocated string containing the modified content. 
The caller is responsible for freeing the returned string using `free()`.


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

char *replace_string(
    const char *str,
    const char *old,
    const char *snew
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
  - The input string in which replacements will be made.

* - `old`
  - `const char *`
  - The substring to be replaced.

* - `snew`
  - `const char *`
  - The substring that will replace occurrences of `old`.

:::


---

**Return Value**


Returns a newly allocated string with all occurrences of `old` replaced by `snew`. 
If `old` is not found in `str`, a duplicate of `str` is returned. 
The caller must free the returned string using `free()`.


**Notes**


- If `str`, `old`, or `snew` is `NULL`, the function returns `NULL`.
- If `old` is an empty string, the function returns a duplicate of `str`.
- The function dynamically allocates memory for the new string, 
  so it is the caller's responsibility to free the returned string.


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

