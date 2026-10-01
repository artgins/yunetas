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

`0` on success, `-1` (logged) when nothing was saved.

**Notes**

The file is `<realm>/<yuno>/data/<GCLASS>-<name>-persistent-attrs.json`, 0600:
a persistent attribute can be a secret (see
[`SDF_SECRET`](#SDF_SECRET) in [the SData guide](../../guide/guide_sdata.md)).
The attributes not in `keys` are kept from the file. A save writes a NEW file
in the same directory (`<file>.tmp-XXXXXX`, created 0600 with `O_EXCL`),
syncs it, `rename()`s it over the old one and syncs the directory: the old
file is never half-written, a symlink or a hard link in its place is
replaced, nothing is written through it, and a save that fails leaves the
old file as it was. A `<file>.tmp-XXXXXX` left by a save that did not end is
removed at the next load.

It is refused (`-1`, nothing written) when a file is there and cannot be read
(*"Persistent attrs NOT saved: the file there cannot be read, and its other
attrs would be lost"*): the operator removes it (its attributes go back to
their defaults) or repairs it. An EMPTY file holds no attributes and refuses
nothing. It is refused too when the file there is of another user (the yuno
run once as root): a save never takes over another user's file.

In a directory the yuno cannot write, the save goes in place, and only into a
file of the yuno's own, regular and of one name (logged *"Persistent attrs
saved in place"*). That path is safe against a full disk -- room is taken
before a byte is written, and a shorter content is padded with blanks before
the file is cut -- but not against a crash in the middle of the write itself:
the "never half-written" above holds for the rename, not for this path. A file
left that way cannot be parsed, and refuses the next saves as above.

**Example**

```C
gobj_write_str_attr(gobj, "password", "hunter2");
if(gobj_save_persistent_attrs(gobj, json_string("password")) < 0) {
    // Error already logged; the attr is set in memory, NOT on disk
}
```

---

(db_remove_persistent_attrs)=
## `db_remove_persistent_attrs()`

Removes persistent attributes from the file-based store: the file is written
again without them, as [`db_save_persistent_attrs()`](#db_save_persistent_attrs)
writes it, and refused the same way when the file there cannot be read.

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
