<!-- ============================================================== -->
(launch_daemon())=
# `launch_daemon()`
<!-- ============================================================== -->


The `launch_daemon()` function starts a new process as a daemon.

It forks the current process, detaches it from the controlling terminal, 
and runs the specified `program` in the background.

If `redirect_stdio_to_null` is `TRUE`, standard input, output, and error 
streams are redirected to `/dev/null`.

The function accepts a variable number of arguments, which are passed 
to the `program` as command-line arguments.


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
    BOOL        redirect_stdio_to_null,
    const char  *program,
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
  - If `TRUE`, redirects standard input, output, and error to `/dev/null`.

* - `program`
  - `const char *`
  - The path to the executable program to be launched as a daemon.

* - `...`
  - `variadic`
  - Additional arguments to be passed to the daemonized program.

:::


---

**Return Value**


Returns the process ID (`pid`) of the newly created daemon process on success.

Returns `-1` if an error occurs during the daemonization process.


**Notes**


- The function does not wait for the daemonized process to complete.
- The caller is responsible for handling the lifecycle of the daemon process.
- If `redirect_stdio_to_null` is `TRUE`, the daemon will not produce any console output.


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

