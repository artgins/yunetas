<!-- ============================================================== -->
(walk_dir_tree())=
# `walk_dir_tree()`
<!-- ============================================================== -->

The `walk_dir_tree()` function traverses a directory tree starting from `root_dir`, applying a user-defined callback function `cb` to each file or directory that matches the specified `pattern` and `opt` options.

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
  - A handle to the Yuneta framework object, used for logging and error handling.

* - `root_dir`
  - `const char *`
  - The root directory from which the traversal begins.

* - `pattern`
  - `const char *`
  - A regular expression pattern to match file or directory names.

* - `opt`
  - `wd_option`
  - Options controlling the traversal behavior, such as recursion, hidden file inclusion, and file type matching.

* - `cb`
  - `walkdir_cb`
  - A callback function that is invoked for each matching file or directory.

* - `user_data`
  - `void *`
  - A user-defined pointer passed to the callback function.
:::

---

**Return Value**

Returns 0 on success, or -1 if an error occurs (e.g., if `root_dir` does not exist or `pattern` is invalid).

**Notes**

The callback function `cb` should return `TRUE` to continue traversal or `FALSE` to stop. The function uses `regcomp()` to compile the `pattern` and `regexec()` to match file names.

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
