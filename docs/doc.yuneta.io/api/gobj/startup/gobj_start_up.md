<!-- ============================================================== -->
(gobj_start_up())=
# `gobj_start_up()`
<!-- ============================================================== -->

`gobj_start_up()` initializes the gobj system, setting up global settings, memory management, and persistent attributes.

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
int gobj_start_up(
    int                         argc,
    char                        *argv[],
    const json_t                *jn_global_settings,
    const persistent_attrs_t    *persistent_attrs,
    json_function_fn            global_command_parser,
    json_function_fn            global_statistics_parser,
    authorization_checker_fn    global_authorization_checker,
    authentication_parser_fn    global_authentication_parser,
    size_t                      mem_max_block,
    size_t                      mem_max_system_memory,
    BOOL                        use_own_system_memory,
    size_t                      mem_min_block,
    size_t                      mem_superblock
);
```

**Parameters**

::: {list-table}
:widths: 20 20 60
:header-rows: 1

* - Key
  - Type
  - Description

* - `argc`
  - `int`
  - Number of command-line arguments.

* - `argv`
  - `char *argv[]`
  - Array of command-line arguments.

* - `jn_global_settings`
  - `const json_t *`
  - Global settings in JSON format (not owned).

* - `persistent_attrs`
  - [`const persistent_attrs_t *`](persistent_attrs_t)
  - Persistent attributes management functions.

* - `global_command_parser`
  - `json_function_fn`
  - Global command parser function.

* - `global_statistics_parser`
  - `json_function_fn`
  - Global statistics parser function.

* - `global_authorization_checker`
  - `authorization_checker_fn`
  - Global authorization checker function.

* - `global_authentication_parser`
  - `authentication_parser_fn`
  - Global authentication parser function.

* - `mem_max_block`
  - `size_t`
  - Maximum memory block size.

* - `mem_max_system_memory`
  - `size_t`
  - Maximum system memory allocation.

* - `use_own_system_memory`
  - `BOOL`
  - Flag to use internal memory manager.

* - `mem_min_block`
  - `size_t`
  - Minimum memory block size (used only in internal memory manager).

* - `mem_superblock`
  - `size_t`
  - Superblock size (used only in internal memory manager).
:::

---

**Return Value**

Returns `0` on success, `-1` if already initialized.

**Notes**

This function must be called before using any other gobj-related functions.
If `persistent_attrs` is provided, it initializes persistent attributes.
If `global_command_parser` is `NULL`, an internal command parser is used.
If `global_statistics_parser` is `NULL`, an internal statistics parser is used.
If `global_authorization_checker` is `NULL`, authorization checks are disabled.
If `global_authentication_parser` is `NULL`, authentication is bypassed.

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
function gobj_start_up(
    jn_global_settings,
    load_persistent_attrs_fn,
    save_persistent_attrs_fn,
    remove_persistent_attrs_fn,
    list_persistent_attrs_fn,
    global_command_parser_fn,
    global_stats_parser_fn
)
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
