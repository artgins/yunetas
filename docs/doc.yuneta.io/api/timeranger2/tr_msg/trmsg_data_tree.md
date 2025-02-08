# `trmsg_data_tree()`

**Prototype:**
```c
PUBLIC json_t *trmsg_data_tree(json_t *tranger,
    const char *topic_name,
    const char *from_date,
    const char *to_date);
```

**Description:**
Retrieves a structured data tree from a specified topic in the TRanger instance within a date range.

**Parameters:**
- `tranger` (`json_t *`): The TRanger instance.
- `topic_name` (`const char *`): The topic from which to retrieve data.
- `from_date` (`const char *`): Start date filter (optional, can be NULL).
- `to_date` (`const char *`): End date filter (optional, can be NULL).

**Returns:**
- `json_t *`: JSON representation of the retrieved data tree.

---
