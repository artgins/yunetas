<!-- ============================================================== -->
(walk_dir_tree())=
# `walk_dir_tree()`
<!-- ============================================================== -->


The `walk_dir_tree` function traverses a directory tree starting from the specified `root_dir`. 
For each file or directory found, it calls the provided callback function `cb` with details about the found item. 
The traversal can be controlled using various options specified in `opt`, such as whether to include hidden files, 
whether to match only directory names or regular files, and whether to traverse recursively. 
If the callback function returns FALSE, the traversal will stop. The function returns 0 on success and -1 on failure.


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

int walk_dir_tree(
    hgobj       gobj,
    const char *root_dir,
    const char *pattern,
    wd_option   opt,
    walkdir_cb  cb,
    void       *user_data
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
  - The gobj associated with the operation, used for context in the callback.

* - `root_dir`
  - `const char *`
  - The path of the directory from which to start the traversal.

* - `pattern`
  - `const char *`
  - A pattern to match files and directories against.

* - `opt`
  - `wd_option`
  - Options that control the behavior of the traversal (e.g., recursive, hidden files).

* - `cb`
  - `walkdir_cb`
  - A callback function that is called for each file or directory found.

* - `user_data`
  - `void *`
  - User-defined data that is passed to the callback function.
:::


---

**Return Value**


Returns 0 on success, indicating that the directory tree was traversed successfully. 
Returns -1 if an error occurred during the traversal.


**Notes**


The function may not handle symbolic links or other special file types unless specified in the options. 
Ensure that the callback function is efficient to avoid performance issues during traversal.


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

