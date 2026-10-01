# Cloud review of 7.25.21

The five defects, seven nits and eight risks of this review are fixed on
main (`6fab00d69`, `5867b1e20`, `6b53c8b17`; see `CHANGELOG.md`
"Unreleased"). Their fixes were checked: clean build with no warning, suite
277/277 as user `yuneta` under `ulimit -Sn 1024`. What is left open:

## Open

- **`timeout_stop_tx` on old kernels.** The stop of a `C_TCP` bounds a write
  in flight with `TCP_USER_TIMEOUT`. A peer with a zero window is held by
  zero-window probes, and older kernels may not end those at the user
  timeout. It works on 6.x (this container, Debian 12); run
  `c_tcp_s_stats` (`drain_port`) on a 4.18 node (RHEL 8) before relying on
  it there.
- **A positional secret that is not the last required parameter.** Written
  with blanks, the rest of it goes into the next required parameter, and
  an extra word after that one is echoed in the "extra parameters" answer.
  No command in the tree has a required `SDF_SECRET` parameter today.

## Closed after the review

- **The Debian 6.x + ext4 limitation** of the CHANGELOG ("Known
  limitations"): `timeranger2/test_delete_key_propagation` ("race in the
  batch") failed 9 runs in 10 on wattyzer and on hidraulia. It was the HIGH
  open since 7.25.20, an rt_disk follower handing a reborn key out of order;
  fixed by reading each key directory through a descriptor of its own
  (`FS_FLAG_DIR_FDS`, `openat`/`unlinkat`). The test passes 30 in 30 on
  wattyzer. See `CHANGELOG.md` "Unreleased".
