# inotify

`inotify` is a small diagnostic tool for raw Linux `inotify`, one level below `fs_watcher`. It puts an inotify watch on a path. It prints each event with its flag bits decoded (`IN_MODIFY`, `IN_CREATE`, `IN_DELETE`, `IN_CLOSE_WRITE`, …).

Use it when `C_FS` / `watchfs` / `fs_watcher` do not work as expected. It shows if the kernel sends the expected events, independent of the Yuneta layers.

## Usage

```bash
inotify <path>
```

To stop the tool, press Ctrl-C.
