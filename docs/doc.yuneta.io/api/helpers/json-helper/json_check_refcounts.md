<!-- ============================================================== -->
(json_check_refcounts())=
# `json_check_refcounts()`
<!-- ============================================================== -->


The `json_check_refcounts` function checks the reference counts of a JSON object recursively and reports if any reference count exceeds the specified maximum value.

If the reference count of any JSON object within the hierarchy exceeds the `max_refcount`, the function will set the `result` parameter to a non-zero value.

This function is useful for debugging and ensuring proper memory management when working with JSON objects.


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
    json_t *kw,
    int max_refcount,
    int *result
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
  - The JSON object to check reference counts recursively.

* - `max_refcount`
  - `int`
  - The maximum allowed reference count.

* - `result`
  - `int *`
  - Pointer to an integer that will be set to a non-zero value if any reference count exceeds the maximum.
:::


---

**Return Value**


The function returns an integer indicating the status of the reference count check. It does not return a specific value but uses the `result` parameter to signal if any reference count exceeds the specified maximum.


**Notes**


- This function is helpful for detecting memory leaks or improper handling of JSON objects.
- It recursively traverses the JSON object hierarchy to check reference counts.


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

