# Test Suite

Functional and integration tests that verify correctness of every layer —
from raw io_uring events up to full GObj protocol stacks.
All tests are registered as `ctest` targets and run automatically with
`yunetas test`.

**Source:** `tests/c/`

```bash
# Run the full suite
yunetas test

# Run a single test by name
ctest -R test_c_timer --output-on-failure --test-dir build

# Run ctest in a loop until first failure (flaky-test detection)
./ctest-loop.sh
```

## Event loop (yev_loop)

Low-level tests for the io_uring event loop, without the GObj layer.

| Binary | Description |
|--------|-------------|
| **`test_yevent_listen1–4`** | Server creates a listen socket and accepts one client connection. |
| **`test_yevent_connect1–2`** | Client connects to a listening server; verifies the connection succeeds. |
| **`test_yevent_traffic1–6`** | Client ↔ server message echo over plain TCP (multiple variants). |
| **`test_yevent_udp_traffic1`** | UDP message echo. |
| **`test_yevent_traffic_secure1`** | TLS-encrypted TCP message echo. |
| **`test_yevent_timer_once1–2`** | One-shot timer expiration. |
| **`test_yevent_timer_periodic1`** | Periodic (recurring) timer. |

**Source:** `tests/c/yev_loop/yev_events/`, `tests/c/yev_loop/yev_events_tls/`

## Timers

| Binary | Description |
|--------|-------------|
| **`test_c_timer`** | `C_TIMER` GClass — periodic timeout operations. |
| **`test_c_timer0`** | `C_TIMER0` GClass — basic (low-level) timer. |

**Source:** `tests/c/c_timer/`, `tests/c/c_timer0/`

## TCP networking

Plain and TLS TCP through the full GObj protocol stack.

| Binary | Description |
|--------|-------------|
| **`test_c_tcp test1–4`** | Plain TCP: connect, disconnect, echo, and rapid multi-message burst. |
| **`test_c_tcp2 test1–4`** | Same scenarios using the newer `C_TCP_S` method (no `child_tree_filter`). |
| **`test_c_tcps test1–4`** | TLS TCP: connect, disconnect, echo, and multi-message burst. |
| **`test_c_tcps2 test1–4`** | TLS TCP with the newer method. |

**Source:** `tests/c/c_tcp/`, `tests/c/c_tcp2/`, `tests/c/c_tcps/`, `tests/c/c_tcps2/`

## TLS certificate hot-reload

Protects the [cert-reload feature](guide/guide_cert_management.md) — validates
that `ytls_reload_certificates()` swaps certificates atomically without
dropping live sessions and rolls back cleanly on invalid material.

| Binary | Description |
|--------|-------------|
| **`test_cert_reload`** | Swap cert A → B and confirm `view-cert` reflects the new subject/not_after; feed an invalid cert and verify the previous context is kept intact (rollback). |
| **`test_cert_info`** | `ytls_get_cert_info()` edge cases: short / long validity, self-signed invariant, serial shape, client-side `NULL`, already-expired cert. |
| **`test_cert_reload_mem`** | 1000 reloads without any live session and asserts `get_cur_system_memory() == 0` (leak gate, run under valgrind for exhaustive checking). |
| **`test_yevent_reload_live`** | One reload while a TCP session is live; the session keeps working end-to-end. |
| **`test_yevent_reload_stress`** | 50 reloads with a live session, one echo message per iteration. |

**Source:** `tests/c/ytls/`, `tests/c/yev_loop/yev_events_tls/`

## HTTP parser (llhttp wrapper)

| Binary | Description |
|--------|-------------|
| **`test_c_llhttp_parser`** | Sanity test for the vendored llhttp library and the `ghttp_parser` wrapper: request/response parse, keep-alive pipelining, `HPE_PAUSED_UPGRADE` tail bytes, EOF completion via `ghttp_parser_finish()`. |

**Source:** `tests/c/c_llhttp_parser/`

## OAuth2 / BFF (c_auth_bff)

Self-contained yunos that spin up a **mock Keycloak** (signed HS256 JWTs,
scriptable latency / response status / body override) alongside `C_AUTH_BFF`
and drive `/auth/login`, `/auth/callback`, `/auth/refresh`, `/auth/logout`
through happy paths and failure modes. Each test is its own binary so a
crash or leak cannot mask neighbours.

