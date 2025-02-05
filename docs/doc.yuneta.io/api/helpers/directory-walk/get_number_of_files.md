<!-- ============================================================== -->
(get_number_of_files())=
# `get_number_of_files()`
<!-- ============================================================== -->


The `get_number_of_files()` function traverses a directory tree starting from `root_dir` and counts the number of files that match the specified `pattern`. 
The behavior of the traversal can be customized using the `opt` parameter, which allows for options such as recursive traversal, inclusion of hidden files, and file type matching.


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

PUBLIC int get_number_of_files(
    hgobj       gobj,
    const char  *root_dir,
    const char  *pattern,
    wd_option   opt
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
  - A handle to the gobj (generic object) that may be used for logging or other purposes during the operation.

* - `root_dir`
  - `const char *`
  - The root directory from which the traversal begins.

* - `pattern`
  - `const char *`
  - A pattern to match file names. Can be `NULL` to include all files.

* - `opt`
  - `wd_option`
  - Options to customize the traversal behavior, such as recursion, hidden file inclusion, and file type filtering.

:::


---

**Return Value**


Returns the number of files matching the specified `pattern` in the directory tree. 
If an error occurs during traversal, the function may return `-1`.


**Notes**


- This function relies on the `walk_dir_tree()` function internally to perform the directory traversal.
- The `opt` parameter allows for fine-grained control over the traversal, such as limiting matches to specific file types or including hidden files.
- Ensure that the `root_dir` is accessible and valid to avoid errors.


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

