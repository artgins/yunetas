# Misc

Miscellaneous helpers that don't fit anywhere else: byte-order conversion, base64, hex dumps, small math utilities.

Source code:

- [`helpers.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [`helpers.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(get_hostname)=
## `get_hostname()`

Retrieves the hostname of the current system. On Linux, it reads from `/etc/hostname`, while on ESP32, it returns a default value.

```C
const char *get_hostname(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A pointer to a static string containing the hostname of the system.

**Notes**

The returned string is statically allocated and should not be modified or freed by the caller.

---

(node_uuid)=
## `node_uuid()`

The `node_uuid()` function retrieves a unique identifier for the current machine. On Linux, it reads or generates a UUID stored in `/yuneta/store/agent/uuid/uuid.json`. On ESP32, it derives the UUID from the device's MAC address.

```C
const char *node_uuid(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Returns a pointer to a string containing the unique node UUID.

**Notes**

The returned UUID is generated once and stored persistently. On Linux, if the UUID file does not exist, a new one is created. On ESP32, the UUID is derived from the MAC address.

---

(check_open_fds)=
## `check_open_fds()`

Returns the number of currently open file descriptors in the process. On Linux, reads from `/proc/self/fd`. On other systems, returns 0.

```C
int check_open_fds(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

The number of open file descriptors in the calling process. Returns 0 on non-Linux platforms.

---

(cpu_usage)=
## `cpu_usage()`

Returns the accumulated CPU time of the current process in jiffies (scheduler clock ticks). On Linux, reads from `/proc/self/stat`. On other systems, returns 0.

```C
uint64_t cpu_usage(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

The sum of user time (utime) and system time (stime) in jiffies. Returns 0 on non-Linux platforms.

---

(cpu_usage_percent)=
## `cpu_usage_percent()`

Calculates the CPU usage percentage of the current process over an elapsed time interval. Must be called repeatedly to track CPU usage over time.

```C
double cpu_usage_percent(
    uint64_t *last_cpu_ticks,
    uint64_t *last_ms
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `last_cpu_ticks` | `uint64_t *` | IN/OUT — pointer to previous CPU ticks value. |
| `last_ms` | `uint64_t *` | IN/OUT — pointer to previous timestamp in milliseconds. |

**Returns**

CPU usage percentage as a double ranging from 0.0 to 100.0+. Returns 0.0 on the first call or if the time interval is zero.

---

(create_random_uuid)=
## `create_random_uuid()`

Generates a cryptographically secure random UUID (version 4) and formats it as a string.

```C
int create_random_uuid(
    char *bf,
    int bfsize
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Output buffer for the UUID string. |
| `bfsize` | `int` | Size of the output buffer, must be at least 37 bytes. |

**Returns**

Returns 0 on success. Returns -1 if the buffer is too small or if random number generation fails.

---

(free_ram_in_kb)=
## `free_ram_in_kb()`

Returns the amount of free RAM available in the system in kilobytes. On Linux, reads from `/proc/meminfo`. On other systems, returns 0.

```C
unsigned long free_ram_in_kb(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

The amount of free RAM in kilobytes.

---

(get_name_from_nn_table)=
## `get_name_from_nn_table()`

Looks up a name string in a number-to-name lookup table by matching a numeric value.

```C
const char *get_name_from_nn_table(
    const number_name_table_t *table,
    int number
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `table` | `const number_name_table_t *` | Pointer to the lookup table. |
| `number` | `int` | The numeric value to search for. |

**Returns**

A pointer to the name string corresponding to the given number, or NULL if not found.

---

(get_number_from_nn_table)=
## `get_number_from_nn_table()`

Looks up a numeric value in a number-to-name lookup table by matching a name string.

```C
int get_number_from_nn_table(
    const number_name_table_t *table,
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `table` | `const number_name_table_t *` | Pointer to the lookup table. |
| `name` | `const char *` | The name string to search for. |

**Returns**

The numeric value corresponding to the given name, or -1 if not found.

---

(get_yunetas_base)=
## `get_yunetas_base()`

Resolves the Yunetas base directory path. The environment variable `YUNETAS_BASE` takes priority, followed by standard paths.

```C
const char *get_yunetas_base(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A pointer to a static string containing the Yunetas base directory path.

---

(is_yuneta_user)=
## `is_yuneta_user()`

Checks whether a given username belongs to the "yuneta" group or is exactly the "yuneta" user.

```C
int is_yuneta_user(
    const char *username
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `username` | `const char *` | The username to check. |

**Returns**

Returns TRUE if the user is "yuneta" or belongs to the "yuneta" group. Returns FALSE otherwise.

---

(print_open_fds)=
## `print_open_fds()`

Logs information about all currently open file descriptors in the process using a formatted prefix string.

```C
int print_open_fds(
    const char *fmt,
    ...
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fmt` | `const char *` | Printf-style format string for the log prefix. |
| `...` | variadic | Variable arguments for the format string. |

**Returns**

The number of open file descriptors found and logged.

---

(source2base64_for_yunetas)=
## `source2base64_for_yunetas()`

Converts a file to base64-encoded format, finding the file in either the current path or relative to the Yunetas base directory.

```C
gbuffer_t *source2base64_for_yunetas(
    const char *source,
    char *comment,
    int commentlen
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `source` | `const char *` | Path to the file to encode. |
| `comment` | `char *` | Output buffer for error messages. |
| `commentlen` | `int` | Size of the comment buffer. |

**Returns**

A `gbuffer_t` pointer containing the base64-encoded file content, or NULL if the file is not found.

---

(total_ram_in_kb)=
## `total_ram_in_kb()`

Returns the total amount of RAM installed in the system in kilobytes. On Linux, reads from `/proc/meminfo`.

```C
unsigned long total_ram_in_kb(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

The total RAM in kilobytes.

---

(yuneta_getgrnam)=
## `yuneta_getgrnam()`

Portable wrapper around `getgrnam()` that looks up group information by name. Uses static implementations when `CONFIG_FULLY_STATIC` is defined.

```C
struct group *yuneta_getgrnam(
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `name` | `const char *` | The group name to look up. |

**Returns**

A pointer to a `struct group`, or NULL if the group is not found.

---

(yuneta_getgrouplist)=
## `yuneta_getgrouplist()`

Portable wrapper around `getgrouplist()` that retrieves all groups a user belongs to.

```C
int yuneta_getgrouplist(
    const char *user,
    gid_t group,
    gid_t *groups,
    int *ngroups
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `user` | `const char *` | The username to look up groups for. |
| `group` | `gid_t` | Primary group ID. |
| `groups` | `gid_t *` | Output array for group IDs. |
| `ngroups` | `int *` | IN/OUT — pointer to the size of the groups array. |

**Returns**

The number of groups on success. Returns -1 if the array is too small.

---

(yuneta_getpwnam)=
## `yuneta_getpwnam()`

Portable wrapper around `getpwnam()` that looks up user information by username.

```C
struct passwd *yuneta_getpwnam(
    const char *name
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `name` | `const char *` | The username to look up. |

**Returns**

A pointer to a `struct passwd`, or NULL if the username is not found.

---

(yuneta_getpwuid)=
## `yuneta_getpwuid()`

Portable wrapper around `getpwuid()` that looks up user information by user ID.

```C
struct passwd *yuneta_getpwuid(
    uid_t uid
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `uid` | `uid_t` | The user ID to look up. |

**Returns**

A pointer to a `struct passwd`, or NULL if the user ID is not found.

---

