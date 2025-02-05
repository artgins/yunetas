<!-- ============================================================== -->
(get_ordered_filename_array())=
# `get_ordered_filename_array()`
<!-- ============================================================== -->


The `get_ordered_filename_array` function retrieves an ordered array of full filenames from a specified directory (`root_dir`) that match a given pattern. The function allocates memory for the array, which must be freed by the caller using `free_ordered_filename_array()`. This function is useful for file management tasks where a sorted list of files is required.


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
    hgobj    gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    int     *size
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
  - The directory path from which to retrieve the filenames.

* - `pattern`
  - `const char *`
  - A pattern to match filenames against (e.g., "*.txt").

* - `opt`
  - `wd_option`
  - Options for the directory traversal, such as whether to include hidden files.

* - `size`
  - `int *`
  - A pointer to an integer where the size of the returned array will be stored.
:::


---

**Return Value**


Returns a pointer to an array of strings (char **) containing the ordered filenames. If an error occurs, it returns NULL.


**Notes**


The caller is responsible for freeing the memory allocated for the returned array using `free_ordered_filename_array()`. This function may reinvent the wheel for file retrieval; consider using `glob()` for similar functionality.


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

