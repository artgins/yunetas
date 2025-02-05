<!-- ============================================================== -->
(walk_dir_tree())=
# `walk_dir_tree()`
<!-- ============================================================== -->


The `walk_dir_tree()` function traverses a directory tree starting from the specified `root_dir`. 
It calls the provided callback function `cb` for each file or directory found. The traversal can 
be customized using the `opt` parameter, which specifies options such as recursion, matching 
hidden files, or filtering by file types. If the callback function returns `FALSE`, the traversal 
stops. This function is useful for processing files and directories in a structured manner.


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
    const char  *root_dir,
    const char  *pattern,
    wd_option   opt,
    walkdir_cb  cb,
    void        *user_data
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
  - The gobj instance used for logging or context.

* - `root_dir`
  - `const char *`
  - The root directory from which the traversal begins.

* - `pattern`
  - `const char *`
  - A pattern to match file or directory names. Can be `NULL` for no filtering.

* - `opt`
  - `wd_option`
  - Options to customize the traversal behavior, such as recursion or file type matching.

* - `cb`
  - `walkdir_cb`
  - Callback function to be called for each file or directory found.

* - `user_data`
  - `void *`
  - User-defined data passed to the callback function.
:::


---

**Return Value**


Returns `0` on success or `-1` on failure. The return value follows standard Unix conventions.


**Notes**


- The callback function `cb` must return `TRUE` to continue traversal or `FALSE` to stop.
- The `opt` parameter allows fine-grained control over the traversal, including options like 
  `WD_RECURSIVE` for recursive traversal or `WD_HIDDENFILES` to include hidden files.
- Ensure that the `root_dir` exists and is accessible; otherwise, the function will fail.


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

