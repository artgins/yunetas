# Memory Functions

Source code in:
- [gobj.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)
- [gobj.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)

(gobj_calloc_func)=
## `gobj_calloc_func()`

Returns the current system memory allocation function for zero-initialized memory blocks.

```C
sys_calloc_fn_t gobj_calloc_func(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A function pointer of type `sys_calloc_fn_t` that points to the current memory allocation function used for zero-initialized memory blocks.

**Notes**

This function allows retrieval of the memory allocation function used internally for zero-initialized memory blocks. The returned function can be used to allocate memory in a manner consistent with the system's memory management strategy.

---

(gobj_free_func)=
## `gobj_free_func()`

Returns the function pointer for the system's memory deallocation function.

```C
sys_free_fn_t gobj_free_func(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A function pointer of type `sys_free_fn_t` that can be used to free allocated memory.

**Notes**

This function provides access to the currently set memory deallocation function, which may be customized using [`gobj_set_allocators()`](#gobj_set_allocators).

---

(gobj_get_allocators)=
## `gobj_get_allocators()`

Retrieves the current memory allocation function pointers used by the system.

```C
void gobj_get_allocators(
    sys_malloc_fn_t *malloc_func,
    sys_realloc_fn_t *realloc_func,
    sys_calloc_fn_t *calloc_func,
    sys_free_fn_t *free_func
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `malloc_func` | `sys_malloc_fn_t *` | Pointer to store the current `malloc` function. |
| `realloc_func` | `sys_realloc_fn_t *` | Pointer to store the current `realloc` function. |
| `calloc_func` | `sys_calloc_fn_t *` | Pointer to store the current `calloc` function. |
| `free_func` | `sys_free_fn_t *` | Pointer to store the current `free` function. |

**Returns**

None.

**Notes**

This function allows retrieving the function pointers for memory allocation routines, which can be useful for debugging or replacing the default memory management functions.

---

(gobj_get_maximum_block)=
## `gobj_get_maximum_block()`

Returns the maximum memory block size that can be allocated by the system.

```C
size_t gobj_get_maximum_block(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

The maximum allocatable memory block size in bytes.

**Notes**

This function provides the upper limit for memory allocation requests.

---

(gobj_malloc_func)=
## `gobj_malloc_func()`

`gobj_malloc_func()` returns the current memory allocation function used by the system.

```C
PUBLIC sys_malloc_fn_t gobj_malloc_func(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

Returns a function pointer of type `sys_malloc_fn_t`, which represents the memory allocation function currently in use.

**Notes**

This function allows retrieval of the memory allocator function, which can be customized using [`gobj_set_allocators()`](#gobj_set_allocators).

---

(gobj_realloc_func)=
## `gobj_realloc_func()`

Returns the function pointer for memory reallocation used by the system.

```C
sys_realloc_fn_t gobj_realloc_func(void);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `-` | `-` | This function does not take any parameters. |

**Returns**

A function pointer of type `sys_realloc_fn_t` that points to the system's memory reallocation function.

**Notes**

['The returned function pointer can be used to reallocate memory dynamically.', 'By default, it points to the internal `_mem_realloc()` function unless overridden by `gobj_set_allocators()`.']

---

(gobj_set_allocators)=
## `gobj_set_allocators()`

Sets custom memory allocation functions for the system, replacing the default allocators.

```C
int gobj_set_allocators(
    sys_malloc_fn_t  malloc_func,
    sys_realloc_fn_t realloc_func,
    sys_calloc_fn_t  calloc_func,
    sys_free_fn_t    free_func
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `malloc_func` | `sys_malloc_fn_t` | Function pointer for memory allocation. |
| `realloc_func` | `sys_realloc_fn_t` | Function pointer for memory reallocation. |
| `calloc_func` | `sys_calloc_fn_t` | Function pointer for memory allocation with zero initialization. |
| `free_func` | `sys_free_fn_t` | Function pointer for memory deallocation. |

**Returns**

Returns 0 on success.

**Notes**

This function allows replacing the default memory management functions with custom implementations.

---

(gobj_strdup)=
## `gobj_strdup()`

`gobj_strdup()` duplicates a given string by allocating memory for a new copy and returning a pointer to it.

```C
char *gobj_strdup(
    const char *string
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `string` | `const char *` | The null-terminated string to duplicate. |

**Returns**

A pointer to the newly allocated duplicate string, or `NULL` if memory allocation fails.

**Notes**

The returned string must be freed using `gobj_free_func()` to avoid memory leaks.

---

(gobj_strndup)=
## `gobj_strndup()`

`gobj_strndup()` duplicates a substring of a given string up to a specified length, ensuring memory allocation for the new string.

```C
char *gobj_strndup(
    const char *str,
    size_t      size
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `str` | `const char *` | The input string to duplicate. |
| `size` | `size_t` | The maximum number of characters to duplicate from `str`. |

**Returns**

A newly allocated string containing up to `size` characters from `str`, or `NULL` if memory allocation fails.

**Notes**

The returned string must be freed using `gobj_free_func()` to avoid memory leaks.

---

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
