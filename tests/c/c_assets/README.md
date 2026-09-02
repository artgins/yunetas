# c_assets test

Tests the `C_ASSETS` GClass: the bytes a treedb node owns but cannot hold.

What it checks:

- **The asset id IS the sha256 of its content.** The expected hash is
  computed outside this program (`sha256sum`), so the id is checked against
  an independent answer and not against the code that produces it.
- Storing the same bytes twice is **one** asset, not two. That is what makes
  a whole census reload create nothing at all.
- `get-asset` answers **inline** when no web server is configured and with a
  **signed url** once one is. The fallback is the whole reason a caller has a
  single code path, so both shapes are exercised in the same run.
- The signed url reproduces what nginx's `secure_link_md5` hashes. The
  expected token is worked out in the test from the directive's own template
  (`"$secure_link_expires$uri <secret>"`), never from the function under
  test, and the expiry is checked to land inside `url_ttl`.
- An asset is an **orphan** until a node links it, and "linked" is read from
  the schema's hooks.
- A linked asset cannot be deleted; an unlinked one can, and its bytes go
  with the row.
- `import-assets` turns a directory already on the node into N assets, skips
  what the service must not serve, records the `source_path` a loader links
  by, and a second run creates nothing.
- `gc-assets` removes exactly the orphans, and `dry_run` removes none.
- The authz gate answers `-403`. The test yuno installs its own checker that
  refuses one principal, so the gate is exercised rather than stepped around.

Two things this test exists to pin down, because both were found the hard way
and both are silent:

- **`gobj_topic_desc()` answers `{topic_name, pkey, ..., cols}`, while
  `topic_desc_hook_names()` walks a LIST OF COLS.** Handing it the dict does
  not fail — `json_array_foreach()` over an object iterates nothing — it
  answers "no hooks", and then every asset looks like an orphan.
- **With the `hook_size` option a hook is not a number: it renders as
  `[{"size": N}]`.** An empty hook is therefore a NON-EMPTY list, so "the
  list has elements" is exactly the wrong test — it makes every asset look
  linked and `gc-assets` deletes nothing.

Passing `NULL` as the `authz_checker` of `yuneta_setup()` does **not** mean
"no checker": `entry_point.c` installs `C_AUTHZ`'s, which denies everything
when no `C_AUTHZ` service is running. This yuno runs none, so it brings its
own.

## Run

```bash
ctest -R test_c_assets --output-on-failure --test-dir build
```
