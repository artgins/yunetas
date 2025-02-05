<!-- ============================================================== -->
(open_exclusive())=
# `open_exclusive()`
<!-- ============================================================== -->


The `open_exclusive` function is used to open a file in an exclusive mode, ensuring that no other process can access the file simultaneously. This is particularly useful in scenarios where file locking is necessary to prevent data corruption or conflicts. The function takes the file path, the desired flags for opening the file, and the permissions to set if the file is created.


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

int open_exclusive(
    const char *path,
    int flags,
    int permission
);

```

**Parameters**


::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The path to the file that needs to be opened exclusively.

* - `flags`
  - `int`
  - The flags that determine the mode in which the file is opened (e.g., read, write, create).

* - `permission`
  - `int`
  - The permissions to set for the file if it is created (e.g., read/write permissions).
:::


---

**Return Value**


The function returns a file descriptor (an integer) if the file is opened successfully. If the operation fails, it returns -1, indicating an error occurred during the file opening process.


**Notes**


This function is particularly useful in multi-threaded or multi-process applications where file access needs to be controlled to avoid race conditions. Ensure to handle the returned file descriptor properly, closing it when it is no longer needed.


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

