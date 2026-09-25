# tr_dt_unknown test

Tests a timeranger2 topic opened on a filesystem that gives `readdir()` no
`d_type` (XFS with `ftype=0`, NFS, FUSE, overlay).

`find_keys_in_disk()` then asks the inode with `lstat()` (`stat()` up to this fix). It joined the entry
to the TOPIC's directory instead of its `keys/`: `<topic>/<key>` did not
exist, no key counted as a directory, and the topic opened with an **empty
cache** over files that were all there. Reads answered 0 rows, and the first
append started a cell `{rows:1}` over a file that held N, so the wrong record
was served from then on.

ext4 and tmpfs always fill `d_type`, and a fully static test binary cannot be
`LD_PRELOAD`ed. So this test links with `-Wl,--wrap=readdir,--wrap=lstat` (see its
`CMakeLists.txt`): every `readdir()` of the libraries goes through the test's
`__wrap_readdir()`, which can hide the type the way such a filesystem does:

```c
struct dirent *__wrap_readdir(DIR *dirp)
{
    struct dirent *entry = __real_readdir(dirp);
    if(entry && hide_d_type) {
        entry->d_type = DT_UNKNOWN;
    }
    return entry;
}
```

The test checks:

1. A topic with two keys and five records is written and closed.
2. Reopened with `d_type` hidden, it finds both keys and all five records,
   and the wrapper really served entries without a type.
3. Reopened with `d_type` hidden and the `lstat()` of key `B` failing with
   `EIO` (the test's `__wrap_lstat()`), the topic does **not** open:
   *"Cannot list the keys of the topic, stat() FAILED"* and *"Cannot open
   topic: its keys cannot be listed"*. Up to this fix the key was taken as
   "not a directory" and left out of the cache with no log, so the topic
   opened without it. Only `ENOENT` (the key went away between the
   `readdir()` and the `lstat()`) leaves a key out, as
   `find_files_with_suffix_array()` does with a file.
4. A symbolic link `keys/L -> A` is not a key, with `d_type` (it is
   `DT_LNK`) and without it: 2 keys, 5 records. Up to this fix the path
   without `d_type` asked `stat()`, which follows the link: a third key, with
   the records of `A`.
5. A key directory of mode `r--` (read, not searched: every `lstat()` and
   `open()` in it answers `EACCES`) is flagged `unreadable` the same way
   without `d_type` as with it. Up to this fix
   `find_files_with_suffix_array()` skipped each file with no log without
   `d_type`, and the key loaded EMPTY and unflagged. SKIPPED as root.
