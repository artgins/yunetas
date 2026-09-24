# tests/c

Unit and integration tests for the C kernel. Each sub-directory tests a specific GClass or subsystem and is built as a standalone CTest target.

## Running

```bash
# Build everything (first time)
yunetas init && yunetas build

# Run the whole suite
yunetas test

# Or a single test
ctest -R test_c_timer --output-on-failure --test-dir build

# Loop until first failure (flaky detection)
./ctest-loop.sh
```

## What's here

Every sub-directory that `tests/c/CMakeLists.txt` builds:

| Directory | Target |
|---|---|
| `yev_loop` (`yev_events`, `yev_events_tls`, `static_resolv`) | io_uring event loop: listen, connect, TCP and UDP traffic, timers, TLS, the static resolver |
| `ytls` | TLS layer: certificate info and reload, handshakes refused, the TLS floor, peer verification |
| `timeranger2` | timeranger2 append / read / iterator tests |
| `tr_msg` | msg2db wrapper (`tr_msg.c`) |
| `tr_msg2db` | msg2db (`tr_msg2db.c`): an empty `pkey2`, a key whose history did not load whole |
| `tr_queue` | timeranger2 queue: enqueue, dequeue, ack, restart; a queue whose load cannot read every pending message |
| `tr_treedb`, `tr_treedb_link_events` | TreeDB core and link-event subscriptions |
| `tr_treedb_failed_save` | A treedb write whose save fails is taken back whole in memory, with no event (update, links, update with its links, clean, stale refs); a refused forced delete changes nothing, and a child it cannot put back stays unlinked in memory as on disk; two active snaps at open |
| `tr_treedb_delete_instance` | `treedb_delete_instance()` on a `pkey2` index, and a tombstone write that fails |
| `tr_treedb_hook_hygiene` | Hook fixes on a versioned (`pkey2`) parent: reverse-hook unlink, idempotent link |
| `tr_treedb_update_instance` | An update is seen through the `pkey2` index too |
| `tr_treedb_snap`, `tr_treedb_snap_clone` | Snap shoot / activate / clear, and the clone of a record another snap tagged |
| `tr_treedb_immutable` | Immutable topics and records |
| `tr_treedb_files` | The `file` column and `__assets__` |
| `tr_treedb_schema_parse` | `parse_schema()` on a column with no `flag` |
| `tr_treedb_rowid` | The id of a topic whose `id` column has the `rowid` flag |
| `tr_treedb_load_failed` | A topic whose keys cannot all be read: treedb loads the others, remembers the failed keys, and refuses what memory would answer wrong (a create of such an id, snapshot operations with a partial `__snaps__`) |
| `tr_treedb_relink` | A link into a single-valued fkey replaces the old one |
| `tr_treedb_hook_rename` | Renaming a hook, and the derived `fkey` mark |
| `tr_dt_unknown` | A topic on a filesystem whose `readdir()` gives no `d_type` |
| `gobj_post_event` | `gobj_post_event()`: delivery on the next cycle of the loop |
| `c_timer0`, `c_timer` | `C_TIMER0` / `C_TIMER` scheduling |
| `c_tcp`, `c_tcp2` | `C_TCP` client GClass (connect, I/O, timeouts) |
| `c_tcps`, `c_tcps2` | `C_TCP_S` TLS client (handshake, OpenSSL + mbedTLS) |
| `c_tcp_inactivity` | `C_TCP` `timeout_inactivity` |
| `c_subscriptions` | subscribe/publish semantics of the GObj core |
| `kw` | `kw_*` helpers from `gobj-c/kwid.c` |
| `helpers` | `helpers.c` string helpers, the agent's audit record builder, the rotatory (log files) |
| `build_path` | `build_path()` and its clamped `..` |
| `gbuffer` | NULL guards of the gbuffer accessors; a refused `gbmem_realloc()` leaves the old block valid and tracked |
| `glogger_utf8` | The logger never writes a record that is not valid UTF-8 JSON |
| `command_authz` | The per-command authorization gate (`SDF_AUTHZ_X`) |
| `command_delete_user` | `delete-user` of C_AUTHZ: immutability is the only boundary |
| `command_shutdown` | `shutdown` answers first and stops after |
| `command_binary_kw` | A command, and `build_stats()`, whose kw carries a `gbuffer`: the handler's kw holds a reference of its own, and the caller's references are intact after the command |
| `c_tranger` | `C_TRANGER` commands: topics, keys, paged records |
| `libjwt` | JWT algorithm-confusion regression |
| `c_node_link_events` | TreeDB `EV_TREEDB_NODE_LINKED/UNLINKED`, and `import-db` |
| `c_node_failed_save` | `update-node` with `autolink` whose save fails: answered `NULL` / `-1`, taken back whole, no event (update and create) |
| `c_node_initial_load` | The `initial_load` seed of `C_NODE` |
| `c_assets` | `C_ASSETS`: the bytes a treedb node owns |
| `c_node_paged_nodes` | Paging of `C_NODE`'s `nodes` |
| `c_node_authz` | The permission every `C_NODE` command asks for |
| `c_treedb_system_schema` | `__system__`, the meta-treedb of `C_TREEDB`: projection, save, apply, delete |
| `c_treedb_literal_wins` | `C_TREEDB`: a schema from C newer than the schema file in use wins whole, `__system__` is projected from it whole, the operator work it discards is reported once, and an unfinished or killed projection completes at the next open |
| `treedb_schema_fidelity` | Every real schema of the tree through `__system__` and back |
| `c_mqtt` | Embedded MQTT broker + client round-trip |
| `c_auth_bff` | BFF HTTP auth flow (mock Keycloak + signed JWTs) |
| `c_task_authenticate` | `C_TASK_AUTHENTICATE`, the OIDC password-grant task |
| `c_llhttp_parser` | The vendored llhttp and `ghttp_parser` |
| `msg_interchange` | `msg_ievent` / `iev_msg` conversion |

### Added after 7.25.4

| Directory | New binaries |
|---|---|
| `c_node_failed_save`, `c_treedb_literal_wins`, `command_binary_kw`, `tr_treedb_failed_save`, `tr_treedb_load_failed` | new directories, one binary each (`test_<directory>`) |
| `gbuffer` | `test_gbmem_realloc_refused` |
| `helpers` | `test_audit_record`, `test_rotatory`, `test_dir_array_nomem` |
| `timeranger2` | `test_tm_order`, `test_lost_lock`, `test_topic_var_replace`, `test_key_reborn_pages`, `test_open_list_history`, `test_unreadable_at_open`, `test_mark_tm_order`, `test_uncommitted_append`, `test_torn_md2_tail`, `test_md2_read_error`, `test_md2_short_write`, `test_nul_escape_record`, `test_torn_tail_check_fails`, `test_cmp_file_ids` |
| `tr_msg2db` | `test_msg2db_load_failed` |
| `tr_queue` | `test_tr_queue_load_failed` |
| `yev_loop/yev_events` | `test_yevent_sq_full`, `test_yevent_sq_nomem`, `test_yevent_sq_retry`, `test_yevent_stop_in_flight`, `test_yevent_udp_ipv6`, `test_yevent_udp_zerocopy`, `test_yevent_loop_end_drain`, `test_yevent_connect_src_url` |

Tests that compare against expected `INFO`-level log output rely on the backend being **silent in `set_trace()`** — see `kernel/c/ytls/README.md`.
