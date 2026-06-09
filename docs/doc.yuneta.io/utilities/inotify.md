(util-inotify)=
# `inotify`

Raw Linux `inotify` probe — one level below [`fs_watcher`](fs_watcher.md). It
adds watches recursively and prints each event with its decoded flag bits,
reading the inotify fd through io_uring.

## Usage

```bash
inotify <directory>
```

No options — a single directory argument. Press Ctrl-C to exit.

## See also

- [`fs_watcher`](fs_watcher.md) — the higher-level library harness.
- [`utils/c/inotify/README.md`](https://github.com/artgins/yunetas/blob/7.5.6/utils/c/inotify/README.md).
