(dl_add)=
## `dl_add()`

Adds an item to the end of a doubly linked list `dl`.

```C
int dl_add(dl_list_t *dl, void *item);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `dl` | `dl_list_t *` | Pointer to the doubly linked list where the item will be added. |
| `item` | `void *` | Pointer to the item to be added to the list. |

**Returns**

Returns 0 on success, or -1 if the item already has links.

**Notes**

The function checks if the item already has links before adding it to the list.

---

(dl_delete)=
## `dl_delete()`

Removes a specified item from a doubly linked list and optionally frees its memory.

```C
int dl_delete(
    dl_list_t *dl,
    void *curr_,
    void (*fnfree)(void *)
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `dl` | `dl_list_t *` | Pointer to the doubly linked list from which the item will be removed. |
| `curr_` | `void *` | Pointer to the item to be removed from the list. |
| `fnfree` | `void (*)(void *)` | Optional function pointer to free the memory of the removed item. |

**Returns**

Returns 0 on success, or -1 if an error occurs (e.g., null item, empty list, or item not found).

**Notes**

The function ensures that the list remains consistent after deletion. If `fnfree` is provided, it is called to free the memory of the removed item.

---

(dl_find)=
## `dl_find()`

`dl_find()` searches for an item in a doubly linked list and returns a pointer to the item if found.

```C
void *dl_find(
    dl_list_t *dl,
    void *item
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `dl` | `dl_list_t *` | Pointer to the doubly linked list. |
| `item` | `void *` | Pointer to the item to search for in the list. |

**Returns**

Returns a pointer to the item if found, otherwise returns `NULL`.

**Notes**

This function performs a linear search through the list and returns the first matching item.

---

(dl_flush)=
## `dl_flush()`

Removes all items from the given double-linked list and optionally frees them using the provided function.

```C
void dl_flush(
    dl_list_t *dl,
    void (*fnfree)(void *)
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `dl` | `dl_list_t *` | Pointer to the double-linked list to be flushed. |
| `fnfree` | `void (*)(void *)` | Function pointer to a deallocation function for freeing each item, or NULL if no deallocation is needed. |

**Returns**

None.

**Notes**

Ensures that the list is completely emptied. If `fnfree` is provided, it is called for each item before removal.

---

(dl_init)=
## `dl_init()`

Initializes a double-linked list structure by setting its head, tail, and item count to zero.

```C
int dl_init(
    dl_list_t *dl,
    hgobj      gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `dl` | `dl_list_t *` | Pointer to the double-linked list structure to initialize. |
| `gobj` | `hgobj` | Pointer to the associated gobj, used for tracking purposes. |

**Returns**

Returns 0 on successful initialization.

**Notes**

This function ensures that the list is empty before initialization. If the list is not empty, an error message is logged.

---

(dl_insert)=
## `dl_insert()`

Inserts an item at the head of a doubly linked list `dl`.

```C
int dl_insert(
    dl_list_t *dl,
    void      *item
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `dl` | `dl_list_t *` | Pointer to the doubly linked list where the item will be inserted. |
| `item` | `void *` | Pointer to the item to be inserted at the head of the list. |

**Returns**

Returns 0 on success, or -1 if the item already has links and cannot be inserted.

**Notes**

The function checks if the item already has links before inserting it. If the list is empty, the item becomes both the head and tail.

---

(dl_delete_item)=
## `dl_delete_item()`

*Description pending — signature extracted from header.*

```C
int dl_delete_item(
    void *curr,
    void (*fnfree)(void *)
);
```

---

(dl_nfind)=
## `dl_nfind()`

*Description pending — signature extracted from header.*

```C
void * dl_nfind(
    dl_list_t *dl,
    size_t nitem
);
```

---

