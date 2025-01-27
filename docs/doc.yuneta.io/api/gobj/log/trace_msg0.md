

<!-- ============================================================== -->
(trace_msg0())=
# `trace_msg0()`
<!-- ============================================================== -->

The `trace_msg0` function is a debugging utility that formats and logs a message with `LOG_DEBUG` priority. It is designed for use in debug-level tracing and invokes a global callback (if defined) to inform external systems about the log event.

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
PUBLIC int trace_msg0(
    const char *fmt,
    ...
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - **Parameter**
  - **Type**
  - **Description**

* - `fmt`
  - `const char *`
  - A format string, similar to `printf`, defining the message to log.

* - `...`
  - Variadic arguments
  - Arguments corresponding to the format specifiers in `fmt`.
:::

---

**Return Value**

- Always returns `0`.

**Notes**
- **Log Handling:**
  - Uses `_log_bf` to handle the formatted log message. The message is logged with `LOG_DEBUG` priority.
- **Callback Support:**
  - If the global callback `__inform_cb__` is defined, it is called after the log entry is processed to notify external systems.
- **Debug Counter:**
  - Maintains a global debug counter `__debug_count__`, which is passed to the callback if invoked.
- **Buffer Handling:**
  - The formatted log message is stored in a temporary buffer (`temp`) with a maximum size of `BUFSIZ`.
- **Thread Safety:**
  - Ensure that the logging system and callback function are thread-safe if used in a multi-threaded environment.

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
