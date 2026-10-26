# Memory

Yuneta's memory management API: allocate, free, and reallocate memory through the system-wide allocator configured at startup. All gobj-aware code must use these helpers instead of raw `malloc`/`free` so that leak tracking and memory limits work correctly.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gobj.c)

(print_track_mem)=
## [`print_track_mem()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L324)

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

This function is only effective when `CONFIG_DEBUG_TRACK_MEMORY` is enabled (the Kconfig knob alone; since 7.16.5 it no longer also requires a Debug build type). It logs memory blocks that have not been freed, aiding in debugging memory leaks. On a clean shutdown it emits nothing.

---

(set_memory_check_list)=
## [`set_memory_check_list()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L314)

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

This function is only effective when `CONFIG_DEBUG_TRACK_MEMORY` is defined. It allows tracking specific memory allocations for debugging purposes.

---

(gbmem_calloc)=
## [`gbmem_calloc()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L231)

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
## [`gbmem_free()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L215)

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
## [`gbmem_get_allocators()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L156)

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

(gbmem_get_json_allocators)=
## [`gbmem_get_json_allocators()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L186)

Retrieves the pair of allocator functions to hand jansson with `json_set_alloc_funcs()`: the same memory tracking and limits as [`gbmem_malloc()`](<#gbmem_malloc>), without zeroing the block.

```C
int gbmem_get_json_allocators(
    sys_malloc_fn_t *malloc_func,
    sys_free_fn_t *free_func
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `malloc_func` | `sys_malloc_fn_t *` | Pointer to receive the malloc function for jansson. Can be `NULL` if not needed. |
| `free_func` | `sys_free_fn_t *` | Pointer to receive the free function for jansson. Can be `NULL` if not needed. |

**Returns**

0 on success.

**Notes**

jansson asks its allocator for the contract of `malloc()`, and it writes every byte it reads. Zeroing its blocks (the pair of an object, an integer, a string: most of the blocks of a yuno) only costs a `memset()` each, so this malloc does not zero. Everything else is as in `gbmem_malloc()`: the largest block, the memory tracking (`CONFIG_DEBUG_TRACK_MEMORY`) and the free function, which is the one of the other blocks.

In a build with `CONFIG_DEBUG_TRACK_MEMORY` the block is filled with `0xA5` instead of being left as it is: a read of a byte that jansson did not write reads garbage in the tests too, not only on a node.

If the allocators were replaced with [`gbmem_set_allocators()`](<#gbmem_set_allocators>), the answer is those.

`yuneta_entry_point()` does this for every yuno, and `C_AUTHZ` again after libjwt has pointed jansson at its own wrappers. A program that does not start through `yuneta_entry_point()` (a tool, a benchmark) does it itself, before its first json:

```C
sys_malloc_fn_t malloc_func;
sys_free_fn_t free_func;

gbmem_get_json_allocators(
    &malloc_func,
    &free_func
);

json_set_alloc_funcs(
    malloc_func,
    free_func
);
```

Use [`gbmem_get_allocators()`](<#gbmem_get_allocators>) for a library that needs `realloc()` or `calloc()` too, or zeroed memory.

---

(gbmem_get_maximum_block)=
## [`gbmem_get_maximum_block()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L286)

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
## [`gbmem_malloc()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L207)

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

The block is zeroed, as with `gbmem_calloc()`. The blocks of jansson are not: see [`gbmem_get_json_allocators()`](<#gbmem_get_json_allocators>).

---

(gbmem_realloc)=
## [`gbmem_realloc()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L223)

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

If `ptr` is `NULL`, this behaves like `gbmem_malloc()`. The returned pointer can differ from `ptr` if the block was moved. Contents up to the minimum of the old and new sizes are preserved.

When `size` is larger than the largest block ([`gbmem_get_maximum_block()`](<#gbmem_get_maximum_block>)), the answer is `NULL` and an ERROR *"SIZE GREATER THAN MAX_BLOCK"* says so. Then `ptr` is still valid, with its content, and it is still counted by the memory tracking (`CONFIG_DEBUG_TRACK_MEMORY`). Keep it, and free it later as usual. In 7.25.4 and earlier, a refused `gbmem_realloc()` took the block out of the tracking first: its free logged *"Wrong dl_item_t, WITHOUT links"* and the memory counter wrapped.

```C
char *bigger = gbmem_realloc(list, new_size);
if(!bigger) {
    // Error already logged: `list` is unchanged, still valid and still yours
    return -1;
}
list = bigger;
```

---

(gbmem_set_allocators)=
## [`gbmem_set_allocators()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L139)

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
## [`gbmem_setup()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L103)

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
| `mem_max_system_memory` | `size_t` | Maximum total memory the allocator can use. Default is 64 MB. |
| `use_own_system_memory` | `BOOL` | Reserved for future use. |
| `mem_min_block` | `size_t` | Minimum block size. Default is 512 bytes. |
| `mem_superblock` | `size_t` | Superblock size. Default is 16 MB. |

**Returns**

0 on success.

**Notes**

Must be called before any other `gbmem_*` allocation function. Typically called once during yuno initialization.

---

(gbmem_shutdown)=
## [`gbmem_shutdown()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L131)

Stops the memory manager and cleans up resources.

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

Must be called during yuno shutdown. After this call, no further `gbmem_*` allocation functions must be used.

---

(gbmem_strdup)=
## [`gbmem_strdup()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L275)

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
## [`gbmem_strndup()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L239)

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
## [`get_cur_system_memory()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L302)

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
## [`get_max_system_memory()`](https://github.com/artgins/yunetas/blob/7.28.7/kernel/c/gobj-c/src/gbmem.c#L294)

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