| Binary | Description |
|--------|-------------|
| **`test1_login`** | Happy-path login round-trip through the BFF. |
| **`test2_kc_401`** | IdP returns 401; BFF must surface a stable `error_code` and not poison its `kc_ok` counter. |
| **`test3_callback`** | `/auth/callback` code-for-token exchange. |
| **`test4_refresh`** | Proactive refresh-token round-trip. |
| **`test5_logout`** | Logout clears cookies and drives the IdP revoke endpoint. |
| **`test6_invalid_body`** | Malformed JSON body → 4xx with `error_code` mapped for the GUI. |
| **`test7_slow_login`** | IdP replies slowly; the BFF keeps the browser waiting without timing out early. |
| **`test8_queue_full`** | Pipeline 4 POSTs + overflow: `dl_list` pending queue drops the overflow in order and reports the configured `pending_queue_size`. |
| **`test9_browser_cancel`** | Browser disconnects mid-round-trip; the BFF drops the stale IdP reply (`responses_dropped` counter) instead of forwarding it. |
| **`test10_kc_silence`** | IdP goes totally silent; `kc_timeout_ms` watchdog fires, responds 504 to the browser and drains the task (`kc_timeouts` counter). |
| **`test11_cancel_retry`** | Cancel-then-retry: state cleanup between two back-to-back logins on the same browser. |
| **`test12_stale_reply`** | Per-task browser generation blocks cross-user token leak races. |
| **`test13_refresh_expired`** | Expired refresh token → mapped error, no silent `kc_error`. |
| **`test14_method_not_allowed`** | Non-POST on an auth endpoint → 405. |
| **`test15_missing_body`** | Missing body → 4xx with mapped error. |
| **`test16_unknown_endpoint`** | Unknown `/auth/*` path → 404, not 5xx. |

**Source:** `tests/c/c_auth_bff/` (includes `c_mock_keycloak`, `test_helpers`).

## MQTT

| Binary | Description |
|--------|-------------|
| **`test_c_mqtt test1`** | Self-contained broker + client: subscribe, publish QoS 0, verify reception, disconnect. |

**Source:** `tests/c/c_mqtt/`

## Subscriptions & link events

| Binary | Description |
|--------|-------------|
| **`test_subscriptions test1–2`** | GObj event subscribe / unsubscribe lifecycle. |
| **`test_c_node_link_events`** | `EV_TREEDB_NODE_LINKED` / `UNLINKED` events at the `C_NODE` GClass level. |
| **`test_tr_treedb_link_events`** | Low-level link/unlink callback mechanism in `tr_treedb`. |

**Source:** `tests/c/c_subscriptions/`, `tests/c/c_node_link_events/`,
`tests/c/tr_treedb_link_events/`

## Timeranger2 persistence

| Binary | Description |
|--------|-------------|
| **`test_tranger_startup`** | Database initialization and startup. |
| **`test_topic_pkey_integer`** | Open topic as master, manage runtime lists, append records with integer keys. |
| **`test_topic_pkey_integer_iterator`** | Iterator without callbacks over integer-key data. |
| **`test_topic_pkey_integer_iterator2`** | Iterator with per-key callbacks and time-based matching. |
| **`test_topic_pkey_integer_iterator3`** | Absolute-position searches. |
| **`test_topic_pkey_integer_iterator4`** | Relative-position searches. |
| **`test_topic_pkey_integer_iterator5`** | Paginated searches. |
| **`test_topic_pkey_integer_iterator6`** | Master/client iterator mode with realtime record additions. |
| **`test_testing`** | Testing utilities (event counters, assertions). |

**Source:** `tests/c/timeranger2/`

## TR_MSG & TR_QUEUE

| Binary | Description |
|--------|-------------|
| **`test_tr_msg1`** | Message topics: iteration, key matching, instance retrieval. |
| **`test_tr_msg2`** | Stress variant: 1 000 devices × 100 traces. |
| **`test_tr_queue1`** | Queue topic: enqueue / dequeue with time-based keys over a multi-day period. |

**Source:** `tests/c/tr_msg/`, `tests/c/tr_queue/`

## TreeDB

| Binary | Description |
|--------|-------------|
| **`test_tr_treedb`** | Schema creation, user/department/compound structures, node CRUD, and state snapshots (foto files). |

**Source:** `tests/c/tr_treedb/`

## Keyword matching

| Binary | Description |
|--------|-------------|
| **`test_kw1`** | `kw_match_simple()` with strings, booleans, integers, and floats. |

**Source:** `tests/c/kw/`

## Generic message interchange

A JSON-driven test runner that executes scripted test sequences and validates
event traces and error logs.

| Test scenario | Description |
|---------------|-------------|
| **`test_mqtt_qos0`** | MQTT QoS 0 publish/subscribe interchange. |
| **`test_tcp_connect`** | TCP connection establishment. |
| **`test_tcp_reconnect`** | TCP reconnection and recovery. |

**Source:** `tests/c/msg_interchange/`

## Build flag

Tests are enabled by default via `.config` (`CONFIG_MODULE_TEST=y`).
Toggle with `menuconfig` or pass `-DENABLE_TESTS=OFF` to CMake.

## Related

- **Performance benchmarks** live under `performance/c/` — `perf_c_tcp`, `perf_c_tcps`, `perf_yev_ping_pong`, `perf_yev_ping_pong2` and the new **`perf_auth_bff`** (ping-pong-style live throughput over the BFF; default 10 s runs, ~180 000 ops on the reference box).
- **Stress runners** live under `stress/c/` — `stress/auth_bff` drives concurrent BFF login / refresh / logout cycles to expose races between the pending queue, the watchdog and the flush-on-disconnect path.
