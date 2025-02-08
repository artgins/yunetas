# `trq_load_all()`

**Description:**
Loads all messages within a given range of row IDs.

**Prototype:**
```c
PUBLIC int trq_load_all(tr_queue trq_, const char *key, int64_t from_rowid, int64_t to_rowid);
```

**Parameters:**
- `trq_` (`tr_queue`): The queue instance.
- `key` (`const char *`): The key filter (optional).
- `from_rowid` (`int64_t`): The starting row ID.
- `to_rowid` (`int64_t`): The ending row ID.

**Returns:**
- `int`: `0` on success, `-1` on error.
