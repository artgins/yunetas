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


## Start up functions

### `gobj_start_up`

Initialize a Yuno instance.

This function prepares a Yuno for operation by setting global configurations, handling persistent attributes, and configuring memory management. It serves as the entry point for starting a Yuno instance.

**Parameters**

| Name                         | Type                     | Description                                                 |
|------------------------------|--------------------------|-------------------------------------------------------------|
| `argc`                       | `int`                   | Number of command-line arguments.                          |
| `argv`                       | `char *`                | Array of command-line argument strings.                    |
| `jn_global_settings`         | `json_t *`              | Global settings, not owned.                                |
| `startup_persistent_attrs`   | `int (*)(void)`         | Callback to start up persistent attributes.                |
| `end_persistent_attrs`       | `void (*)(void)`        | Callback to end persistent attributes.                     |
| `load_persistent_attrs`      | `int (*)(hgobj, json_t *)` | Callback to load persistent attributes, `json_t *keys` owned. |
| `save_persistent_attrs`      | `int (*)(hgobj, json_t *)` | Callback to save persistent attributes, `json_t *keys` owned. |
| `remove_persistent_attrs`    | `int (*)(hgobj, json_t *)` | Callback to remove persistent attributes, `json_t *keys` owned. |
| `list_persistent_attrs`      | `json_t * (*)(hgobj, json_t *)` | Callback to list persistent attributes, `json_t *keys` owned. |
| `global_command_parser`      | `json_function_t`       | Function for global command parsing.                       |
| `global_stats_parser`        | `json_function_t`       | Function for global statistics parsing.                    |
| `global_authz_checker`       | `authz_checker_fn`      | Function for global authorization checks.                  |
| `global_authenticate_parser` | `authenticate_parser_fn` | Function for global authentication parsing.               |
| `max_block`                  | `size_t`                | Largest memory block size.                                 |
| `max_system_memory`          | `size_t`                | Maximum system memory size.                                |


**Return Value**

- **`0`**: Success.
- **`< 0`**: Indicates an error during initialization.
