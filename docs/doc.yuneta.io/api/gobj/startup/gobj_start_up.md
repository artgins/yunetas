<!-- ============================================================== -->
(gobj_start_up())=
# `gobj_start_up()`
<!-- ============================================================== -->

The `gobj_start_up()` function initializes the GObj system and prepares the [](yuno) for operation. It applies global configurations, sets up memory management, and integrates key components like command parsing, statistics handling, and user authentication.

**Key Features:**
1. **Global Settings**:
   Accepts a JSON object [](global_settings) that defines configuration attributes for classes and objects in the Yuno. These attributes are applied to each GObj upon creation.

2. **Persistent Attributes**:
   Uses database functions [](persistent_attrs_t) to store and retrieve persistent attributes marked in Gobjs.

3. **Custom Parsers**:
   - **Command Parser**: Parses commands for Gobjs. Defaults to a built-in parser if `NULL`. See [](command_parser).
   - **Statistics Parser**: Handles GObj statistics requests, linked to attributes flagged with `SDF_STATS`. Defaults to a built-in function if `NULL`. See [](stats_parser).

4. **User Management**:
   - **Authorization Checker**: Validates user permissions for commands, attributes, and events. See [](authorization_checker).
   - **Authentication Parser**: Authenticates users with a built-in or custom function. See [](authentication_parser).

5. **Memory Management**:
   Configures limits for memory block size (`mem_max_block`) and total system memory (`mem_max_system_memory`). Exceeding these limits triggers a Yuno **exit** and [](re-launch).

**Usage:**
`gobj_start_up()` must be called before using any GObj functionalities. To terminate properly, use its counterpart, [`gobj_end()`](gobj_end.md).

---

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
PUBLIC int gobj_start_up(       /* Initialize the gobj's system */
    int                         argc,
    char                        *argv[],
    const json_t                *jn_global_settings,    /* NOT owned */
    const persistent_attrs_t    *persistent_attrs,
    json_function_fn            global_command_parser,
    json_function_fn            global_statistics_parser,
    authorization_checker_fn    global_authorization_checker,
    authentication_parser_fn    global_authentication_parser,
    size_t                      mem_max_block,
    size_t                      mem_max_system_memory,
    BOOL                        use_own_system_memory,
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
  - Argument `argc` of *main()*.

* - `argv`
  - `char *[]`
  - Argument `argv` of *main()*.

* - `jn_global_settings`
  - [`json_t *`](json_t)
  - A JSON object containing global configuration settings. 
    This parameter is **not owned** by the function.
    See [](global_settings).

* - `persistent_attrs`
  - [`persistent_attrs_t`](persistent_attrs_t)
  - A structure containing database functions for managing persistent attributes of gobjs. 
    See [](persistent_attrs_t).

* - `global_command_parser`
  - [`json_function_fn`](json_function_fn)
  - A function pointer for handling global command parsing. If `NULL`, the internal command parser is used. See [](command_parser).

* - `global_statistics_parser`
  - [`json_function_fn`](json_function_fn)
  - A function pointer for handling global statistics parsing. If `NULL`, the internal statistics parser is used. See [](stats_parser).

* - `global_authorization_checker`
  - [`authorization_checker_fn`](authorization_checker_fn)
  - A function pointer for performing global authorization checks.
  See [](authorization_checker).

* - `global_authentication_parser`
  - [`authentication_parser_fn`](authentication_parser_fn)
  - A function pointer for parsing and handling global authentication requests. 
  See [](authentication_parser).

* - `mem_max_block`
  - `size_t`
  - Specifies the maximum size of a single memory block, in bytes. The default value is `16M`.  
    This value can be customized in your [](yuno).  
    Attempting to allocate a memory block larger than this limit will cause the yuno to **exit** and trigger a [`re-launch`](re-launch).

* - `mem_max_system_memory`
  - `size_t`
  - Specifies the total memory limit for the system, in bytes. The default value is `64M`.  
    This value can be customized in your [](yuno).  
    If the total memory usage exceeds this limit, the yuno will **exit** and trigger a [`re-launch`](re-launch).

* - `use_own_system_memory`
  - `BOOL`
  - Flag indicating whether to use the internal memory manager.
   Default is `false`. {warning}`NOT IMPLEMENTED`

* - `mem_min_block`
  - `size_t`
  - Minimum size of memory blocks, used only if `use_own_system_memory` is `TRUE`. Default is `512`.
  {warning}`NOT IMPLEMENTED`.

* - `mem_superblock`
  - `size_t`
  - Size of superblocks, used only if `use_own_system_memory` is `TRUE`. Default is `16M`.
  {warning}`NOT IMPLEMENTED`.

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
