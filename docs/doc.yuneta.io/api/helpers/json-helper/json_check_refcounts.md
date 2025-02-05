<!-- ============================================================== -->
(json_check_refcounts())=
# `json_check_refcounts()`
<!-- ============================================================== -->


The `json_check_refcounts()` function checks the reference counts of a given JSON object (`kw`) and ensures that they do not exceed a specified maximum value (`max_refcount`). It traverses the JSON object and its children recursively to validate their reference counts. The result of the check is stored in the `result` parameter, which must be initialized to 0 before calling the function.

This function is useful for debugging and ensuring the integrity of JSON objects in memory, particularly in scenarios where reference counting is critical.


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

int json_check_refcounts(
    json_t *kw,       // not owned
    int     max_refcount,
    int    *result    // firstly initialize to 0
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `kw`
  - `json_t *`
  - The JSON object to check. This parameter is not owned by the function.

* - `max_refcount`
  - `int`
  - The maximum allowed reference count for any JSON object in the hierarchy.

* - `result`
  - `int *`
  - Pointer to an integer where the result of the check will be stored. Must be initialized to 0 before calling the function.

:::


---

**Return Value**


Returns an integer indicating the success or failure of the operation:
- `0`: All reference counts are within the allowed limit.
- `-1`: At least one reference count exceeds the specified maximum.


**Notes**


- The `result` parameter must be initialized to 0 before calling the function.
- This function does not modify the input JSON object (`kw`).
- Use this function for debugging purposes to ensure that JSON objects are not over-referenced.


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

