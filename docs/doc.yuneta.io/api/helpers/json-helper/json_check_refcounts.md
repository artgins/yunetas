<!-- ============================================================== -->
(json_check_refcounts())=
# `json_check_refcounts()`
<!-- ============================================================== -->


Checks the reference counts of the JSON object `kw`. This function traverses the JSON structure and verifies that the reference counts of all elements do not exceed the specified `max_refcount`. It is useful for debugging memory management issues in applications that manipulate JSON data.


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
    json_t *kw,          // not owned
    int max_refcount,
    int *result           // firstly initialize to 0
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
  - The JSON object to check. This parameter is not owned by the function and should not be freed by it.

* - `max_refcount`
  - `int`
  - The maximum allowed reference count for any element in the JSON object.

* - `result`
  - `int *`
  - A pointer to an integer where the function will store the result of the check. It should be initialized to 0 before calling the function.
:::


---

**Return Value**


Returns an integer indicating the success of the operation. A return value of 0 indicates that all reference counts are within the allowed limit, while a non-zero value indicates that at least one reference count exceeded `max_refcount`.


**Notes**


This function is primarily intended for debugging purposes. It is important to initialize the `result` parameter to 0 before calling the function to ensure accurate results.


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

