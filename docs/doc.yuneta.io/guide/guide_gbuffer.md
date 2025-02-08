(gbuffer())=
# **GBuffer**


The `gbuffer_t` structure is a flexible, dynamic buffer designed to handle data efficiently. It supports reading, writing, serialization, and other utility operations. `gbuffer_t` is commonly used in Yuneta to manage data streams, encode/decode information, and store dynamic content.

Source code in:

- [gbuffer.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gbuffer.c)
- [gobj.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)


---

## Structure Overview

(gbuffer_t())=
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
- Functions like `gbuffer_incref` and `gbuffer_decref` ensure that the buffer is only freed when no references remain.

### 2. **Dynamic Memory Management**
- The buffer dynamically allocates and grows its memory up to `max_memory_size`.
- The `data_size` field tracks the currently allocated size.

### 3. **Reading and Writing**
- Separate read (`curp`) and write (`tail`) pointers allow for efficient reading and writing without interfering with each other.
- Functions like `gbuffer_get` and `gbuffer_append` provide convenient access to manipulate data.

### 4. **Serialization and Encoding**
- Supports serialization to and from JSON objects.
- Provides Base64 encoding and decoding for binary data.

### 5. **Utility Functions**
- Functions for setting markers (`mark`) and labels (`label`).
- Utility functions for resetting, clearing, or calculating free and used space.

---

## Core Operations

### Reading
- **Retrieve Data:** Access data from the current read pointer using functions like `gbuffer_get` and `gbuffer_getchar`.
- **Reset Pointer:** Reset the read pointer to the start of the buffer with `gbuffer_reset_rd`.

### Writing
- **Append Data:** Add data to the buffer using `gbuffer_append` or `gbuffer_append_string`.
- **Reset Pointer:** Clear written data by resetting the write pointer with `gbuffer_reset_wr`.

### Utility
- **Memory Info:** Retrieve statistics like free space (`gbuffer_freebytes`) or total bytes (`gbuffer_totalbytes`).
- **Markers and Labels:** Set or get markers (`gbuffer_setmark`, `gbuffer_getmark`) and labels (`gbuffer_setlabel`, `gbuffer_getlabel`).

### Serialization and Encoding
- **JSON Serialization:** Convert the buffer to and from JSON objects using `gbuffer_serialize` and `gbuffer_deserialize`.
- **Base64 Encoding/Decoding:** Encode or decode data in Base64 format 
    with `gbuffer_string_to_base64`, `gbuffer_base64_to_string` and `gbuffer_encode_base64`.

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
