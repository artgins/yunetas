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

**Whose file is read.** The yuno's own, root's (the yuno run once as root),
and -- for a yuno run as root -- the one of the yuno's user: the owner of the
lowest directory of the chain that is closed from `/` down -- every directory
of it nobody else can write, each one owned by root or by that user
(`/yuneta/realms`, 0755, on a node). The chain is walked down with
`openat(O_NOFOLLOW)` and ends at the first directory others can write (the
realm's, 02775) or of a third user: what is below can be renamed away and
replaced by a member of the group, so it names nobody. A symlink met inside
the chain is followed, since only root or the chain's user can have put it
there, and its target is walked from `/` under the same rules, the user named
so far still holding (at most 40 links); a `..` is the parent of the
directories walked. A node laid out as

```text
/yuneta -> /srv/yuneta        root's link in root's "/"
/srv            root  0755
/srv/yuneta     yuneta 0755   <- the yuno's user
.../realms/<owner>/<realm>/<yuno>/data   02775: the chain ends above it
```

trusts the files of `yuneta` for a root yuno; a link planted under the 02775
directory is below the chain and names nobody. A file of another user that
the group or others can write is refused as well: it can have been edited
there. Refused, it is not loaded and the saves are refused, with an ERROR;
the operator gives it to the yuno's user or makes it 0600:

```bash
stat -c '%a %U' /yuneta/realms/<owner>/<realm>/<yuno>/data/*-persistent-attrs.json
sudo chown yuneta: <file> && sudo chmod 0600 <file>
```

Up to 7.25.21 the yuno's user was the data directory's owner, and a trusted
file was read whatever its mode. In 7.25.22 it was the owner of the first
closed directory found going UP from the file: a closed data directory
planted under its 02775 parent (or a symlink to one) named its maker, whose
file was loaded and given the next save. The first form of the walk down
stopped at any symlink: under a linked `/yuneta` a root yuno refused its own
files.

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
goes on without the reservation, logged); a shorter content is padded with
blanks and the file is cut only once it is on disk. The old content is read
before the write: a write that stops half way -- an `ENOSPC` where the room
could not be reserved, or on a copy-on-write filesystem (btrfs, reflinked
XFS) whose reservation does not cover the rewrite -- writes it back, and the
ERROR says *"written back as it was"*. A crash from the write until its
first `fsync()` returns, or a write back that fails too (the ERROR then says
*"UNPARSABLE"*), can leave a file that cannot be parsed: "never truncated"
holds for the rename, not for this path, and such a file refuses the next
saves as above. Up to 7.25.21 one `pwrite()` was made, and a short one left
the new start over the old tail.

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
