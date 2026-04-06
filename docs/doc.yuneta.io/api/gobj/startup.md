# Startup

Initialize and shut down the gobj runtime and control the process exit code. Call `gobj_start_up()` once before any other gobj function and `gobj_end()` on termination.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)

(gobj_end)=
## `gobj_end()`

De-initializes the gobj system, freeing resources and shutting down the yuno if it exists.

```C
void gobj_end(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

None.

**Notes**

If a yuno exists, it will be stopped and destroyed before freeing global resources.

---

(gobj_get_exit_code)=
## `gobj_get_exit_code()`

Retrieves the exit code set for the application shutdown process.

```C
int gobj_get_exit_code(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Returns the exit code as an integer.

**Notes**

The exit code is set using [`gobj_set_exit_code()`](#gobj_set_exit_code).

---

(gobj_is_shutdowning)=
## `gobj_is_shutdowning()`

Checks if the system is in the process of shutting down.

```C
BOOL gobj_is_shutdowning(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Returns `TRUE` if the system is shutting down, otherwise returns `FALSE`.

**Notes**

This function is useful for determining whether the system is in the process of shutting down, allowing components to take appropriate actions.

---

(gobj_set_exit_code)=
## `gobj_set_exit_code()`

Sets the exit code for the process, which will be returned when the program terminates.

```C
void gobj_set_exit_code(int exit_code);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `exit_code` | `int` | The exit code to be set for the process. |

**Returns**

This function does not return a value.

**Notes**

The exit code set by this function can be retrieved using [`gobj_get_exit_code()`](#gobj_get_exit_code).

---

(gobj_start_up)=
## `gobj_start_up()`

`gobj_start_up()` initializes the gobj system, setting up global settings, memory management, and persistent attributes.

```C
int gobj_start_up(
    int argc,
    char *argv[],
    const json_t *jn_global_settings,
    const persistent_attrs_t *persistent_attrs,
    json_function_fn global_command_parser,
    json_function_fn global_statistics_parser,
    authorization_checker_fn global_authorization_checker,
    authentication_parser_fn global_authentication_parser
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `argc` | `int` | Number of command-line arguments. |
| `argv` | `char *argv[]` | Array of command-line arguments. |
| `jn_global_settings` | `const json_t *` | Global settings in JSON format (not owned). |
| `persistent_attrs` | [`const persistent_attrs_t *`](#persistent_attrs_t) | Persistent attributes management functions. |
| `global_command_parser` | `json_function_fn` | Global command parser function. |
| `global_statistics_parser` | `json_function_fn` | Global statistics parser function. |
| `global_authorization_checker` | `authorization_checker_fn` | Global authorization checker function. |
| `global_authentication_parser` | `authentication_parser_fn` | Global authentication parser function. |
| `mem_max_block` | `size_t` | Maximum memory block size. |
| `mem_max_system_memory` | `size_t` | Maximum system memory allocation. |
| `use_own_system_memory` | `BOOL` | Flag to use internal memory manager. |
| `mem_min_block` | `size_t` | Minimum memory block size (used only in internal memory manager). |
| `mem_superblock` | `size_t` | Superblock size (used only in internal memory manager). |

**Returns**

Returns `0` on success, `-1` if already initialized.

**Notes**

This function must be called before using any other gobj-related functions.
If `persistent_attrs` is provided, it initializes persistent attributes.
If `global_command_parser` is `NULL`, an internal command parser is used.
If `global_statistics_parser` is `NULL`, an internal statistics parser is used.
If `global_authorization_checker` is `NULL`, authorization checks are disabled.
If `global_authentication_parser` is `NULL`, authentication is bypassed.

---

(gobj_set_shutdown)=
## `gobj_set_shutdown()`

Marks the gobj system as shutting down. Once called, `gobj_is_shutdowning()` will return `TRUE`. This flag is used throughout the framework to signal that the yuno is in the process of shutting down, allowing GClasses to perform cleanup and stop accepting new work.

```C
void gobj_set_shutdown(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

**Notes**

Calling this function multiple times is safe -- subsequent calls are ignored if the shutdown flag is already set. Use `gobj_is_shutdowning()` to check the current shutdown state.

---

