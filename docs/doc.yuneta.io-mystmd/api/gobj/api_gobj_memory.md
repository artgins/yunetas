# Memory Functions

Source code in:
- [gobj.c](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)
- [gobj.h](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)

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

*Description pending — signature extracted from header.*

```C
void *gbmem_calloc(
    size_t n,
    size_t size
);
```

---

(gbmem_free)=
## `gbmem_free()`

*Description pending — signature extracted from header.*

```C
void gbmem_free(
    void *ptr
);
```

---

(gbmem_get_allocators)=
## `gbmem_get_allocators()`

*Description pending — signature extracted from header.*

```C
int gbmem_get_allocators(
    sys_malloc_fn_t *malloc_func,
    sys_realloc_fn_t *realloc_func,
    sys_calloc_fn_t *calloc_func,
    sys_free_fn_t *free_func
);
```

---

(gbmem_get_maximum_block)=
## `gbmem_get_maximum_block()`

*Description pending — signature extracted from header.*

```C
size_t gbmem_get_maximum_block(void);
```

---

(gbmem_malloc)=
## `gbmem_malloc()`

*Description pending — signature extracted from header.*

```C
void *gbmem_malloc(
    size_t size
);
```

---

(gbmem_realloc)=
## `gbmem_realloc()`

*Description pending — signature extracted from header.*

```C
void *gbmem_realloc(
    void *ptr,
    size_t size
);
```

---

(gbmem_set_allocators)=
## `gbmem_set_allocators()`

*Description pending — signature extracted from header.*

```C
int gbmem_set_allocators(
    sys_malloc_fn_t malloc_func,
    sys_realloc_fn_t realloc_func,
    sys_calloc_fn_t calloc_func,
    sys_free_fn_t free_func
);
```

---

(gbmem_setup)=
## `gbmem_setup()`

*Description pending — signature extracted from header.*

```C
int gbmem_setup(
    size_t mem_max_block,
    size_t mem_max_system_memory,
    BOOL use_own_system_memory,
    size_t mem_min_block,
    size_t mem_superblock
);
```

---

(gbmem_shutdown)=
## `gbmem_shutdown()`

*Description pending — signature extracted from header.*

```C
void gbmem_shutdown(void);
```

---

(gbmem_strdup)=
## `gbmem_strdup()`

*Description pending — signature extracted from header.*

```C
char *gbmem_strdup(
    const char *str
);
```

---

(gbmem_strndup)=
## `gbmem_strndup()`

*Description pending — signature extracted from header.*

```C
char *gbmem_strndup(
    const char *str,
    size_t size
);
```

---

(get_cur_system_memory)=
## `get_cur_system_memory()`

*Description pending — signature extracted from header.*

```C
size_t get_cur_system_memory(void);
```

---

(get_max_system_memory)=
## `get_max_system_memory()`

*Description pending — signature extracted from header.*

```C
size_t get_max_system_memory(void);
```

---

