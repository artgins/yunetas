<!-- ============================================================== -->
(istream_new_gbuffer())=
# `istream_new_gbuffer()`
<!-- ============================================================== -->


This function creates a new `gbuffer` associated with the specified `istream`. 
It allocates memory for the `gbuffer` based on the provided `data_size` and `max_size`. 
If the `istream` already has an associated `gbuffer`, it will be replaced with the new one.
The function ensures that the `istream` can handle the specified sizes and manages memory allocation accordingly.


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

int istream_new_gbuffer(
    istream_h istream,
    size_t    data_size,
    size_t    max_size
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `istream`
  - `istream_h`
  - The input stream for which the new `gbuffer` is being created.

* - `data_size`
  - `size_t`
  - The size of the data to be stored in the new `gbuffer`.

* - `max_size`
  - `size_t`
  - The maximum size that the new `gbuffer` can grow to.
:::


---

**Return Value**


Returns an integer indicating the success or failure of the operation. 
A return value of 0 indicates success, while a negative value indicates an error occurred during the creation of the `gbuffer`.


**Notes**


If the `istream` already has an associated `gbuffer`, it will be destroyed before creating the new one. 
Ensure to handle any potential memory leaks by managing the lifecycle of the `istream` and its associated `gbuffer` properly.


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

