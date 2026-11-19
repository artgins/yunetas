# fs_watcher

A standalone CLI test harness for the `fs_watcher` library (`kernel/c/timeranger2/src/fs_watcher.c`). It prints every filesystem event that the watcher reports.

## Usage

```bash
fs_watcher <path>
```

1. Run `fs_watcher` on a directory.
2. In that directory, touch, rename, or delete files. The tool prints the stream of events.
3. To exit, press Ctrl-C.

The tool creates a `yev_loop` and attaches an `fs_event_t` to the path. Use it to debug the low-level layer under `root-linux/C_FS` and `yunos/c/watchfs`.
