# Cloud review of main

Reviewed up to `e6ace430a` (2026-10-02). What is resolved is removed from
this file. Last check: clean build with no warning, suite 278/278 as user
`yuneta` under `ulimit -Sn 1024`, `timeranger2/test_delete_key_propagation`
20 in 20 (kernel 6.18).

## Open

Nothing. Of this review:

- **Fixed** (see `CHANGELOG.md` "Unreleased", the reborn-key section): the
  use-after-free and the stale md2 descriptor under `EMFILE` in
  `keys_are_the_life_of()` (the md2 is looked for again after the open of
  the content, `build_path()` for its paths); the read by path in
  `consume_link_by_fd()` (removed: the path that consumed the link unlinks
  and then reads); the misleading `/proc` log (`ENOSPC`/`ENOMEM` fail at
  once with their own error); the indentation in
  `load_first_and_last_record_md()`; `event_wd` set twice; the descriptor
  count after the stop in the "dir fds" test.
- **Moved to `TODO.md`** ("An rt_disk follower out of descriptors or of
  watches"): the descriptor per key directory for the life of a feed, and a
  key directory whose watch cannot be made. Reading such a directory by path
  was not done: with no watch, the links made later are not heard either, so
  it would read some records and then go quiet.
- **Not changed**: the `/proc/self/fd/%d` path of `fs_watcher.c` is not
  assembled from segments, and an int cannot overflow `PATH_MAX`.

## Checked and correct

- **`a89a6992e` (`yev_loop`, kernel too old):** the probe frees itself. A
  kernel before 5.19 answers `-EINVAL` to the cancel flags, and a newer one
  answers 0 or `-ENOENT`, so the check is sound.
- **`23bd58aec` (`gclass_create()`):** it refuses a required secret before
  another required parameter. The check stops at the first parameter that
  is not required, as the parser does.
- **`b2f972382` (key directories through descriptors):**
  - Every directory descriptor is closed on destroy, stop, GONE, a dropped
    watch and a stale watch after an overflow.
  - A directory removed and made again cannot be reached through the old
    descriptor.
  - `unlinkat()` goes through the watched directory's descriptor.
  - The master side is unchanged.
  - The hot path costs about the same calls as before.
