# c_fs test

Tests what `C_FS` (`kernel/c/root-linux/src/c_fs.c`) publishes for each change
of the tree it watches.

A driver gclass (`C_TEST_FS`) creates two trees and a `C_FS` child watching
each. It does not subscribe to them: a `C_FS` follows the CHILD subscription
model and subscribes its parent (up to 7.25.20 it subscribed nobody, the
driver subscribed by hand, and without that call it heard nothing):

- `$HOME/tests_yuneta/c_fs`, with subdirectories `sub` and `keep`, watched
  with `"recursive": 0`;
- `$HOME/tests_yuneta/c_fs_rec`, with `a/b`, watched with `"recursive": 1`.

When the yuno plays, the driver:

1. creates a file `f` in the first root, writes one byte and removes it;
2. does the same with `keep/n`, one level down;
3. creates a directory `d2`;
4. removes `sub`;
5. does the same as 1 with `a/b/g` in the recursive tree, two levels down.

300 ms later it counts the `EV_FS_CHANGED` it received per `filename` and per
`C_FS`, and expects exactly `{"f": 3, "d2": 1, "sub": 1}` from the first
(`n` is not in its tree), `{"g": 3}` from the recursive one, and no
`EV_FS_RENAMED`.

Up to 7.25.20 `C_FS` read the types of `fs_watcher` as bits, though they are
values (1..8): `FS_FILE_MODIFIED_TYPE` (5) matched a created directory (1), a
created file (3) and a deleted file (4) by accident, and a deleted directory
(2) matched nothing -- `sub` was never published (`{"f": 3, "d2": 1}`), and the
`kw` built for it was leaked.

Up to 7.25.20, too, every watch of `C_FS` recursed (`n` was published by the
non-recursive `C_FS`), and a recursive `C_FS` added a recursive watcher of its
own for each subdirectory found at start, each with its own inotify fd: `g`
was heard by the watchers of the root, `a` and `b`, and published 9 times.

## Run

```bash
ctest -R test_c_fs --output-on-failure --test-dir build
```
