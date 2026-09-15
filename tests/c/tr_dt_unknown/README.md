# tr_dt_unknown test

Tests a timeranger2 topic opened on a filesystem that gives `readdir()` no
`d_type` (XFS with `ftype=0`, NFS, FUSE, overlay).

`find_keys_in_disk()` then asks the inode with `stat()`. It joined the entry
to the TOPIC's directory instead of its `keys/`: `<topic>/<key>` did not
exist, no key counted as a directory, and the topic opened with an **empty
cache** over files that were all there. Reads answered 0 rows, and the first
append started a cell `{rows:1}` over a file that held N, so the wrong record
was served from then on.

ext4 and tmpfs always fill `d_type`, and a fully static test binary cannot be
`LD_PRELOAD`ed. So this test links with `-Wl,--wrap=readdir` (see its
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
