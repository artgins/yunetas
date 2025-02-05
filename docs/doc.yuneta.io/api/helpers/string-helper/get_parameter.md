<!-- ============================================================== -->
(get_parameter())=
# `get_parameter()`
<!-- ============================================================== -->


Extracts a parameter from a given string `s`, which is delimited by whitespace characters (spaces or tabs) or quotes (single or double). The function modifies the input string by inserting null characters at the delimiters to separate the parameter from the rest of the string. The `save_ptr` is used to maintain context for subsequent calls, allowing for continued extraction from the same string.


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
    char   *s,
    char **save_ptr
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
  - The input string from which the parameter will be extracted. This string will be modified by the function.

* - `save_ptr`
  - `char **`
  - A pointer to a pointer that stores the context for the next extraction. It allows the function to keep track of the position in the string for subsequent calls.

:::


---

**Return Value**


Returns a pointer to the extracted parameter as a string. If no parameter can be extracted, it returns NULL.


**Notes**


The input string `s` will be modified, and it is important to ensure that it is not used after the function call unless it is reinitialized. The function is designed to handle strings that may contain quoted values, and it will correctly parse these while ignoring whitespace outside of quotes.


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

