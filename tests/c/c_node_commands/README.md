# c_node_commands test

What the read commands of `C_NODE` ANSWER (their permissions are tested in
`tests/c/c_node_authz`). On a small treedb -- departments in a tree (the self
hook `departments`), users under departments (`users`), items with a pkey2
(`version`) -- it asks:

- `treedb-info`: the treedb name, `master: true`, `schema_version` 1 and the
  topics.
- `node`: a node with its links (`department_id` names `top`, the `users`
  hook holds `ana`); a node that is not there answers `-1`, *"Node not
  found"*, `data` null.
- `instances` (both versions of `item1`, `v1` and `v2`) and `pkey2s`
  (`["version"]`).
- `jtree` of `top` by `departments` with `rename_hook: "data"`: the children
  whole in `data`, each with its `__path__` (`` top`dev ``), the hook itself
  gone. And without `rename_hook`: the children whole in the hook itself,
  each ONCE (up to 7.25.22 after the refs the hook held: `dev ops dev ops`).
- `parents` of `dev` (`top`; `["top"]` with `options: {only_id: 1}`; `[]` for
  the root) and `children` (`dev ops` of `top`; `ana` by the `users` hook of
  `dev`; nothing by `users` of `top`, not recursive). **With no `options`**:
  up to 7.25.22 both logged an ERROR with a stack for every option they
  looked for (*"kw must be list or dict"*: six for `parents`, one for
  `children`), because the options were read off a NULL; now they are an
  empty dict, as `nodes` already did.
- `hooks` of `departments` (`departments`, `users`) and `links` of `users`
  (`departments`) and of `departments` (`department_id`).
- `print-tranger` with `path: "topics"`: the subtree, `departments` in it.
- A kw that carries a gbuffer through `treedbs`, `links`, `hooks` and
  `node`: each keeps its reference to the kw with `kw_incref()`, which takes
  one of the gbuffer too (red with `json_incref()`, as up to 7.25.4: eight
  *"BAD gbuf_decref()"*).
- The snaps, on the data: `shoot-snap s1`, the node changed and saved;
  `activate-snap s1` marks it active (`snaps` says so) and the treedb opened
  again (C_NODE stopped and started, as the agent's `restart_nodes()` does)
  shows the node as it was (INFO *"loading snap_tag 1"*); `deactivate-snap`
  and opened again, the node as it is.

- `export-db` (`filename: roundtrip`, written as `roundtrip.trdb.json`
  under the realm's `temp/`) and `import-db` of that file, with `dev`
  renamed meanwhile, in each mode of `if-resource-exists`: `skip` (nothing
  overwritten, every node ignored, `dev` keeps its new name), `overwrite`
  (every node overwritten, none added, `dev` back to what was exported) and
  the default, abort (`-1`, *ABORTED*). Each node that exists is tried
  first: one WARNING *"Node already exists"* each.

Set `C_NODE_COMMANDS_PROBE=1` to print every answer.

## Run

```bash
ctest -R test_c_node_commands --output-on-failure --test-dir build
```
