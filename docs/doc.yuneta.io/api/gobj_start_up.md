<!-- ============================================================== -->
(gobj_start_up())=
# `gobj_start_up()`
<!-- ============================================================== -->

Initialize a Yuno instance.

This function prepares a Yuno for operation by setting global configurations, handling persistent attributes, and configuring memory management. It serves as the entry point for starting a Yuno instance.

To properly terminate the Yuno, call the function [](gobj_end()).

```{tip}
The [](gobj_start_up()) function serves as the entry point to Yuno and must be invoked before utilizing any other GObj functionalities. Similarly, its counterpart, [](gobj_end()), should be called to properly terminate and exit Yuno.
```

<!------------------------------------------------------------>
<!--                    Prototypes                          -->
<!------------------------------------------------------------>

``````{tab-set}


`````{tab-item} C

<!--====================================================-->
<!--                    Tab C                           -->
<!--====================================================-->

<!---------------------------------------------------->
<!--                C Prototype                     -->
<!---------------------------------------------------->

**Prototype**

````C
PUBLIC int gobj_start_up(      
    int                         argc,
    char                        *argv[],
    json_t                      *jn_global_settings,    /* NOT owned */
    const persistent_attrs_t    *persistent_attrs,
    json_function_t             global_command_parser,  
    json_function_t             global_stats_parser,
    authz_checker_fn            global_authz_checker,
    authenticate_parser_fn      global_authenticate_parser,
    size_t                      mem_max_block,
    size_t                      mem_max_system_memory,
    BOOL                        use_own_system_memory, // NOT implemented
    size_t                      mem_min_block,  // used only in internal memory manager
    size_t                      mem_superblock  // used only in internal memory manager
);
````

<!---------------------------------------------------->
<!--                C Parameters                    -->
<!---------------------------------------------------->

**Parameters**

````{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description
* - `argc`
  - `int`
  - Number of command-line arguments passed to the program.
* - `argv`
  - `char *[]`
  - Array of command-line arguments.
* - `jn_global_settings`
  - `json_t *`
  - A JSON object containing global configuration settings. This parameter is **not owned** by the function, meaning it should not be freed.
* - `persistent_attrs`
  - `persistent_attrs_t`
  - A structure containing function pointers for managing persistent attributes. 
    See [](Persistent Attributes).
* - `global_command_parser`
  - `json_function_t`
  - A function pointer for handling global command parsing in JSON format. If `NULL`, the internal command parser is used. See {ref}`json_function_t`.
* - `global_stats_parser`
  - `json_function_t`
  - A function pointer for handling global statistics parsing in JSON format. If `NULL`, the internal statistics parser is used. See {ref}`json_function_t`.
* - `global_authz_checker`
  - `authz_checker_fn`
  - A function pointer for performing global authorization checks. See {ref}`authz_checker_fn`.
* - `global_authenticate_parser`
  - `authenticate_parser_fn`
  - A function pointer for parsing and handling global authentication requests. See {ref}`authenticate_parser_fn`.
* - `mem_max_block`
  - `size_t`
  - Maximum size of memory blocks, in bytes. Default is `16M`.
* - `mem_max_system_memory`
  - `size_t`
  - Total memory limit for the system, in bytes. Default is `64M`.
* - `use_own_system_memory`
  - `BOOL`
  - Flag indicating whether to use the internal memory manager.
* - `mem_min_block`
  - `size_t`
  - Minimum size of memory blocks, used only if `use_own_system_memory` is `TRUE`. Default is `512`.
* - `mem_superblock`
  - `size_t`
  - Size of superblocks, used only if `use_own_system_memory` is `TRUE`. Default is `16M`.
````

<!---------------------------------------------------->
<!--                C Return                        -->
<!---------------------------------------------------->

**Return Value**

- **`0`**: Success.
- **`< 0`**: Indicates an error during initialization.

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
TODO
````

<!---------------------------------------------------->
<!--                JS Parameters                   -->
<!---------------------------------------------------->

**Parameters**

````{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description
* - `XXX`
  - `xxx`
  - Xxx xx xxx xxxx.
````

<!---------------------------------------------------->
<!--                JS Return                       -->
<!---------------------------------------------------->

**Return Value**

- **`0`**: Success.
- **`< 0`**: Indicates an error during initialization.

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
TODO
````

<!---------------------------------------------------->
<!--                Python Parameters               -->
<!---------------------------------------------------->

**Parameters**

````{list-table}
:widths: 10 5 40
:header-rows: 1
* - Key
  - Type
  - Description
* - `XXX`
  - `xxx`
  - Xxx xx xxx xxxx.

````

<!---------------------------------------------------->
<!--                Python Return                   -->
<!---------------------------------------------------->

**Return Value**

- **`0`**: Success.
- **`< 0`**: Indicates an error during initialization.

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
// TODO
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
// TODO
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
# TODO
````

<!--====================================================-->
<!--                    End Tab Python                  -->
<!--====================================================-->

`````

``````

```````
