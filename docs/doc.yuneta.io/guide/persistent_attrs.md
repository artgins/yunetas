(persistent_attrs_t)=
# **Persistent Attributes**

Persistent attributes are attributes defined in a GClass that retain their values across restarts or state transitions. They are automatically loaded during Yuno startup and can be explicitly saved as needed. Persistent attributes ensure that critical configuration data and operational settings are preserved, enabling seamless continuity in operations.

---

## Characteristics of Persistent Attributes

### 1. **Automatic Loading**
When a Yuno starts, persistent attributes are automatically loaded. If no custom loading function is provided, the default behavior loads these attributes from a JSON file.

### 2. **Explicit Saving**
Persistent attributes are saved explicitly using `gobj_save_persistent_attrs()`. The function allows:
-Saving All Attributes:** By passing a null or empty `jn_attrs`, all persistent attributes are saved.
-Selective Saving:** By passing a list or dictionary in `jn_attrs`, only the specified attributes are saved.

### 3. **Access Control**
Attributes marked with `SDF_RD` are accessible from other Yunos, while attributes without this flag remain private to the Yuno.

### 4. **Default Values**
Attributes with no explicitly provided value use their default values as defined in the GClass schema (`tattr_desc`).

---

## Customizing Save and Load Behavior

The behavior for saving and loading persistent attributes can be customized by passing a `persistent_attrs_t` structure to `gobj_start_up()`. This structure includes pointers to custom save and load functions. If this argument is null, the default implementation is used, which stores attributes in a JSON file.

| **Scenario**        | **Description**                                                               |
|----------------------|-------------------------------------------------------------------------------|
| **Custom Behavior**  | Provide custom functions in `persistent_attrs_t` for saving/loading attributes. |
| **Default Behavior** | If no custom functions are provided, attributes are stored in a JSON file.    |

---

## Flags and Their Roles

| **Flag**        | **Description**                                                                 |
|------------------|---------------------------------------------------------------------------------|
| `SDF_PERSIST`    | Marks the attribute as persistent, enabling it to be saved and loaded.          |
| `SDF_RD`         | Makes the attribute readable by other Yunos.                                   |
| `SDF_WR`         | Indicates that the attribute is writable (modifiable during runtime).           |

---

## API for Managing Persistent Attributes

### Saving Attributes
-Function:** `gobj_save_persistent_attrs(hgobj gobj, json_t *jn_attrs)`
-Behavior:**
    - Save all attributes by passing null or an empty `jn_attrs`.
    - Save specific attributes by specifying their names in `jn_attrs`.

### Loading Attributes
-Default:** Attributes are automatically loaded during Yuno startup using the default or custom implementation provided in `persistent_attrs_t`.

---

## Workflow for Persistent Attributes

1. **Definition:**
   Define attributes in the GClass schema (`tattr_desc`) with the `SDF_PERSIST` flag.

2. **Custom Save/Load:**
   Optionally, provide custom save/load functions in `persistent_attrs_t` when initializing the Yuno with `gobj_start_up()`.

3. **Loading:**
   Persistent attributes are automatically loaded during startup using either the default or custom implementation.

4. **Saving:**
   Explicitly save attributes with `gobj_save_persistent_attrs()` as needed.

---

## Use Cases

### 1. **Configuration Settings**
Attributes such as `timeout`, `hostname`, or `bind_ip` ensure consistent application behavior across restarts.

### 2. **Operational State**
Values like `deep_trace` or `autoplay` can be saved to maintain runtime states.

### 3. **Custom Storage**
Custom save/load functions allow storing attributes in external systems such as databases or specialized storage solutions.

---

## Benefits of Persistent Attributes

-State Retention:** Automatically preserve important data across restarts.
-Flexibility:** Customize save/load behavior using `persistent_attrs_t`.
-Selective Saving:** Save only the necessary attributes when needed.
-Scalability:** Manage attributes across multiple Yunos with controlled access (`SDF_RD`).

(persistent_attrs_t)=
## persistent_attrs_t
The `persistent_attrs_t` structure contains function pointers for managing persistent attributes of a Yuno instance. This allows the user to handle attributes in storage or memory during the lifecycle of a Yuno.

```c
typedef struct {
    startup_persistent_attrs_fn  startup_persistent_attrs;
    end_persistent_attrs_fn      end_persistent_attrs;
    load_persistent_attrs_fn     load_persistent_attrs;
    save_persistent_attrs_fn     save_persistent_attrs;
    remove_persistent_attrs_fn   remove_persistent_attrs;
    list_persistent_attrs_fn     list_persistent_attrs;
} persistent_attrs_t;
```

**Fields**
-`startup_persistent_attrs`: Initializes persistent attributes function.
-`end_persistent_attrs`: Cleans up persistent attributes function.
-`load_persistent_attrs`: Loads persistent attributes from storage function.
-`save_persistent_attrs`: Saves persistent attributes to storage function.
-`remove_persistent_attrs`: Deletes persistent attributes function.
-`list_persistent_attrs`: Lists all persistent attributes function.
