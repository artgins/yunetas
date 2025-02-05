<!-- ============================================================== -->
(translate_string())=
# `translate_string()`
<!-- ============================================================== -->


The `translate_string` function is designed to transform a source string (`from`) into a target string (`to`) based on specified mappings. It replaces characters in the source string according to the mappings provided in `mk_from` and `mk_to`. Each character in `mk_from` corresponds to a character in `mk_to`, and characters in `from` that match those in `mk_from` are replaced with the corresponding characters from `mk_to`. The function ensures that the resulting string does not exceed the specified length (`tolen`).


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
    char *to,
    int tolen,
    const char *from,
    const char *mk_to,
    const char *mk_from
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
  - A pointer to the destination buffer where the translated string will be stored.

* - `tolen`
  - `int`
  - The maximum length of the destination buffer to prevent overflow.

* - `from`
  - `const char *`
  - A pointer to the source string that needs to be translated.

* - `mk_to`
  - `const char *`
  - A string containing characters that will replace those in `from`.

* - `mk_from`
  - `const char *`
  - A string containing characters to be replaced in `from`.
:::


---

**Return Value**


The function returns a pointer to the destination string (`to`). If no characters were replaced, it returns the original `to` pointer.


**Notes**


Ensure that the destination buffer (`to`) is large enough to hold the translated string, including the null terminator. If `tolen` is less than the length of the resulting string, it may lead to buffer overflow.


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

