<!-- ============================================================== -->
(launch_daemon())=
# `launch_daemon()`
<!-- ============================================================== -->


The `launch_daemon` function is used to create a new daemon process. A daemon is a background process that runs independently of user control. This function allows the user to specify whether to redirect standard input/output/error streams to `/dev/null`, effectively silencing the daemon's output. The function takes a program name as an argument, followed by any additional arguments needed by the program.

The function returns the process ID (PID) of the newly created daemon, which can be used to manage the daemon process later (e.g., for termination or monitoring).


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

int launch_daemon(
    BOOL redirect_stdio_to_null,
    const char *program,
    ...
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `redirect_stdio_to_null`
  - `BOOL`
  - Indicates whether to redirect standard input/output/error to `/dev/null`.

* - `program`
  - `const char *`
  - The name of the program to be executed as a daemon.

* - `...`
  - `...`
  - Additional arguments to be passed to the program being executed.

:::


---

**Return Value**


The function returns an `int` representing the process ID (PID) of the launched daemon. If the daemon could not be launched, it may return a negative value indicating an error.


**Notes**


This function is typically used in server applications where background processing is required. The behavior of the daemon may depend on the operating system and its configuration. Ensure that the program being launched is designed to run as a daemon.


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

