(gbuffer)=
# **GBuffer**


The `gbuffer_t` structure is a flexible, dynamic buffer designed to handle data efficiently. It supports reading, writing, serialization, and other utility operations. `gbuffer_t` is commonly used in Yuneta to manage data streams, encode/decode information, and store dynamic content.

Source code in:

- [gbuffer.c](https://github.com/artgins/yunetas/blob/7.8.1/kernel/c/gobj-c/src/gbuffer.c)
- [gobj.h](https://github.com/artgins/yunetas/blob/7.8.1/kernel/c/gobj-c/src/gobj.h)


---

## Structure Overview

```{figure} ../_static/gbuffer_layout.svg
:alt: A gbuffer data block with two cursors — curp (read) and tail (write). Bytes before curp are consumed, curp to tail is unread data, tail to data_size is free allocated space, and the buffer grows to max_memory_size.
:width: 100%

`curp` (read) and `tail` (write) are independent cursors over one `data`
block, so reading and writing never interfere.
```

(gbuffer_t)=
The `gbuffer_t` structure includes the following key fields:

| **Field**              | **Description**                                                                 |
|-------------------------|---------------------------------------------------------------------------------|
| `refcount`             | Reference counter for memory management.                                        |
| `label`                | A user-defined label for identifying the buffer.                                |
| `mark`                 | A user-defined marker for specific positions in the buffer.                     |
| `data_size`            | The size of the allocated memory for the buffer.                                |
| `max_memory_size`      | The maximum size the buffer can grow to in memory.                              |
| `tail`                 | Write pointer indicating the end of the written data.                          |
| `curp`                 | Read pointer indicating the current read position.                             |
| `data`                 | Dynamically allocated memory containing the buffer's data.                     |

---

## Key Features

### 1. **Reference Counting**
- The buffer uses a reference counter (`refcount`) to manage memory safely.
- Functions like [`gbuffer_incref`](https://github.com/artgins/yunetas/blob/7.8.1/kernel/c/gobj-c/src/gbuffer.h#L78) and [`gbuffer_decref`](https://github.com/artgins/yunetas/blob/7.8.1/kernel/c/gobj-c/src/gbuffer.h#L93) ensure that the buffer is only freed when no references remain.

### 2. **Dynamic Memory Management**
- The buffer dynamically allocates and grows its memory up to `max_memory_size`.
- The `data_size` field tracks the currently allocated size.

### 3. **Reading and Writing**
- Separate read (`curp`) and write (`tail`) pointers allow for efficient reading and writing without interfering with each other.
- Functions like [`gbuffer_get`](#gbuffer_get) and [`gbuffer_append`](#gbuffer_append) provide convenient access to manipulate data.

### 4. **Serialization and Encoding**
- Supports serialization to and from JSON objects.
- Provides [Base64](https://datatracker.ietf.org/doc/html/rfc4648) encoding and decoding for binary data.
- **Hardened deserialization (since 7.6.0).** `gbuffer_deserialize()` is reachable
  pre-auth (e.g. the ievent server), so it is defensive against hostile input: a
  malformed base64 `data` field that decodes to NULL is rejected instead of being
  fed into `gbuffer_setmark()` (was a pre-auth NULL-deref crash), and
  `gbuffer_create()` refuses a `data_size == SIZE_MAX` (the `+1` would wrap to a
  0-byte allocation that slipped past the max-block guard).

### 5. **Utility Functions**
- Functions for setting markers (`mark`) and labels (`label`).
- Utility functions for resetting, clearing, or calculating free and used space.

---

## Core Operations

### Reading
- **Retrieve Data:** Access data from the current read pointer using functions like `gbuffer_get` and [`gbuffer_getchar`](https://github.com/artgins/yunetas/blob/7.8.1/kernel/c/gobj-c/src/gbuffer.h#L152).
- **Reset Pointer:** Reset the read pointer to the start of the buffer with [`gbuffer_reset_rd`](https://github.com/artgins/yunetas/blob/7.8.1/kernel/c/gobj-c/src/gbuffer.h#L127).

### Writing
- **Append Data:** Add data to the buffer using `gbuffer_append` or [`gbuffer_append_string`](https://github.com/artgins/yunetas/blob/7.8.1/kernel/c/gobj-c/src/gbuffer.h#L199).
- **Reset Pointer:** Clear written data by resetting the write pointer with [`gbuffer_reset_wr`](https://github.com/artgins/yunetas/blob/7.8.1/kernel/c/gobj-c/src/gbuffer.h#L189).

### Utility
- **Memory Info:** Retrieve statistics like free space ([`gbuffer_freebytes`](https://github.com/artgins/yunetas/blob/7.8.1/kernel/c/gobj-c/src/gbuffer.h#L257)) or total bytes ([`gbuffer_totalbytes`](https://github.com/artgins/yunetas/blob/7.8.1/kernel/c/gobj-c/src/gbuffer.h#L251)).
- **Markers and Labels:** Set or get markers ([`gbuffer_setmark`](https://github.com/artgins/yunetas/blob/7.8.1/kernel/c/gobj-c/src/gbuffer.h#L271), [`gbuffer_getmark`](https://github.com/artgins/yunetas/blob/7.8.1/kernel/c/gobj-c/src/gbuffer.h#L286)) and labels ([`gbuffer_setlabel`](#gbuffer_setlabel), [`gbuffer_getlabel`](https://github.com/artgins/yunetas/blob/7.8.1/kernel/c/gobj-c/src/gbuffer.h#L265)).

### Serialization and Encoding
- **JSON Serialization:** Convert the buffer to and from JSON objects using [`gbuffer_serialize`](#gbuffer_serialize) and [`gbuffer_deserialize`](#gbuffer_deserialize).
- **Base64 Encoding/Decoding:** Encode or decode data in Base64 format 
    with `gbuffer_string_to_base64`, `gbuffer_base64_to_string` and [`gbuffer_encode_base64`](#gbuffer_encode_base64).

---

## Benefits of `gbuffer_t`

- **Efficiency:** Separate read and write pointers allow simultaneous operations without reallocations.
- **Flexibility:** Support for dynamic memory allocation and customizable markers or labels.
- **Extensibility:** Serialization and encoding functions make it versatile for different use cases.
- **Safety:** Reference counting ensures proper memory management and prevents premature deallocation.

---

## Practical Applications

1. **Data Streams:**
    - Manage incoming or outgoing data streams efficiently with dynamic resizing and memory tracking.
2. **Serialization:**
    - Convert structured data to JSON or Base64 for storage or transmission.
3. **Buffering:**
    - Temporarily store data for asynchronous processing or delayed reading.
