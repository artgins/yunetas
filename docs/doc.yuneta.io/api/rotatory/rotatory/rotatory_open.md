<!-- ============================================================== -->
(rotatory_open)=
# `rotatory_open()`
<!-- ============================================================== -->

`rotatory_open()` initializes and opens a rotatory log file with the specified parameters, creating necessary directories if required.

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
hrotatory_h rotatory_open(
    const char *path,
    size_t      bf_size,
    size_t      max_megas_rotatoryfile_size,
    size_t      min_free_disk_percentage,
    int         xpermission,
    int         rpermission,
    BOOL        exit_on_fail
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `path`
  - `const char *`
  - The file path for the rotatory log.

* - `bf_size`
  - `size_t`
  - The buffer size for writing logs; `0` defaults to `64K`.

* - `max_megas_rotatoryfile_size`
  - `size_t`
  - The maximum size of a rotatory log file in megabytes; `0` defaults to `8MB`.

* - `min_free_disk_percentage`
  - `size_t`
  - The minimum free disk space percentage before stopping logging; `0` defaults to `10%`.

* - `xpermission`
  - `int`
  - The permission mode for directories and executable files.

* - `rpermission`
  - `int`
  - The permission mode for regular log files.

* - `exit_on_fail`
  - `BOOL`
  - If `TRUE`, the process exits on failure; otherwise, logs an error.
:::

---

**Return Value**

Returns a handle to the rotatory log (`hrotatory_h`) on success, or `NULL` on failure.

**Notes**

If the specified log directory does not exist, `rotatory_open()` attempts to create it.
If the log file does not exist, `rotatory_open()` creates a new one with the specified permissions.
Use [`rotatory_close()`](#rotatory_close) to properly close the log handle.

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
