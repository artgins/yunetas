# DB Simple

Simple file-based JSON database for persistent GObj attributes.

**Source:** `kernel/c/root-linux/src/dbsimple.h`

---

(db_load_persistent_attrs)=
## `db_load_persistent_attrs()`

Loads writable persistent attributes from the file-based store and
updates the gobj's attributes.

```C
int db_load_persistent_attrs(
    hgobj gobj,
    json_t *keys   // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject whose attributes are loaded. |
| `keys` | `json_t *` | JSON object or array of attribute names to load (owned). |

**Returns**

`0` on success.

---

(db_save_persistent_attrs)=
## `db_save_persistent_attrs()`

Saves writable persistent attributes to the file-based store.

```C
int db_save_persistent_attrs(
    hgobj gobj,
    json_t *keys   // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject whose attributes are saved. |
| `keys` | `json_t *` | JSON object or array of attribute names to save (owned). |

**Returns**

`0` on success.

---

(db_remove_persistent_attrs)=
## `db_remove_persistent_attrs()`

Removes persistent attributes from the file-based store.

```C
int db_remove_persistent_attrs(
    hgobj gobj,
    json_t *keys   // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject whose attributes are removed. |
| `keys` | `json_t *` | JSON object or array of attribute names to remove (owned). |

**Returns**

`0` on success.

---

(db_list_persistent_attrs)=
## `db_list_persistent_attrs()`

Lists persistent attributes stored for a gobj.

```C
json_t *db_list_persistent_attrs(
    hgobj gobj,
    json_t *keys   // owned
);
```

**Parameters**

| Key | Type | Description |
|---|---|---|
| `gobj` | `hgobj` | GObject to query. |
| `keys` | `json_t *` | JSON object or array of attribute names to list (owned). |

**Returns**

JSON object containing the requested attributes (owned by caller — must be freed).
