<!-- ============================================================== -->
(ISTREAM_CREATE())=
# `ISTREAM_CREATE()`
<!-- ============================================================== -->


The `ISTREAM_CREATE` macro is a utility for safely creating an `istream_h` instance. 
It ensures that if the variable `var` already holds an existing `istream_h` instance, 
it is destroyed before creating a new one. This prevents memory leaks or undefined 
behavior caused by reusing an existing `istream_h` without proper cleanup.

The macro logs an error message if the variable `var` is already initialized, 
indicating that the previous instance is being destroyed.


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

#define ISTREAM_CREATE(var, gobj, data_size, max_size)                  \
    if(var) {                                                           \
        gobj_log_error((gobj), LOG_OPT_TRACE_STACK,                     \
            "function",     "%s", __FUNCTION__,                         \
            "msgset",       "%s", MSGSET_INTERNAL_ERROR,                \
            "msg",          "%s", "istream_h ALREADY exists! Destroyed",  \
            NULL                                                        \
        );                                                              \
        istream_destroy(var);                                           \
    }                                                                   \
    (var) = istream_create((gobj), (data_size), (max_size));

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `var`
  - `istream_h`
  - The variable to hold the `istream_h` instance. If it already exists, it will be destroyed.

* - `gobj`
  - `hgobj`
  - The GObj instance associated with the `istream_h`.

* - `data_size`
  - `size_t`
  - The size of the data buffer for the `istream_h`.

* - `max_size`
  - `size_t`
  - The maximum size of the `istream_h` buffer.
:::


---

**Return Value**


This macro does not return a value. It initializes the `var` variable with a new `istream_h` instance.


**Notes**


- The macro internally calls `istream_create()` to allocate the new `istream_h` instance.
- If `var` already holds an `istream_h` instance, it is destroyed using `istream_destroy()` before creating a new one.
- Ensure that `var` is properly initialized to `NULL` before using this macro for the first time.
- The macro logs an error message if `var` is already initialized, which can help in debugging improper usage.


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

