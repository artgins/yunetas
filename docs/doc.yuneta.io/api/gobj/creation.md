# Creation

Create, destroy, and navigate gobj instances. Every gobj belongs to a parent and a GClass, and lives inside the hierarchical tree that forms a yuno.

Source code:

- [`gobj.h`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.h)
- [`gobj.c`](https://github.com/artgins/yunetas/blob/main/kernel/c/gobj-c/src/gobj.c)

(gobj_create)=
## `gobj_create()`

Creates a new `gobj` (generic object) instance of the specified `gclass` and assigns it to a parent `gobj` if provided.

```C
hgobj gobj_create(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw, // owned
    hgobj           parent
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the `gobj` to be created. It is case-insensitive and converted to lowercase. |
| `gclass_name` | `gclass_name_t` | The name of the `gclass` to which the `gobj` belongs. |
| `kw` | `json_t *` | A JSON object containing configuration parameters for the `gobj`. The ownership of this object is transferred to the function. |
| `parent` | `hgobj` | The parent `gobj` to which the new `gobj` will be attached. If `NULL`, the `gobj` is created without a parent. |

**Returns**

Returns a handle to the newly created `gobj` (`hgobj`). Returns `NULL` if the creation fails due to invalid parameters or memory allocation failure.

**Notes**

['The function internally calls [`gobj_create2()`](#gobj_create2) with default flags.', 'If `gobj_name` is longer than 15 characters on ESP32, the function will abort execution.', 'If `gclass_name` is empty or not found, an error is logged and `NULL` is returned.', "The function ensures that `gobj_name` does not contain invalid characters such as '`' or '^'.", 'If `parent` is `NULL`, an error is logged unless the `gobj` is a Yuno instance.']

---

(gobj_create2)=
## `gobj_create2()`

Creates a new `gobj` (generic object) with the specified name, class, attributes, and parent. The function initializes the object, sets its attributes, and registers it if it is a service.

```C
hgobj gobj_create2(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw,    // owned
    hgobj           parent,
    gobj_flag_t     gobj_flag
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the `gobj` to be created. It is case-insensitive and converted to lowercase. |
| `gclass_name` | [`gclass_name_t`](#gclass_name_t) | The name of the `gclass` to which the `gobj` belongs. |
| `kw` | `json_t *` | A JSON object containing the attributes and configuration for the `gobj`. The function takes ownership of this parameter. |
| `parent` | `hgobj` | The parent `gobj` under which the new `gobj` will be created. Must be non-null unless the `gobj` is a Yuno. |
| `gobj_flag` | [`gobj_flag_t`](#gobj_flag_t) | Flags that define the behavior of the `gobj`, such as whether it is a service, volatile, or a pure child. |

**Returns**

Returns a handle to the newly created `gobj` (`hgobj`). 
Returns `NULL` if creation fails due to invalid parameters or memory allocation failure.

**Notes**

If the `gobj` is marked as a service, it is registered globally.
If the `gobj` is a Yuno, it is stored as the global Yuno instance.

The function checks for required attributes and applies global configuration variables.
If the `gobj` has a [`mt_create2`](#mt_create2) method, it is called with the provided attributes.

If the `gobj` has a parent, it is added to the parent's child list.

---

(gobj_create_default_service)=
## `gobj_create_default_service()`

The function `gobj_create_default_service()` creates a default service gobj with autostart enabled but autoplay disabled. The service will be played by the yuno's play method.

```C
hgobj gobj_create_default_service(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw, // owned
    hgobj           parent
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the gobj to be created. |
| `gclass_name` | `gclass_name_t` | The name of the gclass to which the gobj belongs. |
| `kw` | `json_t *` | A JSON object containing the configuration parameters for the gobj. The ownership of this object is transferred to the function. |
| `parent` | `hgobj` | The parent gobj under which the new gobj will be created. |

**Returns**

Returns a handle to the newly created gobj, or NULL if the creation fails.

**Notes**

The created gobj is marked as a default service and will be started automatically but not played until explicitly triggered by the yuno's play method.

---

(gobj_create_pure_child)=
## `gobj_create_pure_child()`

Creates a new `gobj` as a pure child of the specified parent. A pure child sends events directly to its parent instead of publishing them.

```C
hgobj gobj_create_pure_child(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw,    // owned
    hgobj           parent
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the new `gobj`. |
| `gclass_name` | `gclass_name_t` | The `gclass` type of the new `gobj`. |
| `kw` | `json_t *` | A JSON object containing the configuration attributes for the new `gobj`. This parameter is owned and will be consumed by the function. |
| `parent` | `hgobj` | The parent `gobj` to which the new `gobj` will be attached. |

**Returns**

Returns a handle to the newly created `gobj`, or `NULL` if creation fails.

**Notes**

A pure child does not publish events but instead sends them directly to its parent. This function internally calls [`gobj_create2()`](#gobj_create2) with the `gobj_flag_pure_child` flag.

---

(gobj_create_service)=
## `gobj_create_service()`

The `gobj_create_service()` function creates a new service GObj instance with the specified name, gclass, and configuration, and assigns it to the given parent GObj.

```C
hgobj gobj_create_service(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw,  // owned
    hgobj parent
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the GObj instance to be created. |
| `gclass_name` | `gclass_name_t` | The name of the GClass to which the new GObj belongs. |
| `kw` | `json_t *` | A JSON object containing the configuration parameters for the new GObj. This parameter is owned and will be consumed by the function. |
| `parent` | `hgobj` | The parent GObj under which the new service GObj will be created. |

**Returns**

Returns a handle to the newly created service GObj (`hgobj`). If the creation fails, it returns `NULL`.

**Notes**

The created GObj is marked as a service using the `gobj_flag_service` flag. If the `gclass_name` is not found, an error is logged, and the function returns `NULL`. The function internally calls [`gobj_create2()`](#gobj_create2) with the appropriate flags.

---

(gobj_create_tree)=
## `gobj_create_tree()`

Creates a hierarchical tree of `gobj` instances from a JSON configuration string. The function parses the configuration, applies global settings, and instantiates the objects accordingly.

```C
hgobj gobj_create_tree(
    hgobj parent,
    const char *tree_config,
    json_t *json_config_variables
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `parent` | `hgobj` | The parent `gobj` under which the tree will be created. |
| `tree_config` | `const char *` | A JSON string defining the structure and attributes of the `gobj` tree. |
| `json_config_variables` | `json_t *` | A JSON object (owned) containing configuration variables to be applied to the tree. |

**Returns**

Returns the root `gobj` of the created tree, or `NULL` if an error occurs.

**Notes**

This function internally calls `gobj_create_tree0()` after parsing and applying configuration variables.

---

(gobj_create_volatil)=
## `gobj_create_volatil()`

Creates a new volatile `gobj` instance of the specified `gclass_name` with the given attributes and parent. A volatile `gobj` is automatically destroyed when its parent is destroyed.

```C
hgobj gobj_create_volatil(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw, // owned
    hgobj           parent
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the `gobj` instance. |
| `gclass_name` | `gclass_name_t` | The name of the `gclass` to instantiate. |
| `kw` | `json_t *` | A JSON object containing the attributes for the `gobj`. The ownership of this object is transferred to the function. |
| `parent` | `hgobj` | The parent `gobj` to which the new `gobj` will be attached. |

**Returns**

Returns a handle to the newly created volatile `gobj`, or `NULL` if creation fails.

**Notes**

A volatile `gobj` is automatically destroyed when its parent is destroyed. Use [`gobj_create2()`](#gobj_create2) for more control over `gobj` creation.

---

(gobj_create_yuno)=
## `gobj_create_yuno()`

Creates a new Yuno object with the specified name and gclass. The Yuno object serves as the root object in the hierarchical structure of gobjs.

```C
hgobj gobj_create_yuno(
    const char      *gobj_name,
    gclass_name_t   gclass_name,
    json_t          *kw // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj_name` | `const char *` | The name of the Yuno object. |
| `gclass_name` | `gclass_name_t` | The name of the gclass to associate with the Yuno object. |
| `kw` | `json_t *` | A JSON object containing configuration parameters for the Yuno object. The ownership of this parameter is transferred to the function. |

**Returns**

Returns a handle to the newly created Yuno object, or NULL if creation fails.

**Notes**

The Yuno object is the top-level object in the gobj hierarchy and must be unique within the system.

---

(gobj_destroy)=
## `gobj_destroy()`

The `gobj_destroy()` function deallocates and removes a given `hgobj` instance, ensuring proper cleanup of its resources, subscriptions, and child objects.

```C
void gobj_destroy(
    hgobj   gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | A handle to the `hgobj` instance to be destroyed. |

**Returns**

This function does not return a value.

**Notes**

['If the `hgobj` instance is currently playing, it will be paused before destruction.', 'If the `hgobj` instance is running, it will be stopped before destruction.', 'All child objects of the `hgobj` instance will be recursively destroyed.', 'All event subscriptions related to the `hgobj` instance will be removed.', 'If the `hgobj` instance is a registered service, it will be deregistered before destruction.']

---

(gobj_destroy_children)=
## `gobj_destroy_children()`

Destroys all child objects of the given `hgobj`, ensuring proper cleanup and deallocation.

```C
void gobj_destroy_children(
    hgobj   gobj
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | The parent object whose child objects will be destroyed. |

**Returns**

This function does not return a value.

**Notes**

Each child object is destroyed using [`gobj_destroy()`](#gobj_destroy). If a child is already destroyed or in the process of being destroyed, an error is logged.

---

(gobj_service_factory)=
## `gobj_service_factory()`

Creates a service gobj using `gobj_create_tree0()` and registers it as a service.

```C
hgobj gobj_service_factory(
    const char  *name,
    json_t      *jn_service_config // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `name` | `const char *` | The name of the service to be created. |
| `jn_service_config` | `json_t *` | A JSON object containing the service configuration. This parameter is owned and will be consumed by the function. |

**Returns**

Returns a handle to the created service gobj, or `NULL` if the creation fails.

**Notes**

The function extracts global settings, applies configuration variables, and invokes `gobj_create_tree0()` to instantiate the service.

---

(gobj_destroy_named_children)=
## `gobj_destroy_named_children()`

*Description pending — signature extracted from header.*

```C
int gobj_destroy_named_children(
    hgobj gobj,
    const char *name
);
```

---

(gobj_sdata_create)=
## `gobj_sdata_create()`

*Description pending — signature extracted from header.*

```C
json_t *gobj_sdata_create(
    hgobj gobj,
    const sdata_desc_t* schema
);
```

---

