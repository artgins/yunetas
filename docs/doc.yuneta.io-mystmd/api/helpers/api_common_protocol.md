# Common Protocol Functions

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(comm_prot_free)=
## `comm_prot_free()`

The `comm_prot_free` function releases all registered communication protocol mappings, freeing associated memory.

```C
void comm_prot_free(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

This function does not return a value.

**Notes**

This function should be called to clean up the communication protocol registry before program termination.

---

(comm_prot_get_gclass)=
## `comm_prot_get_gclass()`

Retrieves the gclass name associated with a given communication protocol schema.

```C
PUBLIC gclass_name_t comm_prot_get_gclass(
    const char *schema
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `schema` | `const char *` | The communication protocol schema to look up. |

**Returns**

Returns the gclass name associated with the given schema. If the schema is not found, logs an error and returns NULL.

**Notes**

This function searches the registered communication protocols and returns the corresponding gclass name. If the schema is not found, an error is logged.

---

(comm_prot_register)=
## `comm_prot_register()`

Registers a `gclass` with a specified communication protocol schema, allowing it to be retrieved later by schema name.

```C
int comm_prot_register(
    gclass_name_t gclass_name,
    const char *schema
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gclass_name` | `gclass_name_t` | The name of the `gclass` to be associated with the schema. |
| `schema` | `const char *` | The communication protocol schema to register with the `gclass`. |

**Returns**

Returns `0` on success, or `-1` if memory allocation fails.

**Notes**

The function initializes the internal communication protocol registry if it has not been initialized yet. The schema is stored as a dynamically allocated string, which will be freed when the registry is cleared.

---
