<!-- ============================================================== -->
(get_number_of_files())=
# `get_number_of_files()`
<!-- ============================================================== -->


This function counts the number of files in a specified directory that match a given pattern. It traverses the directory tree starting from `root_dir`, applying the specified options to filter the results. The function returns the total count of files that meet the criteria defined by the pattern and options.


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

int get_number_of_files(
    hgobj    gobj,
    const char *root_dir,
    const char *pattern,
    wd_option opt
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
  - The object that is associated with the operation, typically representing the context in which the function is called.

* - `root_dir`
  - `const char *`
  - The path to the directory where the search for files will begin.

* - `pattern`
  - `const char *`
  - A string pattern used to match file names. This can include wildcards to specify the types of files to count.

* - `opt`
  - `wd_option`
  - Options that modify the behavior of the file search, such as whether to include hidden files or to search recursively.
:::


---

**Return Value**


Returns the number of files that match the specified pattern in the given directory. If an error occurs, it may return a negative value indicating the failure.


**Notes**


Ensure that the `root_dir` is accessible and that the pattern is correctly specified to avoid unexpected results. The function may have performance implications when searching large directory trees, especially with recursive options enabled.


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

