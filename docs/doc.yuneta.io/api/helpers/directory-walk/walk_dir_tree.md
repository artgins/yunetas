<!-- ============================================================== -->
(walk_dir_tree())=
# `walk_dir_tree()`
<!-- ============================================================== -->


The `walk_dir_tree` function traverses a directory tree starting from the `root_dir`, calling the provided callback function `cb` for each file found. If the callback returns FALSE, the traversal stops. It supports various options specified by `opt`.


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
    hgobj gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt,
    walkdir_cb cb,
    void *user_data
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
  - The gobj instance.
  
* - `root_dir`
  - `const char *`
  - The root directory to start the traversal.
  
* - `pattern`
  - `const char *`
  - Pattern to match files or directories.
  
* - `opt`
  - `wd_option`
  - Options for traversal (e.g., recursive, hidden files).
  
* - `cb`
  - `walkdir_cb`
  - Callback function to be called for each file found.
  
* - `user_data`
  - `void *`
  - User data to be passed to the callback function.
:::


---

**Return Value**


Returns 0 on success, -1 on failure.


**Notes**


- The callback function `cb` should return FALSE to stop the traversal.
- The `opt` parameter allows specifying various options for traversal, such as recursive traversal, including hidden files, and more.


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

