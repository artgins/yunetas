# Debug / Backtrace Functions

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(tdump)=
## `tdump()`

`tdump` prints a formatted hexadecimal and ASCII dump of a given byte array.

```C
void tdump(
    const char *prefix,
    const uint8_t *s,
    size_t len,
    view_fn_t view,
    int nivel
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `prefix` | `const char *` | A string prefix to prepend to each line of the dump. |
| `s` | `const uint8_t *` | Pointer to the byte array to be dumped. |
| `len` | `size_t` | The number of bytes to dump. |
| `view` | `view_fn_t` | A function pointer for output formatting, typically `printf`. |
| `nivel` | `int` | The verbosity level: 1 for hex only, 2 for hex and ASCII, 3 for hex, ASCII, and byte index. |

**Returns**

This function does not return a value.

**Notes**

If `nivel` is 0, it defaults to 3. The function formats the output in a structured manner, displaying hexadecimal values alongside their ASCII representation.

---

(tdump2json)=
## `tdump2json()`

`tdump2json` converts a binary buffer into a JSON object, representing the data in a structured hexadecimal and ASCII format.

```C
json_t *tdump2json(
    const uint8_t *s,
    size_t len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `s` | `const uint8_t *` | Pointer to the binary data buffer. |
| `len` | `size_t` | Length of the binary data buffer. |

**Returns**

A JSON object where each key represents an address offset and the value is a string containing the hexadecimal and ASCII representation of the corresponding data.

**Notes**

This function is useful for debugging and inspecting binary data in a human-readable format.

---
