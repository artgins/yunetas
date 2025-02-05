<!-- ============================================================== -->
(get_ordered_filename_array())=
# `get_ordered_filename_array()`
<!-- ============================================================== -->


The `get_ordered_filename_array()` function retrieves an ordered array of filenames from a directory tree rooted at `root_dir`. 
It traverses the directory tree, applies the specified `pattern` and options (`opt`), and returns the filenames in a sorted order. 
The function allocates memory for the returned array, which must be freed using `free_ordered_filename_array()` after use.


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
    hgobj       gobj,
    const char *root_dir,
    const char *pattern,
    wd_option   opt,
    int        *size
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
  - Handle to the GObj (generic object) that may be used for logging or context.

* - `root_dir`
  - `const char *`
  - The root directory from which to start the traversal.

* - `pattern`
  - `const char *`
  - A pattern to match filenames (e.g., "*.txt"). Can be `NULL` to match all files.

* - `opt`
  - `wd_option`
  - Options for traversal, such as recursive search or filtering by file type.

* - `size`
  - `int *`
  - Pointer to an integer where the function will store the number of filenames found.
:::


---

**Return Value**


Returns a pointer to an array of strings (`char **`) containing the ordered filenames. 
If no files are found or an error occurs, the function returns `NULL`. 
The caller is responsible for freeing the returned array using `free_ordered_filename_array()`.


**Notes**


- The function allocates memory for the returned array and its contents. Use `free_ordered_filename_array()` to release this memory.
- The traversal behavior and filtering are controlled by the `opt` parameter, which can include options like `WD_RECURSIVE` or `WD_HIDDENFILES`.
- This function is a custom implementation and may be less efficient than using standard library functions like `glob()`.


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

