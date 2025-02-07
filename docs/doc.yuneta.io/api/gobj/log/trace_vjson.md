<!-- ============================================================== -->
(trace_vjson)=
# `trace_vjson()`
<!-- ============================================================== -->

Logs a JSON-formatted message with optional structured data, using a specified priority level.

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
void trace_vjson(
    hgobj gobj,
    int priority,
    json_t *jn_data,
    const char *msgset,
    const char *fmt,
    va_list ap
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `gobj`
  - `hgobj`
  - The GObj instance associated with the log message.

* - `priority`
  - `int`
  - The priority level of the log message, typically using syslog priority values.

* - `jn_data`
  - `json_t *`
  - Optional JSON data to include in the log message. Not owned by the function.

* - `msgset`
  - `const char *`
  - A string identifier categorizing the log message.

* - `fmt`
  - `const char *`
  - A format string for the log message, similar to printf.

* - `ap`
  - `va_list`
  - A variable argument list containing values to format into the log message.
:::

---

**Return Value**

This function does not return a value.

**Notes**

The function formats the log message as JSON, including metadata such as timestamps and system information. It is used internally by logging functions like [`gobj_trace_json()`](#gobj_trace_json) and [`gobj_trace_msg()`](#gobj_trace_msg).

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
