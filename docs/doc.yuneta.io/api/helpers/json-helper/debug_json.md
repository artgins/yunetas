<!-- ============================================================== -->
(debug_json())=
# `debug_json()`
<!-- ============================================================== -->


The `debug_json` function is used to print debug information about a JSON object. It outputs the contents of the JSON object in a readable format, which can be useful for debugging purposes. The function takes a label to identify the output and a verbosity flag that determines the level of detail in the output.


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

int debug_json(
    const char *label,
    json_t *jn,
    BOOL verbose
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `label`
  - `const char *`
  - A string label that will be printed before the JSON output to identify the context of the debug information.

* - `jn`
  - `json_t *`
  - A pointer to the JSON object that will be printed. This object is not owned by the function and should not be freed by it.

* - `verbose`
  - `BOOL`
  - A boolean flag that indicates whether to print additional details about the JSON object. If TRUE, more detailed information will be included in the output.
:::


---

**Return Value**


The function returns an integer value. Typically, it returns 0 on success and a negative value on failure, indicating an error in processing the JSON object.


**Notes**


This function is primarily intended for debugging purposes. The output format may vary based on the verbosity level specified. Ensure that the JSON object passed to the function is valid to avoid unexpected behavior.


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

