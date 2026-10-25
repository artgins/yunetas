# c_yuno_stats test

Tests the memory and disk stats of a yuno (`load_memory_stats()` and
`load_disk_stats()` in `c_yuno.c`).

- `test1`: `C_TEST_STATS` opens a `C_TRANGER` whose store is in `/dev/shm`
  (a tmpfs), so the yuno writes to two filesystems: `/yuneta` and that one.
  After three readings of the stats (one a second) it checks:
  - `mem_rss_kb` and `mem_node_total_mb` above 0,
    `mem_node_available_percent` in 0..100;
  - `disk_min_free_percent` in 0..100, its path (`disk_min_free_path`) on
    `/yuneta` or in the store, on the FULLER of the two as `statvfs()`
    measures them in the test, and the percent the same within a point.

  Up to 7.28.3 a yuno told only of `/yuneta` (`disk_free_percent`): the
  queues of a simulator filled a ramdisk while `/yuneta` had room.

## Run

```bash
ctest -R c_yuno_stats --output-on-failure --test-dir build
```
