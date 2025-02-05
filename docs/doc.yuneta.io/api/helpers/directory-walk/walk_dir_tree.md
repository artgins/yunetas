<!-- ============================================================== -->
(walk_dir_tree())=
# `walk_dir_tree()`
<!-- ============================================================== -->


The `walk_dir_tree` function traverses a directory tree starting from `root_dir`. 
It calls the provided callback function `cb` for each file or directory found that matches the specified `pattern`. 
If the callback returns FALSE, the traversal is halted. The function returns 0 on success and -1 on failure.


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
  - The gobj context in which the function is executed.

* - `root_dir`
  - `const char *`
  - The path to the root directory from which to start the traversal.

* - `pattern`
  - `const char *`
  - A pattern to match files and directories against.

* - `opt`
  - `wd_option`
  - Options that modify the behavior of the traversal (e.g., recursive, hidden files).

* - `cb`
  - `walkdir_cb`
  - A callback function that is called for each file or directory found.

* - `user_data`
  - `void *`
  - User-defined data that is passed to the callback function.
:::


---

**Return Value**


The function returns 0 on success, indicating that the directory tree was traversed successfully. 
It returns -1 if an error occurred during the traversal.


**Notes**


The `pattern` can include wildcards to match multiple files. 
The `opt` parameter allows for customization of the traversal behavior, such as including hidden files or only matching specific types of files.


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

