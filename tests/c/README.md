# tests/c

Unit and integration tests for the C kernel. Each sub-directory tests a specific GClass or subsystem and is built as a standalone CTest target.

## Running

```bash
# Build everything (first time)
yunetas init && yunetas build

# Run the whole suite
yunetas test

# Or a single test
ctest -R '^test_c_timer$' --output-on-failure --test-dir build

# Loop until first failure (flaky detection)
./ctest-loop.sh
```

## What's here

Every sub-directory that `tests/c/CMakeLists.txt` builds:

| Directory | Target |
|---|---|
| `yev_loop` (`yev_events`, `yev_events_tls`, `static_resolv`) | io_uring event loop: listen, connect, TCP and UDP traffic, timers, TLS, the static resolver; a full or failing submission queue, stops, the end of a loop, a closed fd with other events' submissions on it, a stop in a wrapped ring |
| `ytls` | TLS layer: certificate info and reload, handshakes refused, the TLS floor, peer verification |
| `timeranger2` | timeranger2 append / read / iterator tests |
| `tr_msg` | msg2db wrapper (`tr_msg.c`) |
| `tr_msg2db` | msg2db (`tr_msg2db.c`): an empty `pkey2`, a key whose history did not load whole |
| `tr_queue` | timeranger2 queue: enqueue, dequeue, ack, restart; a queue whose load cannot read every pending message; a backup that fails; the queued messages of an mqtt queue (a content that cannot be read stays queued, no backup while messages are queued) |
| `tr_treedb`, `tr_treedb_link_events` | TreeDB core and link-event subscriptions |
| `tr_treedb_failed_save` | A treedb write whose save fails is taken back whole in memory, with no event (update, links, update with its links, clean, stale refs); a refused forced delete changes nothing, and a child it cannot put back stays unlinked in memory as on disk; two active snaps at open |
| `tr_treedb_delete_instance` | `treedb_delete_instance()` on a `pkey2` index, and a tombstone write that fails; a delete of a key looks at every instance of it (the children of each, and each instance a parent's hook holds); a deleted instance leaves the hooks of its parents and hands its children to the primary; a save of a node no index holds is refused; after a reopen, the instance of the primary IS the primary node; an unlink clears the parent ref in every instance of the child, and a delete of a parent sees the instances that name it without a hook holding them; two `pkey2`s keep the primary's slot; the gc keeps an instance's asset; a forced delete, a delete taken back and a snap shot keep the newest record of each key on the instance that wrote it; a relink through a dict hook over a string fkey keeps the sibling instance |
| `tr_treedb_hook_hygiene` | Hook fixes on a versioned (`pkey2`) parent: reverse-hook unlink, idempotent link |
| `tr_treedb_update_instance` | An update is seen through the `pkey2` index too |
| `tr_treedb_snap`, `tr_treedb_snap_clone` | Snap shoot / activate / clear, and the clone of a record another snap tagged |
| `tr_treedb_immutable` | Immutable topics and records |
| `tr_treedb_files` | The `file` column and `__assets__` |
| `tr_treedb_schema_parse` | `parse_schema()` on a column with no `flag` |
| `tr_treedb_rowid` | The id of a topic whose `id` column has the `rowid` flag |
| `tr_treedb_load_failed` | A topic whose keys cannot all be read: treedb loads the others, remembers the failed keys, and refuses what memory would answer wrong (a create of such an id, snapshot operations with a partial `__snaps__`) |
| `tr_treedb_relink` | A link into a single-valued fkey replaces the old one |
| `tr_treedb_hook_rename` | Renaming a hook, and the derived `fkey` mark; a stale ref in a string fkey column is removed by a clean and a forced delete |
| `tr_dt_unknown` | A topic on a filesystem whose `readdir()` gives no `d_type`: keys found with `lstat()`, a symbolic link is not a key, an unreadable key is flagged |
| `gobj_post_event` | `gobj_post_event()`: delivery on the next cycle of the loop |
| `c_timer0`, `c_timer` | `C_TIMER0` / `C_TIMER` scheduling |
| `c_tcp`, `c_tcp2` | `C_TCP` client GClass (connect, I/O, timeouts); a write that does not start drops the connection (`test5`); the stop of a client already disconnected publishes `EV_STOPPED` and it starts again (`test6`) |
| `c_tcps`, `c_tcps2` | `C_TCP_S` TLS client (handshake, OpenSSL + mbedTLS) |
| `c_tcp_inactivity` | `C_TCP` `timeout_inactivity` |
| `c_tcp_s_ip_lists` | `C_TCP_S` at accept: a peer in `denied_ips` is refused (the deny wins over `allowed_ips`), with `only_allowed_ips` one not in `allowed_ips` is refused, loopback is exempt; a refusal is logged on the transition (one line a minute at most per cause) and counted in `refusedConnxs`; the list key of each peername form (IPv4, bracketed IPv6, IPv4 on a dual-stack socket); the `add-`/`remove-` ip commands store the form a peer is looked up by, refuse what is not an ip, and a `remove-` of an ip not in the list answers `-1`; entries stored as typed are renamed or dropped at load |
| `c_udp_s_tx` | `C_UDP_S` sends every datagram of its queue, and drops one it cannot send (no peer address: an error; refused by the kernel: a warning) and goes on listening |
| `c_udp_s_restart` | `C_UDP_S` reads and sends again after a stop and a start (a file on the old socket number; a stop and a start in the same turn), and publishes `EV_STOPPED` |
| `c_udp_s_self_stop` | `C_UDP_S` whose next read cannot start says it and stops by itself, and the stop ends (`EV_STOPPED`) with a send in flight; a `udps://` url is refused at the start |
| `c_udp_s_rx` | `C_UDP_S` labels every datagram with its peer, so `C_GSS_UDP_S` joins the interleaved pieces of two peers apart; the yuno's ip lists drop a denied peer, and with `only_allowed_ips` one that is not allowed, with one warning per cause (not per datagram) and the drops counted in `rxRefusedMsgs`; the caps of what a peer holds in `C_GSS_UDP_S` (peers, frame size, pending bytes); a host answers the peer of a frame by its label or its address |
| `c_udp_s_echo` | An answer written in the gbuffer of an `EV_RX_DATA` (the kw sent back as `EV_TX_DATA`) reaches its sender whole while other sends wait or are in flight and more datagrams arrive: the next read takes a new gbuffer when the host kept the old one |
| `c_subscriptions` | subscribe/publish semantics of the GObj core; a repeated hard subscription is one, a repeated `__own_event__` / `__rename_event_name__` subscription is overridden and can be withdrawn (`test3`) |
| `kw` | `kw_*` helpers from `gobj-c/kwid.c`; `kw_set_dict_value()` overwrites and answers `-1` when it writes nothing |
| `helpers` | `helpers.c` string helpers, the directory walks and listings (and the errno of a failed `mkrdir()`), the agent's audit record builder (the routing of a peer redacted and capped too), the rotatory (log files, `exit_on_fail` only at the open, the size limit in bytes) |
| `build_path` | `build_path()` and its clamped `..` |
| `gbuffer` | NULL guards of the gbuffer accessors; a refused `gbmem_realloc()` leaves the old block valid and tracked |
| `glogger_utf8` | The logger never writes a record that is not valid UTF-8 JSON |
| `command_authz` | The per-command authorization gate (`SDF_AUTHZ_X`) |
| `c_ievent_srv_peer_subs` | What a remote peer may put in a subscription through `C_IEVENT_SRV` (`__global__` keys of its own only, no `__local__`, no stray keys) and hold (`max_subscriptions`, `max_subscription_size`); a subscription's `__local__`/`__global__` change only its own copy of the event; each peer-repeated log written once; a client withdraws a subscription with a `__global__`; a `gbuffer` published to two remote subscribers and a local one (each twin holds its own reference); a `__username__` and routing stamps a peer sends are overwritten by the gate's; the routing a subscription stores is the peer's reversed hop only, and measured; a repeated subscription is kept as it is; a frame with no routing closes the channel with one capped warning |
| `c_subscription_authz` | The subscription authorization gate (`enable_subscription_authz`): `C_IEVENT_CLI` peers over websocket subscribe to `C_NODE`'s `EV_TREEDB_NODE_*` feed and to `C_TRANGER`'s `EV_TRANGER_RECORD_ADDED`; with the gate on, a peer without `read` is refused and the feed reaches only the accepted subscriptions; a peer's `__config__` keeps only `__first_shot__`; a channel disabled and enabled again serves a new session; the six `C_IOGATE` channel commands with a `channel_name` that matches nothing; the `authzs` trace prints the kw it checked; an orderly teardown of the gate |
| `command_delete_user` | `delete-user` of C_AUTHZ: immutability is the only boundary |
| `command_shutdown` | `shutdown` answers first and stops after |
| `command_binary_kw` | A command, and `build_stats()`, whose kw carries a `gbuffer`: the handler's kw holds a reference of its own, and the caller's references are intact after the command |
| `c_tranger` | `C_TRANGER` commands: topics, keys, paged records; `add-record`; the `rt_id` of a live list's pushes; a handle refused to another session, and through the agent's link to another user; the permission `mark-tm-order` and `add-record` ask |
| `libjwt` | JWT algorithm-confusion regression; the `jwks_*` keyring functions with a NULL set |
| `c_node_link_events` | TreeDB `EV_TREEDB_NODE_LINKED/UNLINKED`, and `import-db` |
| `c_node_failed_save` | `update-node` with `autolink` whose save fails: answered `NULL` / `-1`, taken back whole, no event (update and create) |
| `c_node_initial_load` | The `initial_load` seed of `C_NODE` |
| `c_assets` | `C_ASSETS`: the bytes a treedb node owns |
| `c_node_paged_nodes` | Paging of `C_NODE`'s `nodes` |
| `c_node_authz` | The permission every `C_NODE` command asks for |
| `c_agent_find_new_yunos` | The rows of the agent's `find-new-yunos` against a real `C_NODE` with the agent's schema: a yuno already registered at the new release is marked, not listed as new |
| `c_treedb_system_schema` | `__system__`, the meta-treedb of `C_TREEDB`: projection, save, apply, delete |
| `c_treedb_literal_wins` | `C_TREEDB`: a schema from C newer than the schema file in use wins whole, `__system__` is projected from it whole, the operator work it discards is reported once, and an unfinished or killed projection completes at the next open; `save-schema` of a node of two parents (and of one unshared again), of two topics of one name, and of a draft that shifts its siblings' places; a failed open keeps the saved schema |
| `treedb_schema_fidelity` | Every real schema of the tree through `__system__` and back |
| `c_mqtt` | Embedded MQTT broker + client round-trip; the ACL, `list-queues` / `clean-queues` of a store that cannot be listed, malformed packets, incoming QoS 2 messages queued at a session reload; the out window (the client's Receive Maximum), `list-queues qos=`, an ack of the wrong type; an MQTT client with persistent queues (a QoS 2 dup, an expired queued message) |
| `c_auth_bff` | BFF HTTP auth flow (mock Keycloak + signed JWTs) |
| `c_task_authenticate` | `C_TASK_AUTHENTICATE`, the OIDC password-grant task |
| `c_llhttp_parser` | The vendored llhttp and `ghttp_parser` |
| `msg_interchange` | `msg_ievent` / `iev_msg` conversion |

### Directories with more than one binary

The ctest name is `<directory>/<name>` for these (run one directory with
`ctest -R '^<directory>/' --output-on-failure --test-dir build`); a directory
with one binary registers it as `test_<directory>`. The name is the binary's,
except in `c_tcp`, `c_tcps`, `c_tcp2`, `c_tcps2`, `c_tcp_inactivity`,
`c_subscriptions`, `c_mqtt`, `c_auth_bff` and `c_task_authenticate`, whose
binaries carry a prefix: `c_mqtt/acl` runs `test_mqtt_acl`, `c_tcp/test5` runs
`test_tcp_test5` (the prefixes are `test_tcp_`, `test_tcps_`, `test_tcp2_`,
`test_tcps2_`, `test_tcp_inactivity_`, `test_subs_`, `test_mqtt_`,
`test_auth_bff_` and `test_task_authenticate_`).

| Directory | Names |
|---|---|
| `yev_loop/yev_events` | `test_yevent_listen1` - `4`, `test_yevent_connect1`, `2`, `test_yevent_traffic1` - `6`, `test_yevent_udp_traffic1`, `test_yevent_udp_zerocopy`, `test_yevent_udp_ipv6`, `test_yevent_stop_in_flight`, `test_yevent_loop_end_drain`, `test_yevent_connect_src_url`, `test_yevent_timer_once1`, `2`, `test_yevent_timer_periodic1`, `test_yevent_sq_full`, `test_yevent_sq_retry`, `test_yevent_sq_nomem`, `test_yevent_stop_nomem`, `test_yevent_kept_after_post`, `test_yevent_close_fd_kept`, `test_yevent_stop_stale_sqe` (ctest names `yev_events/...`) |
| `yev_loop/yev_events_tls` | `test_yevent_traffic_secure1`, `test_yevent_reload_live`, `test_yevent_reload_stress` |
| `yev_loop/static_resolv` | `test_static_resolv_spoof` (ctest name `static_resolv/...`) |
| `ytls` | `test_cert_reload`, `test_cert_info`, `test_cert_reload_mem`, `test_handshake_reject_openssl`, `test_handshake_reject_mbedtls`, `test_tls_floor_openssl`, `test_tls_verify_openssl` |
| `timeranger2` | 37 binaries, each one described in `tests/c/timeranger2/README.md` |
| `tr_msg` | `test_tr_msg1`, `test_tr_msg2` |
| `tr_msg2db` | `test_pkey2_empty`, `test_msg2db_load_failed` |
| `tr_queue` | `test_tr_queue1`, `test_tr_queue_load_failed`, `test_tr_queue_backup_failed`, `test_tr2q_queued` |
| `c_tcp`, `c_tcps`, `c_tcp2`, `c_tcps2`, `c_tcp_inactivity` | `test1` - `test4` each; `c_tcp` also `test5`, `test6` |
| `c_subscriptions` | `test1`, `test2`, `test3` |
| `kw` | `test_kw1`, `test_json_flat`, `test_kw_set_dict_value` |
| `helpers` | `test_helpers`, `test_rotatory`, `test_audit_record`, `test_dir_array_nomem`, `test_dir_listing`, `test_dir_read_error` |
| `gbuffer` | `test_gbuffer_guards`, `test_gbmem_realloc_refused` |
| `c_mqtt` | `test1`, `acl`, `malformed`, `queued_in`, `client_queues`, `out_flight` |
| `c_auth_bff` | `test1_login`, `test2_kc_401`, `test3_callback`, `test4_refresh`, `test5_logout`, `test6_invalid_body`, `test7_slow_login`, `test8_queue_full`, `test9_browser_cancel`, `test10_kc_silence`, `test11_cancel_retry`, `test12_stale_reply`, `test13_refresh_expired`, `test14_method_not_allowed`, `test15_missing_body`, `test16_unknown_endpoint`, `test18_discovery_failure`, `test19_logout_no_cookie` |
| `c_task_authenticate` | `test1_discovery`, `test2_explicit_endpoints`, `test4_discovery_failure` |
| `msg_interchange` | `test_mqtt_qos0`, `test_tcp_connect`, `test_tcp_reconnect` |

The single-binary directories that also register under `<directory>/`:
`build_path`, `glogger_utf8`, `command_authz`, `command_delete_user`,
`command_shutdown`, `command_binary_kw`, `c_tranger` and `libjwt`
(`libjwt/test_jwt_alg_confusion`).

### Added after 7.25.4

| Directory | New binaries |
|---|---|
| `c_agent_find_new_yunos`, `c_ievent_srv_peer_subs`, `c_node_failed_save`, `c_subscription_authz`, `c_tcp_s_ip_lists`, `c_treedb_literal_wins`, `c_udp_s_tx`, `c_udp_s_restart`, `c_udp_s_self_stop`, `c_udp_s_rx`, `c_udp_s_echo`, `command_binary_kw`, `tr_treedb_failed_save`, `tr_treedb_load_failed` | new directories, one binary each (`test_<directory>`) |
| `gbuffer` | `test_gbmem_realloc_refused` |
| `c_subscriptions` | `test_subs_test3` |
| `kw` | `test_kw_set_dict_value` |
| `c_tcp` | `test_tcp_test5`, `test_tcp_test6` |
| `c_mqtt` | `test_mqtt_queued_in`, `test_mqtt_client_queues`, `test_mqtt_out_flight` |
| `helpers` | `test_audit_record`, `test_rotatory`, `test_dir_array_nomem`, `test_dir_listing`, `test_dir_read_error` |
| `timeranger2` | `test_tm_order`, `test_lost_lock`, `test_topic_var_replace`, `test_key_reborn_pages`, `test_open_list_history`, `test_unreadable_at_open`, `test_mark_tm_order`, `test_uncommitted_append`, `test_torn_md2_tail`, `test_md2_read_error`, `test_md2_short_write`, `test_nul_escape_record`, `test_torn_tail_check_fails`, `test_cmp_file_ids`, `test_unlistable_dirs`, `test_unlisted_relist_once` |
| `tr_msg2db` | `test_msg2db_load_failed` |
| `tr_queue` | `test_tr_queue_load_failed`, `test_tr_queue_backup_failed`, `test_tr2q_queued` |
| `yev_loop/yev_events` | `test_yevent_sq_full`, `test_yevent_sq_nomem`, `test_yevent_sq_retry`, `test_yevent_stop_in_flight`, `test_yevent_udp_ipv6`, `test_yevent_udp_zerocopy`, `test_yevent_loop_end_drain`, `test_yevent_connect_src_url`, `test_yevent_stop_nomem`, `test_yevent_kept_after_post`, `test_yevent_close_fd_kept`, `test_yevent_stop_stale_sqe` |

Tests that compare against expected `INFO`-level log output rely on the backend being **silent in `set_trace()`** — see `kernel/c/ytls/README.md`.
