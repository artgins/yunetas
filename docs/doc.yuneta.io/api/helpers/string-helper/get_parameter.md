<!-- ============================================================== -->
(get_parameter())=
# `get_parameter()`
<!-- ============================================================== -->


The `get_parameter()` function extracts a parameter from a string `s`, 
delimited by blanks (`\b`, `\t`) or quotes (`'`, `"`). 
The input string `s` is modified during the process, with null characters inserted to separate tokens. 
This function is useful for parsing command-line arguments or similar input formats.


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

char *get_parameter(
    char    *s,
    char    **save_ptr
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `s`
  - `char *`
  - The input string to be parsed. It will be modified in-place.

* - `save_ptr`
  - `char **`
  - A pointer to save the context for subsequent calls to `get_parameter()`. 
    This allows the function to continue parsing the same string across multiple calls.

:::


---

**Return Value**


Returns a pointer to the extracted parameter as a null-terminated string. 
If no more parameters are available, the function returns `NULL`.


**Notes**


- The input string `s` is modified in-place, so ensure that it is writable.
- This function is designed to handle quoted strings, allowing parameters to contain spaces if enclosed in quotes.
- Use the `save_ptr` parameter to maintain parsing state across multiple calls.


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

