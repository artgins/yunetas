<!-- ============================================================== -->
(newfile())=
# `newfile()`
<!-- ============================================================== -->


The `newfile()` function creates a new file at the specified `path` with the given `permission` mode.
If the file already exists, the behavior depends on the `overwrite` flag:
- If `overwrite` is `TRUE`, the existing file will be truncated.
- If `overwrite` is `FALSE`, the function will fail if the file already exists.


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

int newfile(
    const char  *path,
    int         permission,
    BOOL        overwrite
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
  - The path where the new file should be created.

* - `permission`
  - `int`
  - The file permission mode, typically specified using octal values (e.g., `0666`).

* - `overwrite`
  - `BOOL`
  - If `TRUE`, an existing file will be truncated; otherwise, the function fails if the file exists.

:::


---

**Return Value**


Returns `0` on success. Returns `-1` on failure, typically due to permission issues or if the file exists and `overwrite` is `FALSE`.


**Notes**


- The `permission` argument follows standard Unix file permission conventions.
- If `overwrite` is `FALSE` and the file exists, the function does not modify the file.
- Use [`newdir()`](#newdir) if you need to create a directory instead.


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

