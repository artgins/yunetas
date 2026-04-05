# GBuffer Functions

Source code in:

- [gbuffer.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gbuffer.c)
- [gobj.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)

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
    void      *bf,
    size_t     len
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

(gbuffer_append_char)=
## `gbuffer_append_char()`

Appends a single character to the [`gbuffer_t`](#gbuffer_t) buffer, expanding it if necessary.

```C
size_t gbuffer_append_char(
    gbuffer_t *gbuf,
    char       c
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the [`gbuffer_t`](#gbuffer_t) buffer where the character will be appended. |
| `c` | `char` | The character to append to the buffer. |

**Returns**

Returns the number of bytes written, which is always 1 unless an error occurs.

**Notes**

If the buffer does not have enough space, it will attempt to expand before appending the character.

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

(gbuffer_append_string)=
## `gbuffer_append_string()`

`gbuffer_append_string()` appends a null-terminated string to the given `gbuffer_t` buffer, increasing its size if necessary.

```C
size_t gbuffer_append_string(
    gbuffer_t *gbuf,
    const char *s
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` buffer where the string will be appended. |
| `s` | `const char *` | Null-terminated string to append to the buffer. |

**Returns**

Returns the number of bytes successfully appended to the buffer.

**Notes**

If the buffer does not have enough space, it will be reallocated to accommodate the new data.

---

(gbuffer_base64_to_string)=
## `gbuffer_base64_to_string()`

`gbuffer_base64_to_string()` decodes a Base64-encoded string into a newly allocated [`gbuffer_t *`](#gbuffer_t).

```C
gbuffer_t *gbuffer_base64_to_string(
    const char *base64,
    size_t      base64_len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `base64` | `const char *` | Pointer to the Base64-encoded input string. |
| `base64_len` | `size_t` | Length of the Base64-encoded input string. |

**Returns**

Returns a newly allocated [`gbuffer_t *`](#gbuffer_t) containing the decoded data, or `NULL` if decoding fails.

**Notes**

The returned [`gbuffer_t *`](#gbuffer_t) must be freed using [`gbuffer_decref()`](#gbuffer_decref) to avoid memory leaks.

---

(gbuffer_chunk)=
## `gbuffer_chunk()`

`gbuffer_chunk()` returns the number of bytes available for reading in the given `gbuffer_t` instance, ensuring that the returned value does not exceed the allocated buffer size.

```C
PUBLIC size_t gbuffer_chunk(gbuffer_t *gbuf);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance whose available read chunk size is to be determined. |

**Returns**

Returns the number of bytes available for reading, ensuring it does not exceed the allocated buffer size.

**Notes**

This function calculates the available data chunk by taking the minimum of the remaining unread bytes and the allocated buffer size.

---

(gbuffer_clear)=
## `gbuffer_clear()`

Resets the write and read pointers of the given [`gbuffer_t *`](#gbuffer_t), effectively clearing its contents.

```C
void gbuffer_clear(
    gbuffer_t *gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the [`gbuffer_t`](#gbuffer_t) structure to be cleared. |

**Returns**

This function does not return a value.

**Notes**

This function does not deallocate memory; it only resets the buffer's state.

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

(gbuffer_cur_rd_pointer)=
## `gbuffer_cur_rd_pointer()`

Returns a pointer to the current read position in the `gbuffer_t` structure.

```C
void *gbuffer_cur_rd_pointer(
    gbuffer_t *gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` structure. |

**Returns**

Returns a pointer to the current read position in the buffer.

**Notes**

Ensure that `gbuf` is not NULL before calling this function to avoid undefined behavior.

---

(gbuffer_cur_wr_pointer)=
## `gbuffer_cur_wr_pointer()`

Returns a pointer to the current write position in the given `gbuffer_t` structure.

```C
void *gbuffer_cur_wr_pointer(
    gbuffer_t *gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` structure whose write position is to be retrieved. |

**Returns**

Returns a pointer to the current write position in the buffer.

**Notes**

The returned pointer can be used to directly write data into the buffer. Ensure that the buffer has enough space before writing.

---

(gbuffer_decref)=
## `gbuffer_decref()`

Decrements the reference count of a `gbuffer_t` instance and deallocates it if the count reaches zero.

```C
void gbuffer_decref(gbuffer_t *gbuf);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance whose reference count is to be decremented. |

**Returns**

This function does not return a value.

**Notes**

If the reference count reaches zero, the memory associated with the `gbuffer_t` instance is freed. Ensure that the `gbuffer_t` instance is valid before calling [`gbuffer_decref()`](#gbuffer_decref).

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

(gbuffer_freebytes)=
## `gbuffer_freebytes()`

`gbuffer_freebytes()` returns the number of free bytes available in the buffer for writing.

```C
size_t gbuffer_freebytes(gbuffer_t *gbuf);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` structure whose free space is to be determined. |

**Returns**

Returns the number of free bytes available in the buffer.

**Notes**

Ensure that `gbuf` is not NULL before calling [`gbuffer_freebytes()`](#gbuffer_freebytes) to avoid potential errors.

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

(gbuffer_get_rd_offset)=
## `gbuffer_get_rd_offset()`

Returns the current read offset of the given `gbuffer_t` instance.

```C
size_t gbuffer_get_rd_offset(gbuffer_t *gbuf);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance whose read offset is to be retrieved. |

**Returns**

The current read offset as a `size_t` value.

**Notes**

The function does not perform any validation on the `gbuf` pointer before accessing its read offset.

---

(gbuffer_getchar)=
## `gbuffer_getchar()`

`gbuffer_getchar()` retrieves and removes a single byte from the given `gbuffer_t` instance, advancing the read pointer.

```C
char gbuffer_getchar(
    gbuffer_t *gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance from which a byte will be retrieved. |

**Returns**

Returns the retrieved byte if available; otherwise, returns `0` if no data remains.

**Notes**

Ensure that `gbuffer_leftbytes(gbuf)` is greater than zero before calling [`gbuffer_getchar()`](#gbuffer_getchar) to avoid retrieving an invalid byte.

---

(gbuffer_getlabel)=
## `gbuffer_getlabel()`

Retrieves the label associated with the given `gbuffer_t` instance.

```C
char *gbuffer_getlabel(gbuffer_t *gbuf);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance whose label is to be retrieved. |

**Returns**

Returns a pointer to the label string if it exists, otherwise returns `NULL`.

**Notes**

The returned label is owned by the `gbuffer_t` instance and should not be modified or freed by the caller.

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

(gbuffer_getmark)=
## `gbuffer_getmark()`

Retrieves the mark value associated with the given `gbuffer_t` instance.

```C
PUBLIC size_t gbuffer_getmark(gbuffer_t *gbuf);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance from which to retrieve the mark value. |

**Returns**

Returns the mark value stored in the `gbuffer_t` instance. If `gbuf` is NULL, returns 0.

**Notes**

The mark value is a user-defined field that can be used to store arbitrary metadata associated with the buffer.

---

(gbuffer_head_pointer)=
## `gbuffer_head_pointer()`

Returns a pointer to the first position of data in the given `gbuffer_t` instance.

```C
void *gbuffer_head_pointer(
    gbuffer_t *gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance whose data head is to be retrieved. |

**Returns**

Returns a pointer to the first byte of data in the buffer. If `gbuf` is NULL, an error is logged and NULL is returned.

**Notes**

This function does not modify the buffer; it only provides access to the beginning of the stored data.

---

(gbuffer_incref)=
## `gbuffer_incref()`

`gbuffer_incref()` increments the reference count of a `gbuffer_t` instance, ensuring it is not prematurely deallocated.

```C
gbuffer_t *gbuffer_incref(
    gbuffer_t *gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance whose reference count is to be incremented. |

**Returns**

Returns the same `gbuffer_t *` instance with an incremented reference count, or `NULL` if the input is invalid.

**Notes**

If `gbuf` is `NULL` or has an invalid reference count, an error is logged and `NULL` is returned.

---

(gbuffer_leftbytes)=
## `gbuffer_leftbytes()`

`gbuffer_leftbytes()` returns the number of unread bytes remaining in the given `gbuffer_t` instance.

```C
size_t gbuffer_leftbytes(gbuffer_t *gbuf);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance whose unread byte count is to be retrieved. |

**Returns**

Returns the number of unread bytes remaining in the buffer.

**Notes**

If `gbuf` is `NULL`, an error is logged, and `0` is returned.

---

(gbuffer_printf)=
## `gbuffer_printf()`

The function `gbuffer_printf()` appends formatted text to a [`gbuffer_t *`](#gbuffer_t) buffer using a `printf`-style format string and arguments.

```C
int gbuffer_printf(
    gbuffer_t *gbuf,
    const char *format,
    ...
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

This function should not be called directly. Instead, use [`gbuffer_decref()`](#gbuffer_decref) to manage reference counting and ensure proper deallocation.

---

(gbuffer_reset_rd)=
## `gbuffer_reset_rd()`

Resets the read pointer of the given `gbuffer_t` instance to the beginning, allowing data to be read from the start again.

```C
void gbuffer_reset_rd(
    gbuffer_t *gbuf
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance whose read pointer will be reset. |

**Returns**

This function does not return a value.

**Notes**

This function is useful when re-reading data from a buffer without modifying its contents.

---

(gbuffer_reset_wr)=
## `gbuffer_reset_wr()`

Resets the write pointer of the given `gbuffer_t` instance, effectively clearing its contents.

```C
void gbuffer_reset_wr(gbuffer_t *gbuf);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance whose write pointer is to be reset. |

**Returns**

This function does not return a value.

**Notes**

This function sets both the write (`tail`) and read (`curp`) pointers to zero, effectively clearing the buffer. A null terminator is placed at the beginning of the buffer to ensure it remains a valid C string.

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

(gbuffer_setmark)=
## `gbuffer_setmark()`

Sets a marker value in the given `gbuffer_t` instance.

```C
void gbuffer_setmark(
    gbuffer_t *gbuf,
    size_t     mark
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance where the marker will be set. |
| `mark` | `size_t` | The marker value to set in the `gbuffer_t` instance. |

**Returns**

This function does not return a value.

**Notes**

The marker value can be used for custom tracking or reference purposes within the `gbuffer_t` instance.

---

(gbuffer_string_to_base64)=
## `gbuffer_string_to_base64()`

`gbuffer_string_to_base64()` encodes a given string into a Base64-encoded `gbuffer_t` object.

```C
gbuffer_t *gbuffer_string_to_base64(
    const char *src,
    size_t      len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `src` | `const char *` | Pointer to the input string to be encoded. |
| `len` | `size_t` | Length of the input string. |

**Returns**

Returns a newly allocated `gbuffer_t *` containing the Base64-encoded representation of the input string. Returns `NULL` on failure.

**Notes**

The returned `gbuffer_t *` must be freed using [`gbuffer_decref()`](#gbuffer_decref).

---

(gbuffer_totalbytes)=
## `gbuffer_totalbytes()`

`gbuffer_totalbytes()` returns the total number of bytes written in the given `gbuffer_t` instance.

```C
size_t gbuffer_totalbytes(gbuffer_t *gbuf);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance whose total written bytes are to be retrieved. |

**Returns**

Returns the total number of bytes written in the `gbuffer_t` instance.

**Notes**

This function does not modify the `gbuffer_t` instance; it only retrieves the current write position.

---

(gbuffer_ungetc)=
## `gbuffer_ungetc()`

Pushes a single character back onto the read buffer of the given `gbuffer_t` instance, effectively undoing the last read operation.

```C
PUBLIC int gbuffer_ungetc(
    gbuffer_t *gbuf,
    char       c
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gbuf` | `gbuffer_t *` | Pointer to the `gbuffer_t` instance where the character will be pushed back. |
| `c` | `char` | The character to be pushed back onto the read buffer. |

**Returns**

Returns 0 on success.

**Notes**

If the read pointer is already at the beginning of the buffer, the function does nothing.

---

(gbuffer_vprintf)=
## `gbuffer_vprintf()`

`gbuffer_vprintf()` appends a formatted string to a [`gbuffer_t *`](#gbuffer_t) using a `va_list` argument list.

```C
int gbuffer_vprintf(
    gbuffer_t *gbuf,
    const char *format,
    va_list ap
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
    hgobj       gobj,
    gbuffer_t  *gbuf,
    const char *fmt,
    ...
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
    ...
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
