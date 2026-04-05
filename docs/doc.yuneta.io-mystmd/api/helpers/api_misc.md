# Misc Utility Functions

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(create_uuid)=
## `create_uuid()`

Generates a cryptographically secure UUID and stores it in the provided buffer.

```C
int create_uuid(
    char *bf,
    int   bfsize
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `bf` | `char *` | Pointer to a character buffer where the generated UUID will be stored. |
| `bfsize` | `int` | Size of the buffer in bytes; must be at least 37 to accommodate the UUID and null terminator. |

**Returns**

Returns 0 on success, or -1 if an error occurs (e.g., insufficient buffer size).

**Notes**

The function generates a UUID using platform-specific secure random number generators. On Linux, it uses `getrandom()`, while on ESP32, it uses `esp_fill_random()`. The generated UUID follows the version 4 format.

---

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
