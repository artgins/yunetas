# inotify

Tiny diagnostic tool for raw **Linux `inotify`** — one step below `fs_watcher`. Sets up an inotify watch on a path and prints each event with its flag bits decoded (`IN_MODIFY`, `IN_CREATE`, `IN_DELETE`, `IN_CLOSE_WRITE`, …).

Useful when `C_FS` / `watchfs` / `fs_watcher` misbehave and you need to confirm whether the kernel is actually emitting the expected events, independent of any Yuneta layering.

## Usage

```bash
inotify <path>
```

Ctrl-C to exit.
