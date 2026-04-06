# GBuffer

Reference-counted, append-optimised byte buffer used as the standard payload for I/O and serialization inside Yuneta.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)

(gbuf2json)=
## `gbuf2json()`

Converts a [`gbuffer_t *`](#gbuffer_t) containing JSON data into a `json_t *` object. The function consumes the input buffer and returns a parsed JSON object.

```C
json_t *gbuf2json(
    gbuffer_t *gbuf,  // WARNING gbuf own and data consumed
    int verbose       // 1 log, 2 log+dump
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | The input [`gbuffer_t *`](#gbuffer_t) containing JSON data. This buffer is consumed and should not be used after calling this function. |
| `verbose` | `int` | Logging verbosity level: `1` logs errors, `2` logs errors and dumps the buffer content. |

**Returns**

Returns a `json_t *` object if parsing is successful. Returns `NULL` if parsing fails.

**Notes**

The function uses `json_load_callback()` to parse the JSON data from the buffer. If parsing fails, an error is logged based on the `verbose` level.

---

(gbuffer_append)=
## `gbuffer_append()`

Appends a specified number of bytes from a given buffer to the [`gbuffer_t`](#gbuffer_t) structure, expanding its capacity if necessary.

```C
size_t gbuffer_append(
    gbuffer_t *gbuf,
    void *data,
    size_t len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the [`gbuffer_t`](#gbuffer_t) structure where data will be appended. |
| `bf` | `void *` | Pointer to the buffer containing the data to append. |
| `len` | `size_t` | Number of bytes to append from `bf`. |

**Returns**

Returns the number of bytes successfully appended to the [`gbuffer_t`](#gbuffer_t).

**Notes**

If the [`gbuffer_t`](#gbuffer_t) does not have enough space, it will attempt to reallocate memory. If reallocation fails, fewer bytes may be appended than requested.

---

(gbuffer_append_gbuf)=
## `gbuffer_append_gbuf()`

Appends the contents of one `gbuffer_t` to another. The data from `src` is copied into `dst`, preserving the read position of `src`.

```C
int gbuffer_append_gbuf(
    gbuffer_t *dst,
    gbuffer_t *src
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `dst` | `gbuffer_t *` | The destination buffer where data will be appended. |
| `src` | `gbuffer_t *` | The source buffer whose data will be copied into `dst`. |

**Returns**

Returns `0` on success, or `-1` if an error occurs during the append operation.

**Notes**

The function iterates over `src` in chunks, copying data into `dst`. Ensure that `dst` has enough space to accommodate the data from `src`.

---

(gbuffer_create)=
## `gbuffer_create()`

`gbuffer_create()` allocates and initializes a new `gbuffer_t` structure with a specified data size and maximum memory size.

```C
gbuffer_t *gbuffer_create(
    size_t data_size,
    size_t max_memory_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `data_size` | `size_t` | Initial size of the buffer's allocated memory. |
| `max_memory_size` | `size_t` | Maximum allowed memory size for the buffer. |

**Returns**

Returns a pointer to the newly allocated `gbuffer_t` structure, or `NULL` if memory allocation fails.

**Notes**

If memory allocation for the buffer fails, an error is logged, and `NULL` is returned.

---

(gbuffer_deserialize)=
## `gbuffer_deserialize()`

`gbuffer_deserialize()` reconstructs a `gbuffer_t` object from a JSON representation, decoding its base64-encoded data.

```C
gbuffer_t *gbuffer_deserialize(
    hgobj gobj,
    const json_t *jn  // not owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the calling object, used for logging errors. |
| `jn` | `const json_t *` | A JSON object containing the serialized `gbuffer_t` data, including label, mark, and base64-encoded content. |

**Returns**

A newly allocated `gbuffer_t *` containing the deserialized data, or `NULL` if an error occurs.

**Notes**

The function decodes the base64-encoded data from the JSON object and reconstructs the `gbuffer_t`. The caller is responsible for managing the returned buffer's memory.

---

(gbuffer_encode_base64)=
## `gbuffer_encode_base64()`

Encodes the content of the given [`gbuffer_t *`](#gbuffer_t) into a Base64-encoded [`gbuffer_t *`](#gbuffer_t). The input buffer is decremented in reference count after encoding.

```C
gbuffer_t *gbuffer_encode_base64(
    gbuffer_t *gbuf_input  // decref
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf_input` | `gbuffer_t *` | The input [`gbuffer_t *`](#gbuffer_t) containing the data to be Base64-encoded. This buffer is decremented in reference count after encoding. |

**Returns**

A new [`gbuffer_t *`](#gbuffer_t) containing the Base64-encoded representation of the input buffer. Returns `NULL` on failure.

**Notes**

The caller is responsible for managing the reference count of the returned [`gbuffer_t *`](#gbuffer_t).

---

(gbuffer_get)=
## `gbuffer_get()`

`gbuffer_get()` extracts a specified number of bytes from the given `gbuffer_t` and returns a pointer to the extracted data.

```C
void *gbuffer_get(
    gbuffer_t *gbuf,
    size_t      len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance from which data will be extracted. |
| `len` | `size_t` | Number of bytes to extract from the buffer. |

**Returns**

Returns a pointer to the extracted data if `len` bytes are available; otherwise, returns `NULL`.

**Notes**

Ensure that `len` does not exceed the available data in [`gbuffer_t`](#gbuffer_t). If `len` is greater than the remaining bytes, the function returns `NULL` without modifying the buffer.

---

(gbuffer_getline)=
## `gbuffer_getline()`

`gbuffer_getline()` retrieves a line from the given `gbuffer_t` up to the specified separator character, replacing it with a null terminator.

```C
char *gbuffer_getline(
    gbuffer_t *gbuf,
    char       separator
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance from which the line is extracted. |
| `separator` | `char` | Character that marks the end of the line. |

**Returns**

Returns a pointer to the beginning of the extracted line within the buffer, or `NULL` if no more characters are available.

**Notes**

The function modifies the buffer by replacing the separator with a null terminator. It does not allocate new memory.

---

(gbuffer_printf)=
## `gbuffer_printf()`

The function `gbuffer_printf()` appends formatted text to a [`gbuffer_t *`](#gbuffer_t) buffer using a `printf`-style format string and arguments.

```C
int gbuffer_printf(
    gbuffer_t *gbuf,
    const char *format,
    ...) JANSSON_ATTRS((format(printf, 2, 3))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the [`gbuffer_t *`](#gbuffer_t) buffer where the formatted text will be appended. |
| `format` | `const char *` | A `printf`-style format string specifying how subsequent arguments are formatted. |
| `...` | `variadic` | Additional arguments corresponding to the format specifiers in `format`. |

**Returns**

Returns the number of bytes written to the buffer. If an error occurs, it returns `0`.

**Notes**

If the buffer does not have enough space, it attempts to reallocate memory. If reallocation fails, the function logs an error and returns `0`.

---

(gbuffer_remove)=
## `gbuffer_remove()`

The function `gbuffer_remove()` deallocates memory associated with a [`gbuffer_t *`](#gbuffer_t) instance, including its internal data buffer and label, ensuring proper cleanup.

```C
void gbuffer_remove(gbuffer_t *gbuf);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the [`gbuffer_t`](#gbuffer_t) instance to be deallocated. |

**Returns**

This function does not return a value.

**Notes**

This function should not be called directly. Instead, use `gbuffer_decref()` to manage reference counting and ensure proper deallocation.

---

(gbuffer_serialize)=
## `gbuffer_serialize()`

`gbuffer_serialize()` converts a [`gbuffer_t *`](#gbuffer_t) into a JSON object, encoding its data in Base64 format.

```C
json_t *gbuffer_serialize(
    hgobj gobj,
    gbuffer_t *gbuf  // not owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` context for logging and error handling. |
| `gbuf` | `gbuffer_t *` | The [`gbuffer_t *`](#gbuffer_t) to be serialized. The buffer is not modified or owned by the function. |

**Returns**

A JSON object containing the serialized [`gbuffer_t *`](#gbuffer_t), including its label, mark, and Base64-encoded data.

**Notes**

The function encodes the buffer's data in Base64 format to ensure safe storage and transmission. The returned JSON object must be freed by the caller.

---

(gbuffer_set_rd_offset)=
## `gbuffer_set_rd_offset()`

Sets the read offset of the given `gbuffer_t` instance to the specified position, ensuring it does not exceed the allocated data size or the current write position.

```C
int gbuffer_set_rd_offset(
    gbuffer_t *gbuf,
    size_t     position
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance whose read offset is to be set. |
| `position` | `size_t` | The new read offset position within the buffer. |

**Returns**

Returns `0` on success. Returns `-1` if the specified position exceeds the allocated data size or the current write position.

**Notes**

If `position` is greater than the allocated data size or the current write position, an error is logged and `-1` is returned.

---

(gbuffer_set_wr)=
## `gbuffer_set_wr()`

Sets the write offset of the `gbuffer_t` structure, adjusting the position where new data will be written.

```C
int gbuffer_set_wr(
    gbuffer_t *gbuf,
    size_t     offset
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` structure whose write offset is to be set. |
| `offset` | `size_t` | The new write offset position within the buffer. |

**Returns**

Returns 0 on success. Returns -1 if the specified offset exceeds the buffer size.

**Notes**

This function is useful when using [`gbuffer_t`](#gbuffer_t) as a write buffer, allowing manual control over the write position.

---

(gbuffer_setlabel)=
## `gbuffer_setlabel()`

Sets the label of the given [`gbuffer_t *`](#gbuffer_t). If a label already exists, it is freed before assigning the new one.

```C
int gbuffer_setlabel(
    gbuffer_t *gbuf,
    const char *label
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the [`gbuffer_t *`](#gbuffer_t) whose label is to be set. |
| `label` | `const char *` | New label string to assign. If `NULL`, the existing label is removed. |

**Returns**

Returns `0` on success. If `gbuf` is `NULL`, an error is logged and `-1` is returned.

**Notes**

If a label is already set, it is freed before assigning the new one. The function duplicates the input string to ensure memory safety.

---

(gbuffer_vprintf)=
## `gbuffer_vprintf()`

`gbuffer_vprintf()` appends a formatted string to a [`gbuffer_t *`](#gbuffer_t) using a `va_list` argument list.

```C
int gbuffer_vprintf(
    gbuffer_t *gbuf,
    const char *format,
    va_list ap) JANSSON_ATTRS((format(printf, 2, 0))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the [`gbuffer_t *`](#gbuffer_t) where the formatted string will be appended. |
| `format` | `const char *` | Format string specifying how subsequent arguments are converted for output. |
| `ap` | `va_list` | Variable argument list containing the values to format and append. |

**Returns**

Returns the number of characters written, or a negative value if an error occurs.

**Notes**

If the buffer is too small, [`gbuffer_vprintf()`](#gbuffer_vprintf) attempts to reallocate it before writing.

---

(gobj_trace_dump_full_gbuf)=
## `gobj_trace_dump_full_gbuf()`

Logs a full hexdump of the given [`gbuffer_t *`](#gbuffer_t) including all written data.

```C
void gobj_trace_dump_full_gbuf(
    hgobj gobj,
    gbuffer_t *gbuf,
    const char *fmt,
    ... ) JANSSON_ATTRS((format(printf, 3, 4))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance used for logging. |
| `gbuf` | `gbuffer_t *` | The [`gbuffer_t *`](#gbuffer_t) to be dumped. |
| `fmt` | `const char *` | A format string for additional log message details. |
| `...` | `variadic` | Additional arguments for the format string. |

**Returns**

This function does not return a value.

**Notes**

Unlike [`gobj_trace_dump_gbuf()`](#gobj_trace_dump_gbuf), this function logs the entire buffer, not just a chunk.

---

(gobj_trace_dump_gbuf)=
## `gobj_trace_dump_gbuf()`

Logs a hexadecimal dump of the given [`gbuffer_t *`](#gbuffer_t), displaying a chunk of its data for debugging purposes.

```C
void gobj_trace_dump_gbuf(
    hgobj gobj,
    gbuffer_t *gbuf,
    const char *fmt,
    ... ) JANSSON_ATTRS((format(printf, 3, 4))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The `hgobj` instance used for logging. |
| `gbuf` | `gbuffer_t *` | The [`gbuffer_t *`](#gbuffer_t) to be dumped. |
| `fmt` | `const char *` | A format string for additional log message details. |
| `...` | `variadic` | Additional arguments for the format string. |

**Returns**

This function does not return a value.

**Notes**

Only a chunk of the [`gbuffer_t *`](#gbuffer_t) data is printed. Use [`gobj_trace_dump_full_gbuf()`](#gobj_trace_dump_full_gbuf) to log the entire buffer.

---

(json2gbuf)=
## `json2gbuf()`

The function `json2gbuf()` serializes a JSON object into a `gbuffer_t` structure, appending the JSON data to the buffer.

```C
gbuffer_t *json2gbuf(
    gbuffer_t *gbuf,
    json_t *jn,
    size_t flags
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | A pointer to an existing `gbuffer_t` buffer. If NULL, a new buffer is created. |
| `jn` | `json_t *` | A JSON object to be serialized. The function takes ownership of this parameter. |
| `flags` | `size_t` | Flags controlling the JSON serialization behavior. |

**Returns**

Returns a pointer to a `gbuffer_t` containing the serialized JSON data. If an error occurs, NULL is returned.

**Notes**

The function uses `json_dump_callback()` to serialize the JSON object into the buffer. If `gbuf` is NULL, a new buffer is allocated with a default size.

---

(config_gbuffer2json)=
## `config_gbuffer2json()`

Parses the contents of a `gbuffer_t` as a configuration string and returns the resulting JSON object. The buffer is consumed (decremented) by this function.

```C
json_t *config_gbuffer2json(
    gbuffer_t *gbuf,
    int verbose
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | The buffer containing the configuration string to parse. Ownership is taken -- the buffer is decremented after use. |
| `verbose` | `int` | Verbosity level for error reporting: 1 logs errors, 2 logs errors and dumps the buffer content. |

**Returns**

Returns a `json_t *` object parsed from the buffer content, or NULL if parsing fails.

**Notes**

The function reads from the current read position of the buffer, passes the data to `json_config_string2json()`, and then calls `gbuffer_decref()` on the buffer.

---

(gbuf2file)=
## `gbuf2file()`

Writes the entire contents of a `gbuffer_t` to a file on disk. The buffer is consumed (decremented) after the operation, regardless of success or failure.

```C
int gbuf2file(
    hgobj gobj,
    gbuffer_t *gbuf,
    const char *path,
    int permission,
    BOOL overwrite
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The GObj instance used for error logging. |
| `gbuf` | `gbuffer_t *` | The buffer whose contents will be written to the file. Ownership is taken -- the buffer is decremented after the operation. |
| `path` | `const char *` | The file path where the data will be written. |
| `permission` | `int` | File permission mode (e.g. 0644) applied when creating the file. |
| `overwrite` | `BOOL` | If TRUE, an existing file at `path` will be overwritten. If FALSE, the function fails if the file already exists. |

**Returns**

Returns 0 on success, or -1 on error (e.g. file creation failure or write error).

**Notes**

The function writes all available chunks from the buffer sequentially. The buffer is always decremented after the call, even if an error occurs.

---

(gbuffer_append_json)=
## `gbuffer_append_json()`

Serializes a JSON object to a string and appends it to a `gbuffer_t`, followed by a newline character.

```C
size_t gbuffer_append_json(
    gbuffer_t *gbuf,
    json_t *jn
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | The buffer to append the serialized JSON to. |
| `jn` | `json_t *` | The JSON object to serialize. Ownership is taken -- the reference is decremented after use. |

**Returns**

Returns the number of bytes appended (excluding the trailing newline), or -1 (cast to `size_t`) on serialization error.

**Notes**

The JSON object is converted to a compact string using `json2str()`, appended to the buffer, and then a `\n` character is appended. The JSON reference is always decremented, even on error.

---

(gbuffer_base64_to_binary)=
## `gbuffer_base64_to_binary()`

Decodes a Base64-encoded string into a new `gbuffer_t` containing the raw binary data.

```C
gbuffer_t *gbuffer_base64_to_binary(
    const char *base64,
    size_t base64_len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `base64` | `const char *` | Pointer to the Base64-encoded input string. |
| `base64_len` | `size_t` | Length of the Base64 input string in bytes. |

**Returns**

Returns a new `gbuffer_t` containing the decoded binary data, or NULL on error (e.g. NULL input, memory allocation failure, or decoding error).

**Notes**

The output buffer is sized based on the expected decoded length. The caller is responsible for calling `gbuffer_decref()` on the returned buffer when done.

---

(gbuffer_binary_to_base64)=
## `gbuffer_binary_to_base64()`

Encodes raw binary data into a new `gbuffer_t` containing the Base64-encoded string.

```C
gbuffer_t *gbuffer_binary_to_base64(
    const char *src,
    size_t len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `src` | `const char *` | Pointer to the raw binary data to encode. |
| `len` | `size_t` | Length of the input data in bytes. |

**Returns**

Returns a new `gbuffer_t` containing the Base64-encoded string, or NULL on error (e.g. memory allocation failure or encoding error).

**Notes**

The output buffer is sized to hold the full Base64-encoded output. The caller is responsible for calling `gbuffer_decref()` on the returned buffer when done.

---

(gbuffer_file2base64)=
## `gbuffer_file2base64()`

Reads a file from disk and returns its contents as a Base64-encoded string in a new `gbuffer_t`.

```C
gbuffer_t *gbuffer_file2base64(
    const char *path
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `path` | `const char *` | Path to the file to read and encode. |

**Returns**

Returns a new `gbuffer_t` containing the Base64-encoded file contents, or NULL on error (e.g. file not found, memory allocation failure).

**Notes**

The function reads the entire file into memory, encodes it using [`gbuffer_binary_to_base64()`](#gbuffer_binary_to_base64), and frees the intermediate buffer. The caller is responsible for calling `gbuffer_decref()` on the returned buffer when done.

---

(str2gbuf)=
## `str2gbuf()`

Creates a new `gbuffer_t` and fills it with a printf-formatted string.

```C
gbuffer_t *str2gbuf(
    const char *fmt,
    ... ) JANSSON_ATTRS((format(printf, 1, 2))
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `fmt` | `const char *` | A printf-style format string. |
| `...` | `variadic` | Additional arguments corresponding to the format specifiers in `fmt`. |

**Returns**

Returns a new `gbuffer_t` containing the formatted string, or NULL on allocation failure.

**Notes**

The formatted output is limited to `PATH_MAX` bytes. The buffer is created with an exact size matching the resulting string length. The caller is responsible for calling `gbuffer_decref()` on the returned buffer when done.

---

