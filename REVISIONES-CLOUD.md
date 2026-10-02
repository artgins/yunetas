# Cloud review of 7.25.21

The five defects, seven nits and eight risks of this review are fixed on
main (`6fab00d69`, `5867b1e20`, `6b53c8b17`; see `CHANGELOG.md`
"Unreleased"). Their fixes were checked: clean build with no warning, suite
277/277 as user `yuneta` under `ulimit -Sn 1024`. What is left open:

## Open

Nothing.

## Closed after the review

- **The Debian 6.x + ext4 limitation** of the CHANGELOG ("Known
  limitations"): `timeranger2/test_delete_key_propagation` ("race in the
  batch") failed 9 runs in 10 on wattyzer and on hidraulia. It was the HIGH
  open since 7.25.20, an rt_disk follower handing a reborn key out of order;
  fixed by reading each key directory through a descriptor of its own
  (`FS_FLAG_DIR_FDS`, `openat`/`unlinkat`). The test passes 30 in 30 on
  wattyzer. See `CHANGELOG.md` "Unreleased".
- **A positional secret that is not the last required parameter**: such a
  command table is refused by `gclass_create()`, with an ERROR (no command of
  the SDK or of the projects declares one). See `CHANGELOG.md` "Unreleased".
- **`timeout_stop_tx` on old kernels**: moot. A 4.18 kernel (RHEL 8) has no
  io_uring, which `yev_loop` needs for all its work, so no yuno runs there.
  The kernels Yuneta runs on (5.6 and later in practice; RHEL/Rocky 9's
  5.14 with io_uring backported) bound the zero-window wait with
  `TCP_USER_TIMEOUT`: `c_tcp_s_stats` (`drain_port`) passes on 7.0 and on
  wattyzer's 6.12.
