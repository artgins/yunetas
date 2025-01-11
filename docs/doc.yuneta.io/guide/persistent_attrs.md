(Persistent Attributes)=
# Persistent Attributes

## persistent_attrs_t
The `persistent_attrs_t` structure contains function pointers for managing persistent attributes of a Yuno instance. This allows the user to handle attributes in storage or memory during the lifecycle of a Yuno.

```c
typedef struct {
    startup_persistent_attrs_t  startup_persistent_attrs;
    end_persistent_attrs_t      end_persistent_attrs;
    load_persistent_attrs_t     load_persistent_attrs;
    save_persistent_attrs_t     save_persistent_attrs;
    remove_persistent_attrs_t   remove_persistent_attrs;
    list_persistent_attrs_t     list_persistent_attrs;
} persistent_attrs_t;
```

**Fields**
- **`startup_persistent_attrs`**: Initializes persistent attributes.
- **`end_persistent_attrs`**: Cleans up persistent attributes.
- **`load_persistent_attrs`**: Loads persistent attributes from storage.
- **`save_persistent_attrs`**: Saves persistent attributes to storage.
- **`remove_persistent_attrs`**: Deletes persistent attributes.
- **`list_persistent_attrs`**: Lists all persistent attributes.
