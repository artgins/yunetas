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
syncs it, `rename()`s it over the old one and syncs the directory: a symlink
or a hard link in its place is replaced, nothing is written through it, and
the old file is never truncated before the new one is complete -- a save that
fails leaves it as it was. A `<file>.tmp-XXXXXX` left by a save that did not
end is removed at the next load.

The new file is the yuno's user's. A file there of another user was loaded
first (its attributes are in the save): the save takes it over, logged at
INFO with the old owner -- and when the yuno runs as root, the new file is
given to the old owner (`fchown`), so root never takes the file from the
yuno. A file there that cannot be read refuses the save (`-1`, nothing
written: *"Persistent attrs NOT saved: the file there cannot be read, and
its other attrs would be lost"*): the operator removes it (its attributes go
back to their defaults) or repairs it. An empty file holds no attributes and
refuses nothing.

In a directory the yuno cannot write, the save goes IN PLACE, and only into
a file of the yuno's own, regular and of one name (logged *"Persistent attrs
saved in place"*). Room is reserved first without growing the file
(`fallocate(FALLOC_FL_KEEP_SIZE)`; a filesystem without it -- NFSv3, FUSE --
goes on without the reservation, logged), so a full disk leaves the old file
as it was; a shorter content is padded with blanks and the file is cut only
once it is on disk. A crash from the write until its first `fsync()` returns
can leave a file that cannot be parsed: "never truncated" holds for the
rename, not for this path, and such a file refuses the next saves as above.

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
