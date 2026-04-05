# fs_watcher

Standalone CLI test harness for the **`fs_watcher`** library (`kernel/c/timeranger2/src/fs_watcher.c`). Creates a `yev_loop`, attaches an `fs_event_t` to a path, and prints every filesystem event the watcher reports — handy for debugging the low-level layer that powers `root-linux/C_FS` and `yunos/c/watchfs`.

## Usage

```bash
fs_watcher <path>
```

Run against a directory and then touch/rename/delete files in it to see the event stream. Ctrl-C to exit.
