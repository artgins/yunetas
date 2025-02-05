<!-- ============================================================== -->
(open_exclusive())=
# `open_exclusive()`
<!-- ============================================================== -->


The `open_exclusive()` function is used to open a file in an exclusive mode. 
This ensures that the file is not already open or being used by another process. 
If the file does not exist, it will be created with the specified `permission`. 
The function is particularly useful in scenarios where file access conflicts need to be avoided.


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
    int         flags,
    int         permission
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
  - The path to the file to be opened.

* - `flags`
  - `int`
  - Flags specifying the mode in which the file should be opened (e.g., read, write, append).

* - `permission`
  - `int`
  - The permission to be set if the file is created. Typically specified using octal values (e.g., `0644`).
:::


---

**Return Value**


Returns the file descriptor (a non-negative integer) if the operation is successful. 
On failure, it returns `-1` and sets `errno` to indicate the error.


**Notes**


- This function ensures exclusive access to the file by using specific flags (e.g., `O_EXCL`).
- If the file already exists, the function will fail, and `errno` will be set to `EEXIST`.
- Ensure that the `path` provided is valid and accessible.


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

