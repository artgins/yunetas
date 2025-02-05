<!-- ============================================================== -->
(get_ordered_filename_array())=
# `get_ordered_filename_array()`
<!-- ============================================================== -->


This function retrieves an ordered array of full filenames from a specified directory that match a given pattern. It traverses the directory tree starting from `root_dir`, applying the specified `pattern` to filter the files. The results are returned in an array of strings, which must be freed by the caller using `free_ordered_filename_array()`. The order of the filenames in the array is determined by the file system's natural ordering.


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

char **get_ordered_filename_array(
    hgobj gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    int *size
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
  - The gobj context in which the function is called.

* - `root_dir`
  - `const char *`
  - The path to the root directory from which to start the search for files.

* - `pattern`
  - `const char *`
  - The pattern used to filter the filenames. This can include wildcards.

* - `opt`
  - `wd_option`
  - Options that modify the behavior of the directory traversal (e.g., recursive search, hidden files).

* - `size`
  - `int *`
  - A pointer to an integer where the size of the returned array will be stored.
:::


---

**Return Value**


Returns a pointer to an array of strings (char **) containing the ordered filenames. The caller is responsible for freeing this array using `free_ordered_filename_array()`.


**Notes**


This function may reinvent the wheel for file searching; consider using `glob()` for similar functionality. The returned array must be freed by the caller to avoid memory leaks.


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

