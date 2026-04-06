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

*Description pending — signature extracted from header.*

```C
int check_open_fds(void);
```

---

(cpu_usage)=
## `cpu_usage()`

*Description pending — signature extracted from header.*

```C
uint64_t cpu_usage(void);
```

---

(cpu_usage_percent)=
## `cpu_usage_percent()`

*Description pending — signature extracted from header.*

```C
double cpu_usage_percent(
    uint64_t *last_cpu_ticks,
    uint64_t *last_ms
);
```

---

(create_random_uuid)=
## `create_random_uuid()`

*Description pending — signature extracted from header.*

```C
int create_random_uuid(
    char *bf,
    int bfsize
);
```

---

(free_ram_in_kb)=
## `free_ram_in_kb()`

*Description pending — signature extracted from header.*

```C
unsigned long free_ram_in_kb(void);
```

---

(get_name_from_nn_table)=
## `get_name_from_nn_table()`

*Description pending — signature extracted from header.*

```C
const char *get_name_from_nn_table(
    const number_name_table_t *table,
    int number
);
```

---

(get_number_from_nn_table)=
## `get_number_from_nn_table()`

*Description pending — signature extracted from header.*

```C
int get_number_from_nn_table(
    const number_name_table_t *table,
    const char *name
);
```

---

(get_yunetas_base)=
## `get_yunetas_base()`

*Description pending — signature extracted from header.*

```C
const char *get_yunetas_base(void);
```

---

(is_yuneta_user)=
## `is_yuneta_user()`

*Description pending — signature extracted from header.*

```C
int is_yuneta_user(
    const char *username
);
```

---

(print_open_fds)=
## `print_open_fds()`

*Description pending — signature extracted from header.*

```C
int print_open_fds(
    const char *fmt,
    ...
);
```

---

(source2base64_for_yunetas)=
## `source2base64_for_yunetas()`

*Description pending — signature extracted from header.*

```C
gbuffer_t *source2base64_for_yunetas(
    const char *source,
    char *comment,
    int commentlen
);
```

---

(total_ram_in_kb)=
## `total_ram_in_kb()`

*Description pending — signature extracted from header.*

```C
unsigned long total_ram_in_kb(void);
```

---

(yuneta_getgrnam)=
## `yuneta_getgrnam()`

*Description pending — signature extracted from header.*

```C
struct group *yuneta_getgrnam(
    const char *name
);
```

---

(yuneta_getgrouplist)=
## `yuneta_getgrouplist()`

*Description pending — signature extracted from header.*

```C
int yuneta_getgrouplist(
    const char *user,
    gid_t group,
    gid_t *groups,
    int *ngroups
);
```

---

(yuneta_getpwnam)=
## `yuneta_getpwnam()`

*Description pending — signature extracted from header.*

```C
struct passwd *yuneta_getpwnam(
    const char *name
);
```

---

(yuneta_getpwuid)=
## `yuneta_getpwuid()`

*Description pending — signature extracted from header.*

```C
struct passwd *yuneta_getpwuid(
    uid_t uid
);
```

---

