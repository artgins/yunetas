(persistent_attrs)=
# **Persistent Attributes**

Persistent attributes are attributes defined in a GClass that retain their values across restarts or state transitions. They are automatically loaded during Yuno startup and can be explicitly saved as needed. Persistent attributes ensure that critical configuration data and operational settings are preserved, enabling seamless continuity in operations.

---

## Characteristics of Persistent Attributes

### 1. **Automatic Loading**
When a Yuno starts, persistent attributes are automatically loaded. If no custom loading function is provided, the default behavior loads these attributes from a JSON file.

### 2. **Explicit Saving**
Persistent attributes are saved explicitly using [`gobj_save_persistent_attrs()`](gobj_save_persistent_attrs()). 
The function allows:
- Saving All Attributes: By passing a null or empty `jn_attrs`, all persistent attributes are saved.
- Selective Saving: By passing a list or dictionary in `jn_attrs`, only the specified attributes are saved.

### 3. **Access Control**
Attributes marked with [`SDF_RD`](SDF_RD) are accessible from other Yunos, while attributes without this flag remain private to the Yuno.

### 4. **Default Values**
Attributes with no explicitly provided value use their default values as defined in the GClass schema ([`attrs_table`](attrs_table)).

---

## Customizing Save and Load Behavior

The behavior for saving and loading persistent attributes can be customized by passing a [`persistent_attrs_t`](persistent_attrs_t) structure to [`gobj_start_up()`](gobj_start_up()). This structure includes pointers to custom save and load functions. If this argument is null, the default implementation is used, which stores attributes in a JSON file.

::: {list-table}
:widths: 30 70
:header-rows: 1

* - **Scenario**
  - **Description**

* - **Custom Behavior**
  - Provide custom functions in [`persistent_attrs_t`](persistent_attrs_t) for saving/loading attributes.

* - **Default Behavior**
  - If no custom functions are provided, attributes are stored in a JSON file.

:::

---

## Flags and Their Roles

::: {list-table}
:widths: 30 70
:header-rows: 1

* - **Flag**
  - **Description**

* - `SDF_PERSIST`
  - Marks the attribute as persistent, enabling it to be saved and loaded.

* - `SDF_RD`
  - Makes the attribute readable by other Yunos.

* - `SDF_WR`
  - Indicates that the attribute is writable (modifiable during runtime).
:::

---

## API for Managing Persistent Attributes

### Saving Attributes
- Function: [`gobj_save_persistent_attrs(hgobj gobj, json_t *jn_attrs)`](gobj_save_persistent_attrs())
- Behavior:
    - Save all attributes by passing null or an empty `jn_attrs`.
    - Save specific attributes by specifying their names in `jn_attrs`.

### Loading Attributes
- Default: Attributes are automatically loaded during Yuno startup using the default or custom implementation provided in [`persistent_attrs_t`](persistent_attrs_t).


---

## Workflow for Persistent Attributes

1. **Definition:**
   Define attributes in the GClass schema ([`attrs_table`](attrs_table)) with the [`SDF_PERSIST`](SDF_PERSIST) flag.

2. **Custom Save/Load:**
   Optionally, provide custom save/load functions in [`persistent_attrs_t`](persistent_attrs_t) when initializing the Yuno with [`gobj_start_up()`](gobj_start_up()).

3. **Loading:**
   Persistent attributes are automatically loaded during startup using either the default or custom implementation.

4. **Saving:**
   Explicitly save attributes with [`gobj_save_persistent_attrs()`](gobj_save_persistent_attrs()) as needed.

---

## Benefits of Persistent Attributes

- State Retention: Automatically preserve important data across restarts.
- Flexibility: Customize save/load behavior using [`persistent_attrs_t`](persistent_attrs_t).
- Selective Saving: Save only the necessary attributes when needed.
- Scalability: Manage attributes across multiple Yunos with controlled access ([`SDF_RD`](SDF_RD)).

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

**Persistent Attributes Store Functions**
- [`startup_persistent_attrs()`](startup_persistent_attrs_fn): Initializes persistent attributes function.
- [`end_persistent_attrs()`](end_persistent_attrs_fn): Cleans up persistent attributes function.
- [`load_persistent_attrs()`](load_persistent_attrs_fn): Loads persistent attributes from storage function.
- [`save_persistent_attrs()`](save_persistent_attrs_fn): Saves persistent attributes to storage function.
- [`remove_persistent_attrs()`](remove_persistent_attrs_fn): Deletes persistent attributes function.
- [`list_persistent_attrs()`](list_persistent_attrs_fn): Lists all persistent attributes function.


### Prototypes of functions to manage persistent attributes

(startup_persistent_attrs_fn)=
#### **`startup_persistent_attrs()`**

```C
typedef int (*startup_persistent_attrs_fn)(void);
```

(end_persistent_attrs_fn)=
#### **`end_persistent_attrs()`**

```C
typedef void (*end_persistent_attrs_fn)(void);
```

(load_persistent_attrs_fn)=
#### **`load_persistent_attrs()`**

```C
typedef int (*load_persistent_attrs_fn)(
    hgobj gobj,
    json_t *keys  // owned, if null load all persistent attrs, else, load
);
```

(save_persistent_attrs_fn)=
#### **`(save_persistent_attrs)`**

```C
typedef int (*save_persistent_attrs_fn)(
    hgobj gobj,
    json_t *keys  // owned
);
```

(remove_persistent_attrs_fn)=
#### **`(remove_persistent_attrs)`**

```C
typedef int (*remove_persistent_attrs_fn)(
    hgobj gobj,
    json_t *keys  // owned
);
```

(list_persistent_attrs_fn)=
#### **`(list_persistent_attrs)`**

```C
typedef json_t * (*list_persistent_attrs_fn)(
    hgobj gobj,
    json_t *keys  // owned
);
```
