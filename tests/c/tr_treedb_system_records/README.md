# test_tr_treedb_system_records

Regression coverage for the treedb **system-protection marks**:

- **Record-level `__system__` field** (a regular persistent boolean col):
  - `treedb_delete_node()` refuses the delete; `force` does **not** override
    (unlike the snapshot-tag guard).
  - `treedb_delete_instance()` refuses the per-pkey2 delete; `force` does
    **not** override.
  - `treedb_update_node()` cannot clear the mark once TRUE (immutable;
    a warning is logged and the change ignored, other fields update normally).
  - A refused delete leaves the node indexed — no ref leak (the node pointer
    is borrowed from the index and only consumed on success).
- **Topic-level `system_topic` flag** (schema topic key, persisted in
  `topic_var.json`): `treedb_delete_topic()` refuses.
- Both marks **persist across a full close + reopen**.

Only a full store wipe (removing the `store/{database}/` tree) removes a
system record or topic.

The marks are stamped by internal seed loaders only (e.g. `C_AUTHZ`
`initial_load` via direct mt calls); every client-reachable input path in
`C_NODE` (`create-node` / `update-node` commands, `EV_TREEDB_UPDATE_NODE`)
strips `__system__` from the record.
