(util-fs_watcher)=
# `fs_watcher`

CLI harness for Yuneta's `fs_watcher` library: it watches a directory through
`yev_loop` and prints every filesystem event (dir/file created, deleted,
modified). Useful for diagnosing the watch layer that [`watchfs`](../yunos/watchfs.md)
is built on.

## Usage

```bash
fs_watcher <directory>
```

Watches recursively; press Ctrl-C to exit (it dumps the tracked paths on quit).
No options — a single directory argument.

## See also

- [`inotify`](inotify.md) — the raw layer below this.
- [`utils/c/fs_watcher/README.md`](https://github.com/artgins/yunetas/blob/7.5.3/utils/c/fs_watcher/README.md).
