# c_treedb_system_schema test

Tests the `__system__` meta-treedb that `C_TREEDB` keeps next to the treedbs it
manages: the place where a treedb schema is stored **as data** (topics
`treedbs` → `topics` → `cols`), which is what lets a schema be listed and
edited at runtime instead of only compiled into C.

The test drives `C_TREEDB` through its own commands and checks:

1. **Projection** — opening a treedb writes its schema into `__system__`: one
   `treedbs` node with its `schema_version`, one `topics` node per topic (with
   `system_topic` preserved), one `cols` node per column. Every node must be
   addressed by its **qualified** name — its parent's id, a dot, and its own —
   and carry the bare one in `value`: a name is unique only inside its parent,
   so a column stored without `value` is a column nobody can name, and one
   keyed by the bare name collides with the same column of another topic.
2. **Reconstruction** — the projection alone rebuilds the schema. The treedb is
   closed, its `<treedb_name>.treedb_schema.json` deleted so nothing on disk
   can supply the schema, and re-opened: the topics and columns that come back
   must be the ones that went in.

3. **Reconciliation** — a schema that moved forward (`schema_version` 1 → 2,
   one column added, another re-headered) updates the projection, and the
   columns that were already there **keep their id**: an update appends a
   version of the same node, a re-create would be a second column under the
   same name. The new column reaches the running treedb too, which is the
   whole chain: literal → `__system__` → schema file → topic.

4. **Fidelity** — every attribute a column MAY declare survives the round
   trip. The list is not written in the test: it is read from the descriptor
   a user column answers to, so an attribute added there without storage
   behind it fails here instead of disappearing from every schema in silence.
   That is how `enum`, `template` and `pkey2s` were being lost — a column kept
   its `enum` **flag** while its enumeration evaporated, so it declared an
   enumeration it no longer had, and every value passed.

5. **Refused writes** — a write that could not produce a working schema is
   refused where it is written, not at the next open: a column whose `type` is
   outside the enum the meta-schema declares (on create *and* on update, and
   the refused update leaves the node untouched), a change to the `pkey` of an
   existing topic, and a second column with a name the topic already has —
   which the qualified key refuses at the create, since same topic and same
   name means the same id, while the link still has to refuse a column born
   under another topic and hooked in here.

6. **The move to qualified ids** — a projection built by hand the way the old
   projector left it (numeric ids, the name in `value`) is re-projected, and
   what comes back is keyed by the qualified name with the legacy nodes gone.
   A column that is in **no** schema from C rides along: it is an operator's,
   the projector would never write it again, and moving it is the only way it
   survives its parent changing address.

Steps 2 and 3 are the ones that matter for schema editing: they are the path an
edited schema takes to reach a running treedb. Step 4 is what stands between an
editor and a treedb that no longer opens.

## Run

```bash
ctest -R test_c_treedb_system_schema --output-on-failure --test-dir build
```
