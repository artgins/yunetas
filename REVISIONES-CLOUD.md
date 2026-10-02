# Cloud review of main

Reviewed up to `f35562dea` (2026-10-02). What is resolved is removed from this
file; this version is the fixing session's answer, item by item, on
`94533480e`. Every fix has a test that fails on the code before it, except
where it says otherwise.

## Resolved

### High

| # | Item | Commit | Test |
|---|------|--------|------|
| 1 | `jwks` / `default_role` config-only (`SDF_RD`); `add-jwk`/`remove-jwk` change the running set only | `34f8db075` | `command_delete_user`: `write-attr` of both refused, nothing planted |
| 2 | `register-idp-user` with a role asks `update` of `treedb_authzs` (`has_role` answers `may_link`) | `7c03eadea` | `command_delete_user`: a registrar only gets -403; `may_link` per user |
| 3 | The missing comma (`"username" "%s"`) | `34f8db075` | `command_delete_user`: a username holding `%s` |
| 4 | dbsimple: a trusted file writable by group/others refused; the yuno's user = owner of the nearest directory nobody else can write | `76a7db735` | `secret_attrs` case 7 (a root file 0664). The root-yuno branch needs root: no ctest |
| 5 | Units: the deb `prerm` stops and disables both units inline; `ExecStopPost` kills a leftover agent in the unit's own cgroup; the comment says what `KillMode=process` does | `bb3796a55` | The units by hand on wattyzer; the packages (deb upgrade from 7.25.21, removal) are tested with the release |

### Medium

| # | Item | Commit | Test |
|---|------|--------|------|
| 6 | `kill-yuno` signals a yuno alive and not connected (found by the `/proc` scan); `restart_nodes()` relaunches once the killed pids are gone (100 ms look, 10 s max, then says who was left) | `6cd4063c1`, `b0b0068fd` | No ctest compiles `c_agent.c`: checked by hand (SIGSTOP) |
| 7 | C_PROT_TCP4H: small initial buffer, max = frame length + 1; C_WEBSOCKET: a frame of exactly `max_payload_size` completes | `ff7ffd484` | `c_prot_tcp4h/test1` (new), `c_websocket/test2` |
| 8 | C_TCP: an alive marker chain in priv around `EV_CONNECTED` and `EV_RX_DATA` (cleared by `mt_destroy`); the read re-armed when a subscriber answers -1; `set_secure_connected()` checks the sskt and the state after its publish; OpenSSL `flush_encrypted_data()` releases its gbuffer | `d64ac9ff3`, `f35562dea` | `c_tcps/test8` (new); the `EV_CONNECTED` TLS drop has no red test (asynchronous in the test) |
| 9 | C_AUTHZ with no users treedb answers the jwk commands, and the user commands `-1` *"no users treedb in this yuno"*; the upgrade notes say the -403 through `command-yuno` and that `SDF_AUTHZ_X` does nothing by default; the role example carries `realm_id` | `d70e02d8b` | `command_delete_user` |

### Low

| Item | Commit | Test |
|------|--------|------|
| `__reset__` zeroes `refusedConnxs` / `noChannelConnxs` -- and, the same defect, every priv counter read through `mt_reading` in C_TCP, C_TCP_S and C_UDP_S | `b6e7dd43c` | `c_tcp_s_ip_lists`, `c_udp_s_rx` |
| The daemon's close loop: `close_range()` (the loop stays for a kernel < 5.9) | `b6e7dd43c` | By hand: a yuno started with fd 50 open under `ulimit -Sn 20` keeps only its own |
| `info-uptime` reads `errno` before the log | `b6e7dd43c` | -- |
| `--pid-file` without `--start` is refused | `b6e7dd43c` | By hand |
| fs_watcher: the half-limit warning counts the dir fds of every watcher of the process, said again only after falling under the half | `3554be928` | `test_fs_watcher_overflow` (two watchers of 40 under 128) |
| fs_watcher: an end said when `FIONREAD` fails adds all the kernel can hold (`max_queued_events`), never short; a `FIONREAD` that fails again at the pad check ends the watcher (`FS_WATCHER_GONE`) | `3554be928` | `test_fs_watcher_overflow` (1000 files; a broken FIONREAD) |
| Control center pause+play in one turn: fixed in C_TIMER0 -- an arm while its cancel is in flight is done when the cancel ends, and that cancel is not published as `EV_STOPPED` | `868afac7c` | `test_c_timer0` |
| TLS reasons: mbedTLS `close_notify` (*"the peer closed the TLS session"*), data before the handshake (both backends), the WANT stall | `a2d6db1a5` | `ytls/test_free_inside_callback`, both backends (mbedTLS linked by hand: the local build has OpenSSL only) |
| Rates (accepted design): its three limits are written in `gateway.md` | `bccb3b181` | -- |
| timeranger2 9c: a feed of metadata only is fed a record whose body is lost | `43bfe823e` | No red test (the fallback needs a system without /proc) |
| timeranger2 9d: on a master, a file missing from a key still on disk is a CRITICAL *"the store is damaged"*; a follower or a key gone keeps the warning | `43bfe823e` | `test_read_never_exits` |
| rt_disk close: `rmrdir()` walks once more a directory filled while walked (ENOTEMPTY), so only a real failure logs; `.closing.<pid>-<start>.<seq>` carries the process start time, and a reused pid's leftover is removed | `43bfe823e` | `helpers/test_dir_read_error` 19; `test_delete_key_propagation` |
| `--stop`: SIGQUIT to every process of the name, 10 s to be gone, SIGKILL only to what is left | `5337b15bb` | By hand: a `--stop` takes 109 ms (it took 2 s, and gave the agent 1 s) |
| "has `create` but not `update`, with a role" | `8c9783245` | `command_delete_user` |

Also fixed in this round, from TODO.md section 1: a key directory whose watch
fails with `ENOSPC`/`ENOMEM` is tried again at each batch of the watcher and
handed as created once watched, so the follower reads its records
(`94533480e`; `test_fs_watcher_overflow`, `test_rt_disk_unwatched_key` new).

## Not done, and why

- **The soft open-files limit and the release suite's `ulimit -Sn 1024`
  axis.** Every yuno raises its soft limit to the hard one, so a yuno-based
  test no longer runs at 1024. Kept on purpose: the nodes' yunos do the same
  (the packages give `nofile unlimited`), so a yuno test at 1024 tests a
  configuration no node runs. The axis keeps covering what does not go through
  the entry point (the timeranger2 tests, the CLI tools).
- **9a/9b** (the accounting of key deletes in rt_disk followers): the design
  is decided and written in TODO.md -- a per-topic delete sequence carried
  in the NAME of the master's signal (`.delete.<seq>.<key>`), compared per key
  by the follower. It changes the protocol between master and follower
  processes (an old follower does not understand the new signal), so it goes
  in its own release, next cycle, with an upgrade note.
- **26** (msg2db consumers, `C_GATE_PVPC` urls): code of the projects, moved
  to their own TODO files.
- **No red test** still for: item 6 (mbedTLS in `ST_WAIT_STOPPED`), item 15
  (the agent; no ctest compiles `c_agent.c`), item 16, the root-yuno branch
  of item 2.
