# GObj Api

The main file for the Yuneta framework. It defines the GObj (Generic Object) API, which includes the GObj system, event-driven architecture, finite state machines, hierarchical object trees, and additional utilities.

This API provides the prototypes, structures, enums, and macros needed to work with the Yuneta framework's GObj system.

- **GObj Features**:
  - Event-driven model using input/output events.
  - Simple finite state machine to manage object states.
  - Hierarchical organization of GObjs forming a tree structure (Yuno).
  - Communication between objects through events (key-value messages).
  - Built-in logging, buffering, and JSON handling.
- **Internal Systems**:
  - Log system.
  - Buffer system.
  - JSON system based on the [`Jansson`](http://jansson.readthedocs.io/en/latest/) library.


# Start up functions

## `gobj_start_up`

Initialize a Yuno instance.

This function prepares a Yuno for operation by setting global configurations, handling persistent attributes, and configuring memory management. It serves as the entry point for starting a Yuno instance.

```{caution}
The [`gobj_start_up()`](#gobj-start-up) function serves as the entry point to Yuno and must be invoked before utilizing any other GObj functionalities. Similarly, its counterpart, [`gobj_end()`](#gobj-end), should be called to properly terminate and exit Yuno.
```

**Prototypes**

``````{tab-set}
`````{tab-item} C

````C
PUBLIC int gobj_start_up(       /* Initialize the yuno */
    int                         argc,
    char                        *argv[],
    json_t                      *jn_global_settings, /* NOT owned */
    startup_persistent_attrs_t  startup_persistent_attrs,
    end_persistent_attrs_t      end_persistent_attrs,
    load_persistent_attrs_t     load_persistent_attrs,
    save_persistent_attrs_t     save_persistent_attrs,
    remove_persistent_attrs_t   remove_persistent_attrs,
    list_persistent_attrs_t     list_persistent_attrs,
    json_function_t             global_command_parser,
    json_function_t             global_stats_parser,
    authz_checker_fn            global_authz_checker,
    authenticate_parser_fn      global_authenticate_parser,
    size_t                      max_block,            /* largest memory block */
    size_t                      max_system_memory     /* maximum system memory */
);
````

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
  - A JSON object containing global configuration settings. This parameter is *not owned* by the function and should not be modified or freed.
* - `startup_persistent_attrs`
  - `startup_persistent_attrs_t`
  - Function pointer to initialize persistent attributes.
* - `end_persistent_attrs`
  - `end_persistent_attrs_t`
  - Function pointer to clean up persistent attributes.
* - `load_persistent_attrs`
  - `load_persistent_attrs_t`
  - Function pointer to load persistent attributes from storage.
* - `save_persistent_attrs`
  - `save_persistent_attrs_t`
  - Function pointer to save persistent attributes to storage.
* - `remove_persistent_attrs`
  - `remove_persistent_attrs_t`
  - Function pointer to delete persistent attributes.
* - `list_persistent_attrs`
  - `list_persistent_attrs_t`
  - Function pointer to list all persistent attributes.
* - `global_command_parser`
  - `json_function_t`
  - Function pointer to handle global command parsing (in JSON format).
* - `global_stats_parser`
  - `json_function_t`
  - Function pointer to handle global statistics parsing (in JSON format).
* - `global_authz_checker`
  - `authz_checker_fn`
  - Function pointer to perform global authorization checks.
* - `global_authenticate_parser`
  - `authenticate_parser_fn`
  - Function pointer to parse and handle global authentication.
* - `max_block`
  - `size_t`
  - The maximum allowed size for memory blocks, in bytes.
* - `max_system_memory`
  - `size_t`
  - The total memory limit for the system, in bytes.
````


**Return Value**

- **`0`**: Success.
- **`< 0`**: Indicates an error during initialization.

`````

`````{tab-item} JS
````JS
xxx
````
`````

``````







```````{dropdown} Examples

``````{tab-set}
`````{tab-item} C

````C
printf("A code block with a caption.");

`````

`````{tab-item} Python
````python

print("A code block with a caption.")

````
`````

``````
```````

## `gobj_end`
