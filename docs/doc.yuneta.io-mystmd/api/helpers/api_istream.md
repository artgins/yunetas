# IStream Functions

Source code in:

- [helpers.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.h)
- [helpers.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/helpers.c)

(istream_clear)=
## `istream_clear()`

`istream_clear()` resets both the reading and writing pointers of the given `istream_h` instance, effectively clearing its internal buffer.

```C
void istream_clear(
    istream_h istream
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | The input stream handle whose buffer will be cleared. |

**Returns**

This function does not return a value.

**Notes**

This function is equivalent to calling [`istream_reset_rd()`](#istream_reset_rd) and [`istream_reset_wr()`](#istream_reset_wr) in sequence.

---

(istream_consume)=
## `istream_consume()`

`istream_consume()` reads data from the provided buffer and appends it to the internal gbuffer of the given `istream_h` instance until a specified delimiter or byte count is reached.

```C
size_t istream_consume(
    istream_h istream,
    char *bf,
    size_t len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | The input stream handle managing the internal gbuffer. |
| `bf` | `char *` | Pointer to the buffer containing the data to be consumed. |
| `len` | `size_t` | The number of bytes available in `bf` to be consumed. |

**Returns**

Returns the number of bytes successfully consumed from `bf` and appended to the internal gbuffer.

**Notes**

If the `istream` is configured to read until a delimiter, it will check for the delimiter sequence in the accumulated data. If configured to read a fixed number of bytes, it will stop once the required amount is reached. If the delimiter or byte count is reached, the function triggers the configured event.

---

(istream_create)=
## `istream_create()`

`istream_create()` initializes and returns a new input stream (`istream_h`) with a specified buffer size and maximum capacity.

```C
istream_h istream_create(
    hgobj gobj,
    size_t data_size,
    size_t max_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent object managing the input stream. |
| `data_size` | `size_t` | The initial size of the buffer allocated for the input stream. |
| `max_size` | `size_t` | The maximum allowed size of the buffer. |

**Returns**

Returns a handle (`istream_h`) to the newly created input stream, or `NULL` if memory allocation fails.

**Notes**

The returned `istream_h` must be destroyed using [`istream_destroy()`](#istream_destroy) to free allocated resources.

---

(istream_cur_rd_pointer)=
## `istream_cur_rd_pointer()`

Returns the current read pointer of the `istream_h` instance, allowing access to the unread portion of the internal buffer.

```C
char *istream_cur_rd_pointer(
    istream_h istream
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | Handle to the input stream whose read pointer is to be retrieved. |

**Returns**

A pointer to the current read position in the internal buffer. Returns `NULL` if the `istream_h` handle is invalid.

**Notes**

Ensure that `istream` is properly initialized before calling this function to avoid undefined behavior.

---

(istream_destroy)=
## `istream_destroy()`

Releases all resources associated with the given `istream_h` instance, including its internal buffer and allocated memory.

```C
void istream_destroy(
    istream_h istream
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | Handle to the input stream instance to be destroyed. |

**Returns**

This function does not return a value.

**Notes**

After calling `istream_destroy()`, the `istream_h` handle becomes invalid and should not be used.

---

(istream_extract_matched_data)=
## `istream_extract_matched_data()`

Extracts the matched data from the input stream `istream_h` if the matching condition is met.

```C
char *istream_extract_matched_data(
    istream_h istream,
    size_t   *len
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | The input stream handle from which the matched data is extracted. |
| `len` | `size_t *` | Pointer to store the length of the extracted data. |

**Returns**

Returns a pointer to the extracted data if a match is found, otherwise returns `NULL`. The caller does not own the returned pointer.

**Notes**

If no match is found, the function returns `NULL`. The extracted data is not copied, and the pointer references internal memory.

---

(istream_get_gbuffer)=
## `istream_get_gbuffer()`

Retrieves the internal [`gbuffer_t *`](#gbuffer_t) associated with the given `istream_h *`.

```C
gbuffer_t *istream_get_gbuffer(
    istream_h istream
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | Handle to the `istream_h *` instance. |

**Returns**

Returns a pointer to the [`gbuffer_t *`](#gbuffer_t) associated with the given `istream_h *`.

**Notes**

The returned [`gbuffer_t *`](#gbuffer_t) is not owned by the caller and should not be freed directly.

---

(istream_is_completed)=
## `istream_is_completed()`

`istream_is_completed()` checks if the input stream has completed reading the expected data.

```C
PUBLIC BOOL istream_is_completed(
    istream_h istream
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | Handle to the input stream. |

**Returns**

Returns `TRUE` if the input stream has completed reading the expected data, otherwise returns `FALSE`.

**Notes**

['If `istream` is `NULL`, an error is logged and `FALSE` is returned.']

---

(istream_length)=
## `istream_length()`

`istream_length()` returns the number of unread bytes in the internal buffer of the given input stream.

```C
PUBLIC size_t istream_length(
    istream_h istream
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | Handle to the input stream whose unread byte count is to be retrieved. |

**Returns**

Returns the number of unread bytes in the internal buffer of the input stream.

**Notes**

['If `istream` is NULL, an error is logged, and the function returns 0.', 'This function is useful for checking how much data remains to be read from the stream.']

---

(istream_new_gbuffer)=
## `istream_new_gbuffer()`

Creates a new `gbuffer_t` for the given `istream_h` instance, replacing the existing buffer if present.

```C
int istream_new_gbuffer(
    istream_h istream,
    size_t    data_size,
    size_t    max_size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | The input stream handle whose buffer will be replaced. |
| `data_size` | `size_t` | The initial size of the new buffer. |
| `max_size` | `size_t` | The maximum allowed size of the new buffer. |

**Returns**

Returns `0` on success, or `-1` if memory allocation fails.

**Notes**

If a buffer already exists in `istream`, it is decremented and replaced with a new one.

---

(istream_pop_gbuffer)=
## `istream_pop_gbuffer()`

Retrieves and removes the current [`gbuffer_t *`](#gbuffer_t) from the given `istream_h *`, returning ownership to the caller.

```C
gbuffer_t *istream_pop_gbuffer(
    istream_h istream
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | The input stream handle from which the [`gbuffer_t *`](#gbuffer_t) will be extracted. |

**Returns**

Returns the [`gbuffer_t *`](#gbuffer_t) extracted from the `istream_h *`. The caller assumes ownership and must manage its lifecycle. Returns `NULL` if `istream` is invalid or empty.

**Notes**

After calling [`istream_pop_gbuffer()`](#istream_pop_gbuffer), the internal buffer of the `istream_h *` is set to `NULL`, meaning subsequent reads will require a new buffer.

---

(istream_read_until_delimiter)=
## `istream_read_until_delimiter()`

`istream_read_until_delimiter()` configures an input stream to read data until a specified delimiter is encountered.

```C
int istream_read_until_delimiter(
    istream_h   istream,
    const char  *delimiter,
    size_t      delimiter_size,
    gobj_event_t event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | Handle to the input stream. |
| `delimiter` | `const char *` | Pointer to the delimiter string. |
| `delimiter_size` | `size_t` | Size of the delimiter in bytes. |
| `event` | `gobj_event_t` | Event to trigger when the delimiter is encountered. |

**Returns**

Returns `0` on success, or `-1` if an error occurs.

**Notes**

If `delimiter_size` is `0`, an error is logged and `-1` is returned.

---

(istream_read_until_num_bytes)=
## `istream_read_until_num_bytes()`

Configures the `istream_h` stream to read until a specified number of bytes is accumulated before triggering an event.

```C
int istream_read_until_num_bytes(
    istream_h istream,
    size_t    num_bytes,
    gobj_event_t event
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | Handle to the input stream. |
| `num_bytes` | `size_t` | The number of bytes to accumulate before triggering the event. |
| `event` | `gobj_event_t` | The event to trigger when the specified number of bytes is reached. |

**Returns**

Returns 0 on success, or -1 if an error occurs.

**Notes**

Once the specified number of bytes is accumulated, the event is triggered with the collected data.

---

(istream_reset_rd)=
## `istream_reset_rd()`

Resets the reading pointer of the `istream_h` stream, allowing data to be read again from the beginning.

```C
int istream_reset_rd(
    istream_h istream
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | The input stream whose reading pointer will be reset. |

**Returns**

Returns 0 on success, or -1 if the `istream` or its internal buffer is NULL.

**Notes**

This function ensures that subsequent reads start from the beginning of the buffer. If the `istream` or its buffer is NULL, an error is logged.

---

(istream_reset_wr)=
## `istream_reset_wr()`

Resets the write pointer of the `istream_h` stream, effectively clearing any written data.

```C
int istream_reset_wr(
    istream_h istream
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | Handle to the input stream whose write pointer will be reset. |

**Returns**

Returns 0 on success, or -1 if the `istream` handle is NULL or invalid.

**Notes**

This function clears the write pointer but does not deallocate the buffer. Use [`istream_clear()`](#istream_clear) to reset both read and write pointers.

---
(macro_istream_create)=
## `ISTREAM_CREATE()`

Macro to create an `istream_h` instance with specified buffer sizes, ensuring proper memory management and error handling.

```C
ISTREAM_CREATE(
    var,       istream_h *
    gobj,      hgobj
    data_size, size_t
    max_size,  size_t
)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `var` | `istream_h *` | Pointer to the `istream_h` instance to be created. |
| `gobj` | `hgobj` | Handle to the gobj that owns the istream. |
| `data_size` | `size_t` | Initial buffer size for the istream. |
| `max_size` | `size_t` | Maximum buffer size allowed for the istream. |

**Returns**

None. The macro initializes `var` with a new `istream_h` instance or destroys an existing one before reinitialization.

**Notes**

If `var` is already allocated, it is destroyed before creating a new instance. The macro logs an error if `var` is not NULL before reallocation.

---

(macro_istream_destroy)=
## `ISTREAM_DESTROY()`

The `ISTREAM_DESTROY` macro safely destroys an `istream_h` instance by calling `istream_destroy()` and setting the pointer to `NULL`.

```C
#define ISTREAM_DESTROY(istream)
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `istream` | `istream_h` | A pointer to the `istream_h` instance to be destroyed. |

**Returns**

This macro does not return a value.

**Notes**

This macro ensures that the `istream_h` instance is properly deallocated and prevents accidental use of a dangling pointer by setting it to `NULL`.

---

