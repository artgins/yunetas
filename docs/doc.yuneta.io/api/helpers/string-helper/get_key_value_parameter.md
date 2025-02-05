<!-- ============================================================== -->
(get_key_value_parameter())=
# `get_key_value_parameter()`
<!-- ============================================================== -->


The `get_key_value_parameter()` function extracts a key-value pair from a given string `s` formatted as `key=value` or `key='value'`. 
The function modifies the input string `s` by inserting null characters (`\0`) to separate the key and value. 
The key is stored in the memory location pointed to by `key`, and the value is returned as the function's result. 
This function is useful for parsing strings containing key-value pairs, such as configuration parameters or command-line arguments.


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

char *get_key_value_parameter(
    char    *s,
    char    **key,
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
  - The input string containing the key-value pair. This string will be modified in-place.

* - `key`
  - `char **`
  - Pointer to a `char *` where the extracted key will be stored.

* - `save_ptr`
  - `char **`
  - Pointer to a `char *` used to save the parsing state between successive calls. This is typically used for iterative parsing.
:::


---

**Return Value**


The function returns a pointer to the extracted value as a null-terminated string. 
If no valid key-value pair is found, the function returns `NULL`.


**Notes**


- The input string `s` is modified in-place, so ensure that the original content is no longer needed or is backed up.
- The function expects the input string to be properly formatted. Malformed strings may lead to undefined behavior.
- Quoted values (e.g., `key='value'`) are supported, and the quotes are removed in the returned value.


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

