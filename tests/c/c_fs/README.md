# c_fs test

Tests what `C_FS` (`kernel/c/root-linux/src/c_fs.c`) publishes for each change
of the tree it watches.

A driver gclass (`C_TEST_FS`) creates a root with a subdirectory `sub` under
`$HOME/tests_yuneta/c_fs`, and a `C_FS` child watching it (subscribed to it).
When the yuno plays, the driver:

1. creates a file `f`, writes one byte and removes it;
2. creates a directory `d2`;
3. removes `sub`.

300 ms later it counts the `EV_FS_CHANGED` it received per `filename`, and
expects exactly `{"f": 3, "d2": 1, "sub": 1}` and no `EV_FS_RENAMED`.

Up to 7.25.20 `C_FS` read the types of `fs_watcher` as bits, though they are
values (1..8): `FS_FILE_MODIFIED_TYPE` (5) matched a created directory (1), a
created file (3) and a deleted file (4) by accident, and a deleted directory
(2) matched nothing -- `sub` was never published (`{"f": 3, "d2": 1}`), and the
`kw` built for it was leaked.

## Run

```bash
ctest -R test_c_fs --output-on-failure --test-dir build
```
