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

| Directory | Target |
|---|---|
| `c_tcp`, `c_tcp2` | `C_TCP` client GClass (connect, I/O, timeouts) |
| `c_tcps`, `c_tcps2` | `C_TCP_S` TLS client (handshake, OpenSSL + mbedTLS) |
| `c_timer`, `c_timer0` | `C_TIMER` / `C_TIMER0` scheduling |
| `c_subscriptions` | subscribe/publish semantics of the GObj core |
| `c_mqtt` | Embedded MQTT broker + client round-trip |
| `c_auth_bff` | BFF HTTP auth flow (mock Keycloak + signed JWTs) |
| `c_node_link_events` | TreeDB `EV_TREEDB_NODE_LINKED/UNLINKED` |
| `c_node_failed_save` | `update-node` with `autolink` whose save fails: answered `NULL` / `-1`, taken back whole, no event (update and create) |
| `tr_treedb`, `tr_treedb_link_events` | TreeDB core and link-event subscriptions |
| `tr_treedb_failed_save` | A treedb write whose save fails is taken back whole in memory, with no event (update, links, update with its links, clean, stale refs); a refused forced delete changes nothing, and a child it cannot put back stays unlinked in memory as on disk; two active snaps at open |
| `tr_treedb_load_failed` | A topic whose keys cannot all be read: treedb loads the others, remembers the failed keys, and refuses what memory would answer wrong (a create of such an id, snapshot operations with a partial `__snaps__`) |
| `c_treedb_literal_wins` | `C_TREEDB`: a schema from C newer than the schema file in use wins whole, `__system__` is projected from it whole, the operator work it discards is reported once, and an unfinished or killed projection completes at the next open |
| `tr_msg`, `tr_queue` | timeranger2 message wrapper and queue (msg2db) |
| `timeranger2` | timeranger2 append / read / iterator tests |
| `kw` | `kw_*` helpers from `gobj-c/kwid.c` |
| `msg_interchange` | `msg_ievent` / `iev_msg` conversion |
| `yev_loop` | io_uring event loop (TCP, TLS, timers) |

Tests that compare against expected `INFO`-level log output rely on the backend being **silent in `set_trace()`** — see `kernel/c/ytls/README.md`.
