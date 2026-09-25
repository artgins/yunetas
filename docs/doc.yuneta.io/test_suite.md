# Test Suite

Functional and integration tests that verify correctness of every layer —
from raw [io_uring](#io-uring) events up to full GObj protocol stacks.
All tests are registered as `ctest` targets and run automatically with
`yunetas test`.

**Source:** `tests/c/`

This page describes the tests by subject, and not every binary has a row here.
The complete index is `tests/c/README.md` in the repository:
every directory that `tests/c/CMakeLists.txt` builds, the binaries of each
directory that has more than one, and the ctest name of each. Each directory's
own `README.md` says what its binaries check (for timeranger2, all 37 of them).

```bash
# Run the full suite
yunetas test

# Run a single test by name
ctest -R '^test_c_timer$' --output-on-failure --test-dir build

# Run ctest in a loop until first failure (flaky-test detection)
./ctest-loop.sh
```

## Asserting on the logs

Most tests here assert on what was LOGGED, not on a returned value: they
whitelist the log messages the run is allowed to emit, and anything else fails
the test. Two functions do it, and choosing between them is a real decision.

`set_expected_results()` is **strict FIFO**. Every captured log must match the
HEAD of the list, so the list states the exact messages, in the exact order,
the exact number of times. This is what nearly every test wants and what you
should reach for by default.

```c
json_t *errors_list = json_pack("[{s:s}, {s:s}, {s:s}]",
    "msg", "Starting yuno",
    "msg", "Playing yuno",
    "msg", "Yuno stopped, gobj end"
);
set_expected_results(
    APP_NAME,     // test name
    errors_list,  // the messages the run may emit, in order
    NULL,         // expected json, NULL to check only the logs
    NULL,         // ignore_keys
    1             // verbose
);
...
result += test_json(NULL);   // NULL: check only the logs
```

`set_expected_results_unordered()` reads the same list as a **whitelist**: a
captured log matches ANY entry, a match does not consume the entry (so a
message may repeat), every entry must still be matched at least once, and
anything matching no entry fails the test. It gives up *"in this order, this
many times"* and keeps *"these things happened, and nothing else did"*.

```c
set_expected_results_unordered(APP_NAME, errors_list, NULL, NULL, 1);
```

**Use it only when the order is not ours to decide.** The case it was written
for is `c_tcp2/test2`: two `C_TCP` gobjs — the client and the accepted server
side — log `"Connected"` and `"Disconnected"` independently, and the driver
calls `set_yuno_must_die()` from inside one side's close callback, which logs
`"Exit to die"` synchronously and shuts the yuno down. The other side's last
log is swallowed, or is not, depending on whether the two close completions
land in the same io_uring batch. The **count** moved, not only the order, so no
ordered list could be right — and the test failed on a busy box naming a
message that was perfectly correct.

That is the bar. A test whose sequence is merely wrong should have its sequence
fixed, not its assertion relaxed.

## Event loop (yev_loop)

Low-level tests for the io_uring event loop, without the GObj layer.

| Binary | Description |
|--------|-------------|
| **`test_yevent_listen1–4`** | Server creates a listen socket and accepts one client connection. |
| **`test_yevent_connect1–2`** | Client connects to a listening server, then makes sure that the connection succeeds. |
| **`test_yevent_traffic1–6`** | Client ↔ server message echo over plain TCP (multiple variants). |
| **`test_yevent_udp_traffic1`** | UDP message echo. |
| **`test_yevent_traffic_secure1`** | TLS-encrypted TCP message echo. |
| **`test_yevent_timer_once1–2`** | One-shot timer expiration. |
| **`test_yevent_timer_periodic1`** | Periodic (recurring) timer. |
| **`test_yevent_sq_full`** | A full submission queue: the loop flushes it and asks again; when the kernel takes nothing, the submission is kept for the next cycle (a start answers `0`, a stop of a kept start gets `STOPPED` with `-ECANCELED`). |
| **`test_yevent_sq_nomem`** | No memory to keep a submission (largest block 100 000 bytes): the 1025th write start answers `-1` with *"No memory to keep a submission: event NOT started"*, the 1024 kept writes complete, and the memory tracking is whole at the end. |
| **`test_yevent_sq_retry`** | `io_uring_submit()` failing on demand: an entry left in the queue is submitted again at the next cycle and the timer fires; a stop takes back a read still in the queue, so a new timer with the same fd number fires; 100 cycles without the kernel taking anything log one ERROR, and one INFO when it takes them again. |
| **`test_yevent_stop_in_flight`** | A stop of a read, a write to a full socket and a recvmsg the kernel has: the event keeps its gbuffer until the completion of the cancel, and the callback gets it `STOPPED`, `-ECANCELED`, without gbuffer. |
| **`test_yevent_udp_ipv6`** | IPv6 peers on `[::1]`: a UDP listener on `udp://[::1]:0`, a datagram received from an IPv6 client and sent back to it (the peer address in the gbuffer with its length), and a TCP connect to `tcp://[::1]`. Skipped without IPv6 on the host. |
| **`test_yevent_udp_zerocopy`** | A zero-copy UDP send gives two completions: the callback is called once, an event destroyed in its callback lives until its notification, and a failed send (`-EMSGSIZE`) is `STOPPED` with no warning. |
| **`test_yevent_loop_end_drain`** | A loop ended with destroyed events whose completions have not come: `yev_loop_destroy()` frees each of them, and one whose completion never comes after 1 second, with an ERROR. A zero-copy send waiting for its notification is waited for 5 seconds and then NOT freed, with a WARNING: the kernel may still read its gbuffer. |
| **`test_yevent_connect_src_url`** | A connect bound to a local address (`src_url`): `127.0.0.1:<port>`, `[::1]:<port>` and `tcp://127.0.0.1:<port>`; the listener sees the peer at that port, and a bad `src_url` (`[::1:5000`) gives no socket and an error; a destination of two families (`localhost`) with a `src_url` of the second one skips the first address and connects to the second. |
| **`test_yevent_stop_nomem`** | A stop without memory for its cancel answers `-1` and gives up nothing: the timer keeps its fd, the read its gbuffer, and the read then completes with its data. A loop destroyed with no room for the cancel of its dying events says so before the final error. |
| **`test_yevent_kept_after_post`** | A posted action (`gobj_post_event()`) stops a timer whose start is kept: the `STOPPED` reaches the callback at the next cycle, not at the timeout of the run. |
| **`test_yevent_close_fd_kept`** | The socket of a connect event is closed (a stop, then a destroy) while a write of another event on it is still untaken (in the submission queue, then kept): the write is taken back and its callback gets `STOPPED`, `-ECANCELED`; a new socketpair that takes the number reads nothing. |
| **`test_static_resolv_spoof`** | The static resolver (`yuneta_getaddrinfo()`) accepts a DNS answer only from the nameserver it asked: a forged answer from another source port is dropped by the kernel (the UDP socket is connected). |
| **`test_gobj_post_event`** | [`gobj_post_event()`](#gobj_post_event): an event posted is not delivered at once, it arrives on the next cycle of the loop, in order, each with its own kw. |

**Source:** `tests/c/yev_loop/yev_events/`, `tests/c/yev_loop/yev_events_tls/`,
`tests/c/yev_loop/static_resolv/`, `tests/c/gobj_post_event/`

## Timers

| Binary | Description |
|--------|-------------|
| **`test_c_timer`** | [`C_TIMER`](#gclass-c-timer) GClass — periodic timeout operations. |
| **`test_c_timer0`** | [`C_TIMER0`](#gclass-c-timer0) GClass — basic (low-level) timer. |

**Source:** `tests/c/c_timer/`, `tests/c/c_timer0/`

## TCP networking

Plain and TLS TCP through the full GObj protocol stack.

| Binary | Description |
|--------|-------------|
| **`c_tcp/test1–4`** | Plain TCP: connect, disconnect, echo, and rapid multi-message burst. |
| **`c_tcp/test5`** | A write that does not start (an empty gbuffer): `C_TCP` drops the connection, frees the write, and a stop reaches `ST_STOPPED`. |
| **`c_tcp2/test1–4`** | Same scenarios using the newer [`C_TCP_S`](#gclass-c-tcp-s) method (no `child_tree_filter`). |
| **`c_tcps/test1–4`** | TLS TCP: connect, disconnect, echo, and multi-message burst. |
| **`c_tcps2/test1–4`** | TLS TCP with the newer method. |
| **`c_tcp_inactivity/test1–4`** | `C_TCP`'s `timeout_inactivity`. |
| **`test_c_tcp_s_ip_lists`** | [`C_TCP_S`](#gclass-c-tcp-s) at accept: a peer in `denied_ips` is refused (with or without `only_allowed_ips`, and the deny wins over `allowed_ips`), with `only_allowed_ips` a peer not in `allowed_ips` is refused, loopback is exempt from both lists; the list key of each peername form (`1.2.3.4:80`, `[2001:db8::1]:443`, `[::ffff:1.2.3.4]:80`); the `add-`/`remove-` ip commands store the form a peer is looked up by (`2001:DB8::1` as `2001:db8::1`, `::ffff:203.0.113.7` as `203.0.113.7`, `fe80::2%lo` as `fe80::2%1`) and refuse what is not an ip; a link-local entry without its interface denies on every interface and is refused in `allowed_ips`; and entries stored as typed by 7.25.4 are renamed or dropped at load, with a warning, and saved. |

**Source:** `tests/c/c_tcp/`, `tests/c/c_tcp2/`, `tests/c/c_tcps/`, `tests/c/c_tcps2/`,
`tests/c/c_tcp_inactivity/`, `tests/c/c_tcp_s_ip_lists/`

## UDP networking

| Binary | Description |
|--------|-------------|
| **`test_c_udp_s_tx`** | [`C_UDP_S`](#gclass-c-udp-s) sends every datagram of its queue, in order, and drops one it cannot send -- no peer address (an error), refused by the kernel, a peer port `0` (a warning with the peer and `EINVAL`) -- leaking nothing, and goes on listening. |
| **`test_c_udp_s_restart`** | [`C_UDP_S`](#gclass-c-udp-s) stopped with a send in flight ends the stop with `EV_STOPPED`; started again after a file took the number of its old socket, it reads and sends again; stopped and started in the same turn (its read still canceling), it reads again. |
| **`test_c_udp_s_rx`** | The pieces of two long frames of two peers, interleaved, come out whole from [`C_GSS_UDP_S`](#gclass-c-gss-udp-s): every datagram is labelled with its peer. The yuno's ip lists: with `only_allowed_ips` a peer that is not allowed is dropped with a warning, a peer in `denied_ips` too (denied wins over allowed), an allowed one and the loopback are heard. A refused peer that insists is said once per cause, not per datagram, and the stat `rxRefusedMsgs` counts every drop. What a peer holds in a `C_GSS_UDP_S` is capped: a peer beyond `max_channels` is dropped (said once), a frame with no NUL within `max_frame_size` is delivered cut, the bytes of all the unfinished frames stop at `max_pending_bytes` (the frame of the peer that passes it is dropped), and the memory grows by far less than the 1 MB per source port of 7.25.5. |
| **`test_c_udp_s_echo`** | A host that answers IN the gbuffer of each [`C_UDP_S`](#gclass-c-udp-s) `EV_RX_DATA` (its kw sent back as `EV_TX_DATA`): with five sends waiting in the queue and two peers talking in the same turn, and with three datagrams read while each answer is in flight, every answer reaches its own sender, whole, with no error. Up to 7.25.4 the gbuffer was cleared and read into again: an answer was sent empty (dropped) or with the bytes and the peer of another datagram. |

**Source:** `tests/c/c_udp_s_tx/`, `tests/c/c_udp_s_restart/`, `tests/c/c_udp_s_rx/`, `tests/c/c_udp_s_echo/`

## TLS certificate hot-reload

Protects the [cert-reload feature](guide/guide_cert_management.md) — validates
that [`ytls_reload_certificates()`](#ytls_reload_certificates) swaps certificates atomically without
dropping live sessions and rolls back cleanly on invalid material.

| Binary | Description |
|--------|-------------|
| **`test_cert_reload`** | Swap cert A → B and make sure that `view-cert` shows the new subject and not_after. Then feed an invalid cert and make sure that the previous context stays intact (rollback). |
| **`test_cert_info`** | [`ytls_get_cert_info()`](#ytls_get_cert_info) edge cases: short / long validity, self-signed invariant, serial shape, client-side `NULL`, already-expired cert. |
| **`test_cert_reload_mem`** | 1000 reloads without any live session and asserts `get_cur_system_memory() == 0` (leak gate, run under [valgrind](https://valgrind.org/) for exhaustive checking). |
| **`test_yevent_reload_live`** | One reload while a TCP session is live. The session keeps working end-to-end. |
| **`test_yevent_reload_stress`** | 50 reloads with a live session, one echo message per iteration. |
| **`test_handshake_reject_openssl`**, **`test_handshake_reject_mbedtls`** | A server-side ytls that receives an HTTP request on its TLS port fails the handshake cleanly, on each backend: no crash, an error that says why. |
| **`test_tls_floor_openssl`** | The TLS protocol floor of the OpenSSL backend: a gate built with `ssl_min_version="TLS1.0"` logs the downgrade, and a peer below the floor is refused with a log. |
| **`test_tls_verify_openssl`** | Peer verification of the OpenSSL backend: a client with a CA checks the chain and the host name of the server, and a refusal is logged. |

**Source:** `tests/c/ytls/`, `tests/c/yev_loop/yev_events_tls/`

## HTTP parser (llhttp wrapper)

| Binary | Description |
|--------|-------------|
| **`test_c_llhttp_parser`** | Sanity test for the vendored llhttp library and the `ghttp_parser` wrapper: request/response parse, keep-alive pipelining, `HPE_PAUSED_UPGRADE` tail bytes, EOF completion via [`ghttp_parser_finish()`](#ghttp_parser_finish). |

**Source:** `tests/c/c_llhttp_parser/`

## OAuth2 / BFF (c_auth_bff)

Self-contained yunos that spin up a **mock Keycloak** (signed HS256 JWTs,
scriptable latency / response status / body override) alongside [`C_AUTH_BFF`](#gclass-c-auth-bff)
and drive `/auth/login`, `/auth/callback`, `/auth/refresh`, `/auth/logout`
through happy paths and failure modes. Each test is its own binary so a
crash or leak cannot mask neighbours.

| Binary | Description |
|--------|-------------|
| **`test1_login`** | Happy-path login round-trip through the BFF. |
| **`test2_kc_401`** | IdP returns 401. BFF must give a stable `error_code`, and it must not corrupt its `kc_ok` counter. |
| **`test3_callback`** | `/auth/callback` code-for-token exchange. |
| **`test4_refresh`** | Proactive refresh-token round-trip. |
| **`test5_logout`** | Logout clears cookies and drives the IdP revoke endpoint. |
| **`test6_invalid_body`** | Malformed JSON body → 4xx with `error_code` mapped for the GUI. |
| **`test7_slow_login`** | IdP replies slowly. The BFF makes the browser wait, and it does not time out early. |
| **`test8_queue_full`** | Pipeline 4 POSTs + overflow: `dl_list` pending queue drops the overflow in order and reports the configured `pending_queue_size`. |
| **`test9_browser_cancel`** | Browser disconnects in the middle of the round-trip. The BFF drops the stale IdP reply (`responses_dropped` counter) and does not forward it. |
| **`test10_kc_silence`** | IdP gives no answer. The `kc_timeout_ms` watchdog fires, answers 504 to the browser, and drains the task (`kc_timeouts` counter). |
| **`test11_cancel_retry`** | Cancel-then-retry: state cleanup between two back-to-back logins on the same browser. |
| **`test12_stale_reply`** | Per-task browser generation blocks cross-user token leak races. |
| **`test13_refresh_expired`** | Expired refresh token → mapped error, no silent `kc_error`. |
| **`test14_method_not_allowed`** | Non-POST on an auth endpoint → 405. |
| **`test15_missing_body`** | Missing body → 4xx with mapped error. |
| **`test16_unknown_endpoint`** | Unknown `/auth/*` path → 404, not 5xx. |
| **`test18_discovery_failure`** | The IdP's discovery document has `issuer` but no `token_endpoint`: the BFF refuses it and does not mark the discovery done. |
| **`test19_logout_no_cookie`** | `POST /auth/logout` with no refresh cookie is refused up front (401, `missing_refresh_token`), before any IdP call. |

**Source:** `tests/c/c_auth_bff/` (includes `c_mock_keycloak`, `test_helpers`).

`C_TASK_AUTHENTICATE` (the OIDC password-grant task) against a mock IdP:

| Binary | Description |
|--------|-------------|
| **`test1_discovery`** | Only `issuer` is given: discovery, token and logout run in order. |
| **`test2_explicit_endpoints`** | `token_endpoint` and `end_session_endpoint` are given: discovery is skipped. |
| **`test4_discovery_failure`** | A discovery body with no `token_endpoint` is refused: an error is logged and `EV_ON_TOKEN` carries `result=-1`. |

**Source:** `tests/c/c_task_authenticate/`.

## MQTT

| Binary | Description |
|--------|-------------|
| **`c_mqtt/test1`** | Self-contained broker + client: subscribe, publish QoS 0, verify reception, disconnect. |
| **`c_mqtt/acl`** | The publish/subscribe ACL of the broker (`EV_MQTT_ACL_CHECK`); `list-queues queue=<name>` of a queue that cannot be opened answers `-1`, not an empty queue; and `list-queues` / `clean-queues` of a store whose topics cannot be listed answer `-1` with a cause of their own. |
| **`c_mqtt/malformed`** | Malformed MQTT packets from a peer. |
| **`c_mqtt/queued_in`** | A raw MQTT client: the incoming QoS 2 messages of a persistent session reloaded beyond `max_inflight_messages` wait queued, and each goes in flight when a PUBREL frees a slot, with its OWN packet id (`PUBREC 3, PUBCOMP 1, PUBREC 4, PUBCOMP 2, PUBCOMP 3, PUBCOMP 4`), as in mosquitto. Up to 7.25.4 the quota test was inverted and a NEW id was sent: the client's PUBREL found nothing, and the message was never released. |

**Source:** `tests/c/c_mqtt/`

## Subscriptions & link events

| Binary | Description |
|--------|-------------|
| **`c_subscriptions/test1–3`** | GObj event subscribe / unsubscribe lifecycle. `test3`: a repeated HARD subscription is one (the second [`gobj_subscribe_event()`](#gobj_subscribe_event) returns the one there with a warning; each event arrives once), [`gobj_unsubscribe_event()`](#gobj_unsubscribe_event) leaves it with a warning, [`gobj_unsubscribe_list()`](#gobj_unsubscribe_list) with `force` removes it. |
| **`test_c_node_link_events`** | `EV_TREEDB_NODE_LINKED` / `UNLINKED` events at the [`C_NODE`](#gclass-c-node) GClass level, and an `update-node` with `autolink`: the links it repeats publish nothing, a moved link is one unlink and one link, and a ref that cannot be linked (missing parent, or a hook that links into another column) is logged while the record is still saved; and `set-link-events` switching a live treedb from the parent's `UPDATED` to `LINKED`/`UNLINKED` and back; and `delete-node` of a key with `pkey2` instances, reopened: a lookup of the primary's value is the primary, a refused delete leaves every instance. |
| **`test_c_node_initial_load`** | The `initial_load` seed of [`C_NODE`](#gclass-c-node): records first and links second whatever the topic order, every seed immutable, a declared link refused to `unlink-nodes`, to an autolink update that omits it and to a `force` delete of its parent, and a second start that creates nothing. |
| **`test_tr_treedb_link_events`** | Low-level link/unlink callback mechanism in `tr_treedb`. |
| **`test_tr_treedb_failed_save`** | A write whose save fails is taken back in memory: with the files of a key read-only, a failed update, link, replace of a single fkey, `treedb_replace_links()`, `treedb_autolink()` and unlink each answer a refusal, leave the nodes and hooks as the disk says, and tell no event. With the files writable the same writes work, and a reload says what memory said. Also: the update with its links (`treedb_update_node_and_links()`, with and without fields); a failed clean, where a stale ref (its hook no longer exists, or fills another column since a schema re-pointed it) goes back into its field and is never linked; refused forced deletes (a child that cannot be saved, a key that cannot be deleted) that change nothing, in memory or on disk; a take-back that puts every child back in its place in the hooks of its parents (a list hook and a dict hook keep the order they had); `treedb_autolink()` refusing a ref whose hook fills another column, with nothing moved; a refused forced delete whose put-back of a child fails too (the child stays unlinked, in memory as on disk, the ERROR says so, and the events of its unlink are told; the test fails the writes of that key in its own `__wrap_write()`); a forced delete that tells its unlinks after the node left the indexes, with a subscriber that cannot find the node nor save it, and a reload that does not bring it back; a write that cannot be taken back whole says so; and two active snaps at open (a deactivation that cannot be saved leaves the snap active; a replica does not repair). |
| **`test_c_node_failed_save`** | An `update-node` with `autolink` of [`C_NODE`](#gclass-c-node) whose save fails (the writes of the key fail in the test's own `__wrap_write()`): the method answers `NULL` and the command `-1`, memory is what the disk has, and no `EV_TREEDB_NODE_LINKED` / `UPDATED` is published. A create with `autolink` whose links cannot be saved leaves the node without them, in memory as on disk, and logs *"Node created, but its links cannot be saved (autolink): the node stays without them"*. A reload from disk agrees, and the retry works. |
| **`test_c_subscription_authz`** | The subscription authorization gate (`enable_subscription_authz`): three `C_IEVENT_CLI` connect over websocket to a `C_IEVENT_SRV` of the same yuno and subscribe to the `EV_TREEDB_NODE_*` feed of a real [`C_NODE`](#gclass-c-node). Gate off: every peer is subscribed. Gate on: a peer without `read` is refused and it is logged, a `reader` is subscribed (the checker is asked `read`), an event not flagged `EVF_AUTHZ_SUBSCRIBE` needs nothing; a node update reaches only the accepted subscriptions; the realtime feed of a real [`C_TRANGER`](#gclass-c-tranger) (`EV_TRANGER_RECORD_ADDED`) is guarded the same way (open to any user up to 7.25.4); the six channel commands of `C_IOGATE` with a `channel_name` that matches nothing answer with no channel (up to 7.25.4 they looped for ever); the `authzs` trace prints the kw it checked (up to 7.25.4 a kw already freed); a peer that asks `__hard_subscription__`, `__own_event__` and `__rename_event_name__` gets a plain subscription (only `__first_shot__` is kept), which goes with its session and reaches neither a later subscriber's loop nor the next user of the channel; every channel disabled and enabled again serves a new session; and the gate stops orderly (no *"Destroying a RUNNING gobj"*). |
| **`test_c_node_authz`** | The permission every [`C_NODE`](#gclass-c-node) command asks for, reads and writes, with a checker that refuses one principal: a refused command answers `-403` and does nothing. |
| **`test_c_ievent_srv_peer_subs`** | What a remote peer may put in a subscription, and hold, through `C_IEVENT_SRV`: one peer writes its frames by hand, with a `__global__` that forges keys of the event and sets framework keys and a `gbuffer`, a `__local__` and a stray key; a plain client subscribes with a `__filter__` and a `__global__`; two local subscribers, one with a `__local__` and a `__global__`. Each gets what IT asked and nobody else's (up to 7.25.4 the publish shared one kw: the plain client got nothing, the later local subscriber got the forgery without the secret and the peer's `__md_iev__`, and the `gbuffer` crashed the yuno), and the publisher's kw comes back as it went. `max_subscriptions` and `max_subscription_size` refuse; the dropped keys, the caps, a withdrawal of nothing (repeated, with a 2 KB key: one line, capped) and a repeated authz refusal are each logged once; a client withdraws a subscription with a `__global__`. |
| **`test_c_node_paged_nodes`** | The paging of `C_NODE`'s `nodes`: no `limit` answers the plain list; a `limit` answers `{total_rows, pages, data}`; `from` is 1-based; a page past the end is empty with the right total; `from` / `limit` given as strings work. |
| **`test_c_tranger`** | [`C_TRANGER`](#gclass-c-tranger) commands: `topics`, `list-keys` (counts and time spans), `open-iterator` and its pages by `t` and by `tm`. `add-record` appends (a dict or its json text, `write`, master-only; it was a stub). The pushes of a live list carry its `list_id` as `rt_id`. A handle of one session is refused to another (`close-rt`, `close-iterator`, `close-list`, `get-page`, `get-list-data` answer `-403`). `mark-tm-order` and `add-record` ask `write` and answer `-403` on a refusal (a counting authz checker). |
| **`test_c_agent_find_new_yunos`** | The rows of the agent's `find-new-yunos`, against a real [`C_NODE`](#gclass-c-node) with the agent's schema: a yuno already registered at the new release (a single one, and a `yuno_multiple` one) is marked *"already registered, pending promotion (deactivate-snap)"*, not listed as new. |

**Source:** `tests/c/c_subscriptions/`, `tests/c/c_node_link_events/`,
`tests/c/c_node_initial_load/`, `tests/c/tr_treedb_link_events/`,
`tests/c/tr_treedb_failed_save/`, `tests/c/c_node_failed_save/`,
`tests/c/c_subscription_authz/`, `tests/c/c_ievent_srv_peer_subs/`, `tests/c/c_agent_find_new_yunos/`,
`tests/c/c_node_authz/`, `tests/c/c_node_paged_nodes/`, `tests/c/c_tranger/`

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
| **`test_delete_instance`** | `tranger2_delete_instance()`: iterators skip a dead row, also after a cold reload; the zero wipe of the content. |
| **`test_delete_key_propagation`** | `tranger2_delete_key()` and who hears it (rt_mem and rt_disk feeds, a real follower); a key directory that cannot be removed keeps its filtered iterators. A key directory whose `stat()` fails with `EIO` is not "not found": `-1`, nothing deleted or announced. A `disks/` whose `opendir()` or `readdir()` fails is logged (by `--wrap=stat,opendir,readdir`). |
| **`test_rt_disk_multi_feed`** | Several rt_disk feeds on one key of a follower: each wake-up serves only the feed whose directory fired. |
| **`test_out_of_order_append`** | An append whose `__t__` belongs to an earlier file of the key goes into that file's cell; every record is served once. |
| **`test_pkey_path_traversal`** | A string primary key is one directory component: a key with `/` or a leading `.` is refused. |
| **`test_topic_path_traversal`** | A topic name is one directory component: `.`, `..`, a `/` or a `` ` `` is refused, and a replica cannot delete or back up a topic. |
| **`test_read_never_exits`** | A failed READ never ends the process, whatever `on_critical_error` says. |
| **`test_iterator_index`** | The id index of a topic's iterators: found by `(id, creator)`, a closed one is not, a duplicate is refused. |
| **`test_late_record`** | A record whose `__t__` is below the times of its md2 file is served by a time-range query. |
| **`test_stop_reopen`** | A tranger stopped and used again: no fd closed twice, a topic opened again revives it. |
| **`test_append_md_contract`** | `tranger2_append_record()` drops a `__md_tranger__` the record carries in, and returns the new metadata in the out-param. |
| **`test_str2system_flag`** | Every name of `tranger2_str2system_flag()` maps to its own bit. |
| **`test_tm_order`** | A `__tm__` that does not grow with `__t__`: segments with holes, the tm range of a file from all its rows, the tm order markers. |
| **`test_lost_lock`** | A master that lost its lock while stopped writes nothing; it takes the lock again or goes on as a replica. |
| **`test_topic_var_replace`** | `topic_var.json` is replaced through a `.new` file and a rename, never truncated. |
| **`test_key_reborn_pages`** | An iterator open while its key is deleted and written again forgets the old segments. |
| **`test_open_list_history`** | A keyless `tranger2_open_list()` names a key that does not load in `load_failed_keys` and goes on. |
| **`test_unreadable_at_open`** | A store damaged before the start: the key is flagged at the cache build, and every load says `load_failed`. |
| **`test_mark_tm_order`** | `tranger2_mark_tm_order()` that fails half way, and a file name with no room for a marker. |
| **`test_uncommitted_append`** | An append whose md2 row is never written: ignored with a warning, cut back, a short write logged as such. |
| **`test_torn_md2_tail`** | An md2 that ends in a part of a row: cut back by a master, read whole by a replica; the shape 7.25.4 left after a torn row is flagged, not cut. |
| **`test_md2_short_write`** | An md2 row written short whose cut back fails: the append is refused, and the next one cuts and goes in. |
| **`test_md2_read_error`** | A read of the first or last md2 row fails at the cache build: the file is flagged until it reads again. |
| **`test_nul_escape_record`** | A record whose string holds NUL characters is read back, also after a restart. |
| **`test_torn_tail_check_fails`** | A check of a torn tail that cannot RUN refuses the append and does not flag the file; records larger than the reader's memory block are flagged, not cut. |
| **`test_cmp_file_ids`** | `cmp_file_ids()` orders two md2 file ids as their names sort, for fixed cases and 2 000 000 random pairs. |
| **`test_unlistable_dirs`** | A key directory or `keys/` that cannot be listed: the key is flagged, the topic does not open, and both recover once the directory can be listed. |
| **`test_unlisted_relist_once`** | A key directory whose `readdir()` fails is flagged once, and listed again only when it changes. |
| **`test_tr_dt_unknown`** | A topic on a filesystem whose `readdir()` gives no `d_type`: the keys are found with `stat()` in `keys/`, and the cache is whole. A key whose `stat()` fails with `EIO` fails the listing, and the topic does not open (by `--wrap=stat`). |

**Source:** `tests/c/timeranger2/`, `tests/c/tr_dt_unknown/`. Each case of
these tests is described in `tests/c/timeranger2/README.md`.

## TR_MSG & TR_QUEUE

| Binary | Description |
|--------|-------------|
| **`test_tr_msg1`** | Message topics: iteration, key matching, instance retrieval. |
| **`test_tr_msg2`** | Stress variant: 1 000 devices × 100 traces. |
| **`test_tr_queue1`** | Queue topic: enqueue / dequeue with time-based keys over a multi-day period. |
| **`test_tr_queue_backup_failed`** | A backup that fails (the backup name taken by a file): `trq_check_backup()` / `tr2q_check_backup()` answer `-1`, the queue keeps its topic, and appends, reads and acks work after it; nothing leaks. And a backup whose new topic cannot be created after the move (its `mkdir` fails, `ENOSPC`): the backup is moved back, the queue keeps its topic whole, and the next call backs it up. A create that fails only at the `mkdir` of its `keys/` answers `NULL` and leaves nothing, and a backup that meets it keeps the queue's topic. A backup that fails and cannot open the topic again (`topic_desc.json` of mode `0` for a moment): the queue has no topic, a read and an ack fail (said once), and the queue takes its topic again by name as soon as it can be opened, and backs up at the next call (`tr_queue` and `tr2q`). While the topic still cannot be opened, the next calls log nothing. And a tranger with `on_critical_error` `LOG_OPT_EXIT_ZERO` (the broker's queues): a backup whose create fails does not exit, and moves the backup back; a plain create whose `keys/` cannot be made exits (in a child process) only after it removed what it made, and the next create makes the topic whole. |
| **`test_tr_queue_load_failed`** | A queue (`tr_queue` and the mqtt `tr2q`) whose load cannot read every pending message: the load returns `-1`, `first_rowid` is kept, and the backup is refused until a load reads them all. |
| **`test_pkey2_empty`** | `msg2db_append_message()` refuses a record whose `pkey2` is empty and writes nothing. |
| **`test_msg2db_load_failed`** | An id whose history did not load whole: reloaded newest first up to the damage; a `pkey2` whose newest message is in the damage is absent (`msg2db_id_incomplete()`), never an older message. |

**Source:** `tests/c/tr_msg/`, `tests/c/tr_queue/`, `tests/c/tr_msg2db/`

## TreeDB

| Binary | Description |
|--------|-------------|
| **`test_tr_treedb`** | Schema creation, user/department/compound structures, node CRUD, and state snapshots (foto files). |
| **`test_tr_treedb_delete_instance`** | Instances of a `pkey2` key, each scenario in its own database with a reopen: `treedb_delete_instance()` and a tombstone that fails; a delete of a parent sees the children of every instance, a delete of a child unhooks every instance, a deleted instance leaves the hooks of its parents (the primary takes its place) and hands its children to the primary, a refused forced delete puts every instance back, a save of a node no index holds is refused, and after a reopen the instance of the primary IS the primary node (an update through it lands, and survives the next save of the primary); a save of a node whose `pkey2` value changed in place is refused, and a create indexes a defaulted `pkey2` by the value its record holds. The instances of a CHILD: an unlink clears the ref in every instance of the child key, a delete of a parent sees (without `force`) and clears (with it) the instances that name it and no hook holds, a forced delete leaves no ref in any instance, what those did is put back when the write does not go, and a relink of an instance its parent does not hold logs nothing. With a snap active, a key created after it (instances and no primary) is deleted quietly, and its create takes the slot; with two `pkey2`s a save keeps the primary in its slot; the gc holds the asset an instance names. |
| **`test_tr_treedb_load_failed`** | A topic whose keys cannot all be read: treedb loads the other keys, remembers the ones that failed, and refuses a create of such an id, and snapshot operations when `__snaps__` did not load whole. The recovery, after the key is deleted, needs no reopen. |
| **`test_tr_treedb_failed_save`** | See *Subscriptions & link events*. |
| **`test_tr_treedb_files`** | The `file` column and `__assets__`: the bytes are not in the record, the id is the hash of the bytes (a client that lies is refused), and the index is in memory. |
| **`test_tr_treedb_hook_hygiene`** | Hooks of a versioned (`pkey2`) parent: an unlink through the reverse hook, a link made twice kept once, a forced delete of a parent with many children. |
| **`test_tr_treedb_hook_rename`** | A hook renamed in the parent's schema: the child's `fkey` mark is derived, never read from disk, so the links load under the new name; a ref that names the old hook, in an array or a STRING fkey column, is removed by a clean or a forced delete. |
| **`test_tr_treedb_immutable`** | Immutable topics and records: a delete is refused, also with `force`, and the mark survives updates and a reload. |
| **`test_tr_treedb_relink`** | A link into a single-valued fkey replaces the old one, and the old parent's hook forgets the child. |
| **`test_tr_treedb_rowid`** | The id a `rowid` topic hands out is never one that exists, also after deletes and updates. |
| **`test_tr_treedb_schema_parse`** | `parse_schema()` of a column with no `flag` (it crashed). |
| **`test_tr_treedb_snap`** | `treedb_shoot_snap()`, `treedb_activate_snap()` and `__clear__`, and the primary promoted at the next reload. |
| **`test_tr_treedb_snap_clone`** | A record already tagged by a snap gets a clone for the new snap, and memory agrees with a reload. |
| **`test_tr_treedb_update_instance`** | An update is seen through the `pkey2` index at once (`treedb_get_instance()`, `treedb_list_instances()`). |
| **`test_c_assets`** | [`C_ASSETS`](#gclass-c-assets): the asset id is the sha256 of its bytes, the same bytes are one asset, `get-asset` inline or as a signed url, orphans, `import-assets`, `gc-assets` and its refusal while a snap is active, the `-403` of the authz gate. |
| **`test_c_treedb_system_schema`** | `__system__`, the meta-treedb of [`C_TREEDB`](#gclass-c-treedb): the projection of a schema at open, `save-schema`, `apply-schema`, `delete-treedb`, and the warnings of a store that runs a topic ahead of its file. |
| **`test_treedb_schema_fidelity`** | Every real schema of the tree through `__system__` and back: each attribute the literal declares comes back with the same value. |
| **`test_c_treedb_literal_wins`** | [`C_TREEDB`](#gclass-c-treedb): a schema from C newer than the schema file in use wins whole; `__system__` is projected from it whole; the operator work it discards is reported once (`withdrawn_at_open`); an unfinished projection is recorded and completed at a later open, also after the process is killed at any write, once or twice. The scenarios are listed in its `README.md`. |

**Source:** `tests/c/tr_treedb/`, `tests/c/tr_treedb_delete_instance/`, `tests/c/tr_treedb_load_failed/`,
`tests/c/c_treedb_literal_wins/`, `tests/c/tr_treedb_*/`, `tests/c/c_assets/`,
`tests/c/c_treedb_system_schema/`, `tests/c/treedb_schema_fidelity/`

## gobj-c helpers and commands

| Binary | Description |
|--------|-------------|
| **`test_helpers`** | String, file and directory helpers: `split2()`, `save_json_to_file()`, `rmrdir()` / `mkrdir()` with links, deep trees and entries that vanish during the walk. |
| **`test_rotatory`** | `rotatory_remove_old_files()` and the rotatory log writer: size rotation, a full disk, a clock set back, a write that fails and the file opened again (no newfile callback for the same file), a keep_all size rotation whose rename fails (tried once, every record kept), a fixed name never emptied, an `MM` mask judged by its month; a new file whose open fails keeps its newfile callback for the next open that works (on a full disk too), the file of the day before is never size-rotated at a new name, and the keep_all rename retry runs on the monotonic clock (a wall clock set back or forward does not move it). The size limit is in bytes: a file of exactly the limit is not rotated, one 2 bytes over it is at the next record. `exit_on_fail` is for [`rotatory_open()`](#rotatory_open) only: with `TRUE`, a later open that fails (a new day, a removed file, the reopen after a failed write, a truncate) prints one line, is tried again at the next record and never exits (each case in a child process); the open itself still exits with `TRUE`, and returns `NULL` with `FALSE`. |
| **`test_audit_record`** | The audit record builder of `yuneta_agent`: secrets redacted, `content64` never written, the command word read as the parser reads it (`COMMAND=list-yunos` with a kw `command=delete-yuno` gets the full record), a secret inside a JSON text inside a JSON text redacted (up to 8 levels of escapes; deeper is written as its size and sha256), whatever quotes come before it or cut it (`x="{\"password\":…}"`, `'{\"password\":…}'`, `password=\"two words\"`), and the credentials after `Basic `; an escaped key with a blank or a slash (`\"secret key\":`), a quote inside a quoted secret (`password="a\" S"`, the shell's `'\''`), a JWT before a `.`; a deterministic fuzz of 20 000 generated commands (the secret in 23 quoting shapes among 17 kinds of noise, keys with blanks and slashes) writes no secret; hard shapes stay linear. |
| **`test_build_path`** | [`build_path()`](#build_path): segments joined with one `/`, and a `..` clamped at the root (no path escapes the base). |
| **`test_gbuffer_guards`** | A NULL gbuffer given to its pointer accessors answers NULL, not a crash; bytes from a peer that are not json are a warning (`gbuf2json_from_peer()`). |
| **`test_glogger_utf8`** | The logger never writes a record that is not valid UTF-8 JSON: an invalid byte is escaped as `\u00XX`, valid multibyte text stays readable. |
| **`test_command_authz`** | The per-command authz gate (`SDF_AUTHZ_X`): off by default, a command without the permission is refused `-403` when on, a command to itself and a granted one run. |
| **`test_command_delete_user`** | `delete-user` of `C_AUTHZ`: a user with roles needs `force`, an immutable seed user is never deleted. |
| **`test_command_shutdown`** | `shutdown` answers first and stops the yuno after the answer left. |
| **`test_jwt_alg_confusion`** | The JWT algorithm-confusion forgery: an `HS*` token verified with the bytes of an RSA/EC public key is refused; the `jwks_*` keyring API accepts a NULL set, `jwks_item_count`, `jwks_find_bykid` (and a NULL kid), `jwks_item_add` and `jwks_item_free_bad` included (not guarded upstream either). |
| **`test_gbmem_realloc_refused`** | A `gbmem_realloc()` refused because the new size is larger than the largest block leaves the old block valid and tracked: the later free logs nothing and the memory counter stays right. |
| **`test_command_binary_kw`** | A command, and `build_stats()`, whose kw carries a `gbuffer`: the handler's kw holds a reference of its own, and the caller's references are intact after the command (no *"BAD gbuf_decref()"*). |
| **`test_dir_array_nomem`** | A directory listing that cannot keep an entry (no memory), or whose root cannot be opened, answers `-1` with the listing empty, and logs it: `find_files_with_suffix_array()`, `walk_dir_array()`, `get_ordered_filename_array()`. |
| **`test_dir_listing`** | The answer of the agent's `dir-*` commands: a tree that cannot be listed answers `-1` with a comment that names the directory and sends to the log (it does not read the process-global last message), never an empty list. |
| **`test_dir_read_error`** | A directory that opens and cannot be READ (`readdir()` fails, `EIO`, by a `--wrap=readdir`) fails the listing: `find_files_with_suffix_array()`, `walk_dir_array()` (root or subdirectory) and `walk_dir_tree()` answer `-1`, empty, logged. `re`/`pattern` `NULL` lists every entry; a root of mode `0` given to `walk_dir_tree()` is logged. A SUBdirectory whose `opendir()` fails with `EMFILE` (a transient cause, by a `--wrap=opendir`) fails the walk; one with `EACCES` is skipped with a warning. A callback that returns `FALSE` in a subdirectory stops the WHOLE walk. Paths longer than `PATH_MAX` fail the walk (*"Path too long"*; up to 7.25.4 a crash), also in `find_files_with_suffix_array()` without `d_type` (the `readdir()` wrap hides it); a tree of 1100 levels fails it too. An entry whose `lstat()` fails with `EIO` fails the walk and `find_files_with_suffix_array()` without `d_type` (by a `--wrap=lstat`); a subdirectory whose `opendir()` fails with `ENOTDIR` or `ELOOP` is skipped with a warning. `rmrcontentdir()` and `rmrdir()` of a directory whose `readdir()` fails answer `-1` and log *"readdir() FAILED"*. |

**Source:** `tests/c/helpers/`, `tests/c/gbuffer/`, `tests/c/command_binary_kw/`,
`tests/c/build_path/`, `tests/c/glogger_utf8/`, `tests/c/command_authz/`,
`tests/c/command_delete_user/`, `tests/c/command_shutdown/`, `tests/c/libjwt/`

## Keyword matching

| Binary | Description |
|--------|-------------|
| **`test_kw1`** | `kw_match_simple()` with strings, booleans, integers, and floats. |
| **`test_json_flat`** | [`json2flat()`](#json2flat) / [`flat2json()`](#flat2json): one row per leaf, keys with `` ` `` and `[`, empty containers as leaves, and the ids refused when one is a leaf and a container at once. |
| **`test_kw_set_dict_value`** | [`kw_set_dict_value()`](#kw_set_dict_value): the value REPLACES what the path holds (a leaf, a dict, an item of a list), the dicts of the path are created, and nothing is written -- `-1`, the kw as it was -- when a segment is a scalar or `null`, an index is out of its list, or the path is empty. The value is owned in every case. |

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

## Which library a test runs

A test binary links the **installed** archives in `outputs/lib`, not the ones
in the build tree. So a change in kernel source reaches a test only after the
library is **installed**, and then the next build relinks the test:

```bash
# 1. change kernel/c/timeranger2/src/timeranger2.c, then install the library
cd kernel/c/timeranger2/build && make install
#   -- Installing: .../outputs/lib/libtimeranger2.a

# 2. the next build of the test relinks it against the new archive
cmake --build build --target test_tr_treedb_rowid
#   [  0%] Linking C executable test_tr_treedb_rowid
```

`cmake --build build --target <test>` alone, without step 1, rebuilds the
library in `build/` but links the test against the OLD installed copy.

Before 7.25.5 step 2 could also print only *"Built target"* and keep the old
library. `install()` keeps the mtime of the file it copies, in whole seconds,
which is the time the archive was BUILT: a test linked between that moment
and the install looked newer than the archive. `tools/cmake/project.cmake` now
stamps an archive that changed with the time of its install, rounded up to
the next whole second. An archive that did not change is not copied
(*"Up-to-date"*) and relinks nothing.

## Debugging a test that corrupts the heap

A test that dies with *"corrupted double-linked list"* or *"free(): chunks in
smallbin corrupted"* is reporting heap corruption that happened **earlier**: the
abort lands at the next `malloc`/`free`, not at the write that caused it. The
backtrace at that point names the detector, not the culprit.

**AddressSanitizer finds it, but not by simply building with it.** The test
tree links the **installed** libraries from `outputs/lib`, not the ones in the
build tree, so configuring a build with `-fsanitize=address` instruments the
test's own `.c` files and nothing else — and ASan reports nothing while the
plain build keeps aborting, which reads as "it is not a real bug" and is only
"you did not look at it".

What actually works, and what it costs:

```bash
# 1. a non-static build, because -static and -fsanitize=address are exclusive
cp .config .config.bak && sed -i 's/^CONFIG_FULLY_STATIC=y/CONFIG_FULLY_STATIC=n/' .config

# 2. configure a separate tree WITH the sanitiser
cmake -S . -B build_asan -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-fsanitize=address -g -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"

# 3. build the LIBRARIES too, not only the test
cmake --build build_asan --target yunetas-gobj timeranger2 yev_loop ytls \
    yunetas-core-linux <the test> -j8

# 4. relink the test by hand against those libraries: take its link.txt and
#    replace every /outputs/lib/lib*.a with the build_asan path
```

**And instrument jansson as well** when the corruption is around json: it lives
in `outputs_ext/lib` and is not part of the build. Compile
`kernel/c/linux-ext-libs/build/jansson/src/*.c` with the sanitiser **and with
its own generated headers** — `-DHAVE_CONFIG_H -I<jansson>/build/private_include
-I<jansson>/build/include` —, or it builds but parses nothing and every test
fails with *"Cannot load json file, bad json"*, which looks like a different bug
entirely.

With all of it instrumented, ASan names the two lines that matter — where the
memory was freed and where it was used afterwards — and a hunt that had taken
hours takes a minute.

Restore `.config` afterwards.

### The mistake it usually is

Three bench tests died this way on 2026-08-28, and all three were the same
thing: **a borrowed pointer handed to a function that owns what it is given**.

`json_array_get()`, `kw_get_dict()`, `treedb_get_node()` and friends answer a
pointer they still own; [`test_json()`](api/testing/testing.md#test_json)
**frees what it receives**. Passing one to the other spends a reference that was
never given, and the next `json_decref()` of the container frees the same block
twice. Incref it at the call:

```C
result += test_json(json_incref(record));   // record is borrowed
```

## Benchmarks run by ctest

`ctest` also runs the benchmarks of `performance/c/`, with small sizes, so a
benchmark that breaks is seen: `perf_c_tcp/test4`, `perf_c_tcp/test5`,
`perf_c_tcps/test4`, `perf_c_tcps/test5`, `perf_yev_ping_pong`,
`perf_yev_ping_pong2`, `perf_auth_bff`, `perf_timeranger2`, `perf_tr_treedb`,
`perf_c_treedb` and `perf_rotatory`. Their figures are in
`performance/c/README.md`.

```bash
ctest -R '^perf_' --output-on-failure --test-dir build
```

## Build flag

Tests are enabled by default via `.config` (`CONFIG_MODULE_TEST=y`).
Toggle with `menuconfig` or pass `-DENABLE_TESTS=OFF` to CMake.

## Related

- **Performance benchmarks** live under `performance/c/` — `perf_c_tcp`, `perf_c_tcps`, `perf_yev_ping_pong`, `perf_yev_ping_pong2`, **`perf_auth_bff`** (live throughput over the BFF in the ping-pong style: a run is 10 s by default, about 180 000 ops on the reference box), and the persistence ones: **`perf_timeranger2`** (the open of a store, the create of topics, a tm query before and after `tranger2_mark_tm_order()`), **`perf_tr_treedb`** (treedb writes), **`perf_c_treedb`** (the open of a dynamic-schema treedb by `C_TREEDB` in a store of 40 treedbs) and **`perf_rotatory`** (one record of the log files and of the agent audit). Each prints one line of JSON per result; ctest runs them small, and `performance/c/README.md` keeps the figures of each release.
- **Stress runners** live under `stress/c/` — `stress/auth_bff` drives concurrent BFF login / refresh / logout cycles to expose races between the pending queue, the watchdog and the flush-on-disconnect path.
