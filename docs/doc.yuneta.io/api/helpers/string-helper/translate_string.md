<!-- ============================================================== -->
(translate_string())=
# `translate_string()`
<!-- ============================================================== -->


The `translate_string()` function translates characters in the input string `from` 
based on the mapping provided by `mk_from` and `mk_to`. Each character in `mk_from` 
is replaced by the corresponding character in `mk_to`. The translated string is 
stored in the buffer `to`, which has a maximum size of `tolen`. The function ensures 
that the output string is null-terminated.


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

char *translate_string(
    char        *to,
    int         tolen,
    const char  *from,
    const char  *mk_to,
    const char  *mk_from
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `to`
  - `char *`
  - Pointer to the buffer where the translated string will be stored.

* - `tolen`
  - `int`
  - The size of the `to` buffer, ensuring the translated string does not exceed this length.

* - `from`
  - `const char *`
  - The input string to be translated.

* - `mk_to`
  - `const char *`
  - A string containing the characters to replace with.

* - `mk_from`
  - `const char *`
  - A string containing the characters to be replaced.

:::


---

**Return Value**


Returns a pointer to the translated string stored in the `to` buffer.


**Notes**


- The `to` buffer must be large enough to hold the translated string, including the null terminator.
- If `mk_from` and `mk_to` have different lengths, only the characters up to the length of the shorter string are considered.
- The function does not handle overlapping memory between `to` and `from`.


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

