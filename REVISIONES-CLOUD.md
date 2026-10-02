# Cloud review of main

Reviewed up to `1af91cb43` (2026-10-02). What is resolved is removed from
this file. Last check: clean build with no warning, suite 278/278 as user
`yuneta` under `ulimit -Sn 1024`; `timeranger2/test_delete_key_propagation`
and `test_fs_watcher_overflow` 20 in 20 (kernel 6.18).

## Open

Nothing pending.

The two items of the review of `b2f972382` that are not fixed (a follower
out of descriptors or of watches) are tracked in `TODO.md`, "An rt_disk
follower out of descriptors or of watches".
