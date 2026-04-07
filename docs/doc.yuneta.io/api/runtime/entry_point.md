# Entry Point

Functions for initializing and launching a yuno application.

**Source:** `kernel/c/root-linux/src/entry_point.h`,
`kernel/c/root-linux/src/manage_services.h`,
`kernel/c/root-linux/src/yunetas_register.h`

---

(yuneta_setup)=
## `yuneta_setup()`

Configures the yuneta runtime: persistent-attribute handlers,
command / stats parsers, authentication / authorization callbacks,
and memory allocator settings. Call **once**, before `yuneta_entry_point()`.

```C
int yuneta_setup(
    const persistent_attrs_t    *persistent_attrs,
    json_function_fn            command_parser,
    json_function_fn            stats_parser,
    authorization_checker_fn    authz_checker,
    authentication_parser_fn    authentication_parser,
    size_t                      mem_max_block,
    size_t                      mem_max_system_memory,
    BOOL                        use_own_system_memory,
    size_t                      mem_min_block,
    size_t                      mem_superblock
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `persistent_attrs` | `const persistent_attrs_t *` | Custom persistent-attribute callbacks (`NULL` uses the built-in `dbsimple` backend). |
| `command_parser` | `json_function_fn` | Custom command parser (`NULL` uses the built-in `command_parser`). |
| `stats_parser` | `json_function_fn` | Custom stats parser (`NULL` uses the built-in `stats_parser`). |
| `authz_checker` | `authorization_checker_fn` | Authorization checker (`NULL` uses the default `C_AUTHZ` monoclass). |
| `authentication_parser` | `authentication_parser_fn` | Authentication parser (`NULL` uses the default `C_AUTHZ` monoclass). |
| `mem_max_block` | `size_t` | Largest memory block size for the allocator. |
| `mem_max_system_memory` | `size_t` | Maximum system memory the allocator may use. |
| `use_own_system_memory` | `BOOL` | `TRUE` to use the custom memory allocator. |
| `mem_min_block` | `size_t` | Smallest memory block size. |
| `mem_superblock` | `size_t` | Super-block size. |

**Returns**

`0` on success.

---

(yuneta_entry_point)=
## `yuneta_entry_point()`

Main entry point for a yuno application. Parses command-line arguments,
loads configuration, sets up the environment, creates the yuno GObj tree,
and runs the event loop.

```C
int yuneta_entry_point(
    int argc, char *argv[],
    const char *APP_NAME,
    const char *APP_VERSION,
    const char *APP_SUPPORT,
    const char *APP_DOC,
    const char *APP_DATETIME,
    const char *fixed_config,
    const char *variable_config,
    int (*register_yuno_and_more)(void),
    void (*cleaning_fn)(void)
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `argc` | `int` | Argument count from `main()`. |
| `argv` | `char *[]` | Argument vector from `main()`. |
| `APP_NAME` | `const char *` | Application / yuno role name. |
| `APP_VERSION` | `const char *` | Application version string. |
| `APP_SUPPORT` | `const char *` | Support e-mail address. |
| `APP_DOC` | `const char *` | Short documentation string. |
| `APP_DATETIME` | `const char *` | Build date/time. |
| `fixed_config` | `const char *` | Fixed configuration (JSON string). |
| `variable_config` | `const char *` | Variable configuration (JSON string). |
| `register_yuno_and_more` | `int (*)(void)` | Callback to register custom GClasses — executed after the environment is set up but before yuno creation. |
| `cleaning_fn` | `void (*)(void)` | Cleanup callback — executed after all resources are freed. |

**Returns**

Process exit code.

---

(set_auto_kill_time)=
## `set_auto_kill_time()`

For testing: kills the yuno after a specified number of seconds.
Only effective when the yuno is **not** running as a daemon.

```C
void set_auto_kill_time(int seconds);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `seconds` | `int` | Timeout in seconds after which the yuno is killed. |

**Returns**

This function does not return a value.

---

(yuneta_json_config)=
## `yuneta_json_config()`

Returns the currently loaded JSON configuration object.

```C
json_t *yuneta_json_config(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Pointer to the configuration `json_t` object. The pointer is **not** owned
by the caller — do not modify or free it.

---

(run_services)=
## `run_services()`

Starts and plays services and the yuno in priority order.
Calls `gobj_start()` on the yuno, starts autostart services,
calls `gobj_play()` on the yuno if autoplay is enabled, and
plays autoplay services.

```C
void run_services(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

---

(stop_services)=
## `stop_services()`

Pauses and stops services and the yuno in reverse priority order
(priority 9 down to 0).

```C
void stop_services(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

---

(yuno_shutdown)=
## `yuno_shutdown()`

Shuts down the yuno by marking it as shutting down and stopping the
event loop.

```C
void yuno_shutdown(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

**Notes**

- Calls [`gobj_set_shutdown()`](#gobj_set_shutdown) followed by
  `yev_loop_reset_running()`.

---

(yunetas_register_c_core)=
## `yunetas_register_c_core()`

Registers all built-in runtime GClasses in one call. This is the
centralized registration point invoked during yuno startup.

```C
int yunetas_register_c_core(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

`0` on success.

**Notes**

- Internally calls every `register_c_*()` function listed in
  [GClass Registration](registration.md).
