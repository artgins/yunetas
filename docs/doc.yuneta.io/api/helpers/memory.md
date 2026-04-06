# Memory

Yuneta's memory management API: allocate, free, and reallocate memory through the system-wide allocator configured at startup. All gobj-aware code should use these helpers instead of raw `malloc`/`free` so that leak tracking and memory limits work correctly.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)

(print_track_mem)=
## `print_track_mem()`

Prints a debug log of memory allocations that have not been freed, helping to track memory leaks.

```C
void print_track_mem(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

None.

**Notes**

This function is only effective when CONFIG_TRACK_MEMORY is enabled. It logs memory blocks that have not been freed, aiding in debugging memory leaks.

---

(set_memory_check_list)=
## `set_memory_check_list()`

Sets a global list of memory addresses to be checked for tracking memory allocations.

```C
void set_memory_check_list(unsigned long *memory_check_list);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `memory_check_list` | `unsigned long *` | Pointer to an array of memory addresses to be monitored. |

**Returns**

This function does not return a value.

**Notes**

This function is only available when `CONFIG_TRACK_MEMORY` is defined. It allows tracking specific memory allocations for debugging purposes.

---

(gbmem_calloc)=
## `gbmem_calloc()`

Allocates memory for an array of elements and initializes them to zero.

```C
void *gbmem_calloc(
    size_t n,
    size_t size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `n` | `size_t` | Number of elements to allocate. |
| `size` | `size_t` | Size of each element in bytes. |

**Returns**

Pointer to the allocated zeroed memory, or `NULL` on failure.

**Notes**

The total allocation is `n * size` bytes. All bytes are initialized to zero before returning.

---

(gbmem_free)=
## `gbmem_free()`

Frees a block of memory previously allocated by gbmem allocation functions.

```C
void gbmem_free(
    void *ptr
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ptr` | `void *` | Pointer to the memory block to free. Can be `NULL`, in which case no action is performed. |

**Returns**

None.

**Notes**

Only pass pointers obtained from `gbmem_malloc()`, `gbmem_calloc()`, `gbmem_realloc()`, `gbmem_strdup()`, or `gbmem_strndup()`. Passing a `NULL` pointer is safe and has no effect.

---

(gbmem_get_allocators)=
## `gbmem_get_allocators()`

Retrieves the current memory allocator function pointers.

```C
int gbmem_get_allocators(
    sys_malloc_fn_t *malloc_func,
    sys_realloc_fn_t *realloc_func,
    sys_calloc_fn_t *calloc_func,
    sys_free_fn_t *free_func
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `malloc_func` | `sys_malloc_fn_t *` | Pointer to receive the current malloc function. Can be `NULL` if not needed. |
| `realloc_func` | `sys_realloc_fn_t *` | Pointer to receive the current realloc function. Can be `NULL` if not needed. |
| `calloc_func` | `sys_calloc_fn_t *` | Pointer to receive the current calloc function. Can be `NULL` if not needed. |
| `free_func` | `sys_free_fn_t *` | Pointer to receive the current free function. Can be `NULL` if not needed. |

**Returns**

0 on success.

**Notes**

Any parameter can be `NULL` if you do not need that particular function pointer.

---

(gbmem_get_maximum_block)=
## `gbmem_get_maximum_block()`

Returns the maximum size of a single memory allocation block.

```C
size_t gbmem_get_maximum_block(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Maximum allocation block size in bytes. The default is 16 MB.

**Notes**

This limit is set during `gbmem_setup()` via the `mem_max_block` parameter.

---

(gbmem_malloc)=
## `gbmem_malloc()`

Allocates a block of memory of the specified size.

```C
void *gbmem_malloc(
    size_t size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `size` | `size_t` | Number of bytes to allocate. |

**Returns**

Pointer to the allocated memory block, or `NULL` on failure.

**Notes**

The contents of the allocated block are uninitialized. Use `gbmem_calloc()` if you need zero-initialized memory.

---

(gbmem_realloc)=
## `gbmem_realloc()`

Resizes a previously allocated memory block.

```C
void *gbmem_realloc(
    void *ptr,
    size_t size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `ptr` | `void *` | Pointer to the memory block to resize. |
| `size` | `size_t` | New size in bytes. |

**Returns**

Pointer to the resized memory block, or `NULL` on failure.

**Notes**

If `ptr` is `NULL`, this behaves like `gbmem_malloc()`. The returned pointer may differ from `ptr` if the block was moved. Contents up to the minimum of the old and new sizes are preserved.

---

(gbmem_set_allocators)=
## `gbmem_set_allocators()`

Replaces the memory allocator functions with custom implementations.

```C
int gbmem_set_allocators(
    sys_malloc_fn_t malloc_func,
    sys_realloc_fn_t realloc_func,
    sys_calloc_fn_t calloc_func,
    sys_free_fn_t free_func
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `malloc_func` | `sys_malloc_fn_t` | Custom malloc function. |
| `realloc_func` | `sys_realloc_fn_t` | Custom realloc function. |
| `calloc_func` | `sys_calloc_fn_t` | Custom calloc function. |
| `free_func` | `sys_free_fn_t` | Custom free function. |

**Returns**

0 on success.

**Notes**

All subsequent `gbmem_*` calls will use the provided functions instead of the defaults. Use `gbmem_get_allocators()` to retrieve the current set before replacing them.

---

(gbmem_setup)=
## `gbmem_setup()`

Initializes the memory manager with custom configuration parameters.

```C
int gbmem_setup(
    size_t mem_max_block,
    size_t mem_max_system_memory,
    BOOL use_own_system_memory,
    size_t mem_min_block,
    size_t mem_superblock
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `mem_max_block` | `size_t` | Maximum size of a single allocation block. Default is 16 MB. |
| `mem_max_system_memory` | `size_t` | Maximum total memory the allocator may use. Default is 64 MB. |
| `use_own_system_memory` | `BOOL` | Reserved for future use. |
| `mem_min_block` | `size_t` | Minimum block size. Default is 512 bytes. |
| `mem_superblock` | `size_t` | Superblock size. Default is 16 MB. |

**Returns**

0 on success.

**Notes**

Must be called before any other `gbmem_*` allocation function. Typically called once during yuno initialization.

---

(gbmem_shutdown)=
## `gbmem_shutdown()`

Shuts down the memory manager and cleans up resources.

```C
void gbmem_shutdown(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

None.

**Notes**

Should be called during yuno shutdown. After this call, no further `gbmem_*` allocation functions should be used.

---

(gbmem_strdup)=
## `gbmem_strdup()`

Duplicates a null-terminated string.

```C
char *gbmem_strdup(
    const char *str
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str` | `const char *` | The null-terminated string to duplicate. |

**Returns**

Pointer to the newly allocated copy of the string, or `NULL` on failure.

**Notes**

The returned string must be freed with `gbmem_free()`.

---

(gbmem_strndup)=
## `gbmem_strndup()`

Duplicates a substring of specified length.

```C
char *gbmem_strndup(
    const char *str,
    size_t size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str` | `const char *` | The source string. |
| `size` | `size_t` | Maximum number of characters to copy. |

**Returns**

Pointer to the newly allocated string, or `NULL` on failure.

**Notes**

Copies at most `size` characters from `str` and appends a null terminator. The returned string must be freed with `gbmem_free()`.

---

(get_cur_system_memory)=
## `get_cur_system_memory()`

Returns the current amount of system memory in use by the allocator.

```C
size_t get_cur_system_memory(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Current memory usage in bytes, or 0 if memory tracking is not enabled.

**Notes**

Useful for monitoring memory consumption at runtime.

---

(get_max_system_memory)=
## `get_max_system_memory()`

Returns the maximum amount of system memory allowed by the allocator.

```C
size_t get_max_system_memory(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Maximum allowed memory in bytes. The default is 64 MB.

**Notes**

This limit is set during `gbmem_setup()` via the `mem_max_system_memory` parameter.

---

