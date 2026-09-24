# Test Suite

Functional and integration tests that verify correctness of every layer —
from raw [io_uring](#io-uring) events up to full GObj protocol stacks.
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
| **`test_yevent_loop_end_drain`** | A loop ended with destroyed events whose completions have not come: `yev_loop_destroy()` frees each of them, and one whose completion never comes after 1 second, with an ERROR. |
| **`test_yevent_connect_src_url`** | A connect bound to a local address (`src_url`): `127.0.0.1:<port>`, `[::1]:<port>` and `tcp://127.0.0.1:<port>`; the listener sees the peer at that port, and a bad `src_url` (`[::1:5000`) gives no socket and an error. |
| **`test_yevent_stop_nomem`** | A stop without memory for its cancel answers `-1` and gives up nothing: the timer keeps its fd, the read its gbuffer, and the read then completes with its data. A loop destroyed with no room for the cancel of its dying events says so before the final error. |
| **`test_yevent_kept_after_post`** | A posted action (`gobj_post_event()`) stops a timer whose start is kept: the `STOPPED` reaches the callback at the next cycle, not at the timeout of the run. |

**Source:** `tests/c/yev_loop/yev_events/`, `tests/c/yev_loop/yev_events_tls/`

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
| **`test_c_tcp test1–4`** | Plain TCP: connect, disconnect, echo, and rapid multi-message burst. |
| **`test_c_tcp2 test1–4`** | Same scenarios using the newer [`C_TCP_S`](#gclass-c-tcp-s) method (no `child_tree_filter`). |
| **`test_c_tcps test1–4`** | TLS TCP: connect, disconnect, echo, and multi-message burst. |
| **`test_c_tcps2 test1–4`** | TLS TCP with the newer method. |

**Source:** `tests/c/c_tcp/`, `tests/c/c_tcp2/`, `tests/c/c_tcps/`, `tests/c/c_tcps2/`

## UDP networking

| Binary | Description |
|--------|-------------|
| **`test_c_udp_s_tx`** | [`C_UDP_S`](#gclass-c-udp-s) sends every datagram of its queue, in order, and drops one it cannot send (no peer address) with an error, leaking nothing. |

**Source:** `tests/c/c_udp_s_tx/`

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
| **`test_c_node_link_events`** | `EV_TREEDB_NODE_LINKED` / `UNLINKED` events at the [`C_NODE`](#gclass-c-node) GClass level, and an `update-node` with `autolink`: the links it repeats publish nothing, a moved link is one unlink and one link, and a ref that cannot be linked (missing parent, or a hook that links into another column) is logged while the record is still saved; and `set-link-events` switching a live treedb from the parent's `UPDATED` to `LINKED`/`UNLINKED` and back. |
| **`test_c_node_initial_load`** | The `initial_load` seed of [`C_NODE`](#gclass-c-node): records first and links second whatever the topic order, every seed immutable, a declared link refused to `unlink-nodes`, to an autolink update that omits it and to a `force` delete of its parent, and a second start that creates nothing. |
| **`test_tr_treedb_link_events`** | Low-level link/unlink callback mechanism in `tr_treedb`. |
| **`test_tr_treedb_failed_save`** | A write whose save fails is taken back in memory: with the files of a key read-only, a failed update, link, replace of a single fkey, `treedb_replace_links()`, `treedb_autolink()` and unlink each answer a refusal, leave the nodes and hooks as the disk says, and tell no event. With the files writable the same writes work, and a reload says what memory said. Also: the update with its links (`treedb_update_node_and_links()`, with and without fields); a failed clean, where a stale ref (its hook no longer exists, or fills another column since a schema re-pointed it) goes back into its field and is never linked; refused forced deletes (a child that cannot be saved, a key that cannot be deleted) that change nothing, in memory or on disk; a take-back that puts every child back in its place in the hooks of its parents (a list hook and a dict hook keep the order they had); `treedb_autolink()` refusing a ref whose hook fills another column, with nothing moved; a refused forced delete whose put-back of a child fails too (the child stays unlinked, in memory as on disk, the ERROR says so, and the events of its unlink are told; the test fails the writes of that key in its own `__wrap_write()`); a forced delete that tells its unlinks after the node left the indexes, with a subscriber that cannot find the node nor save it, and a reload that does not bring it back; a write that cannot be taken back whole says so; and two active snaps at open (a deactivation that cannot be saved leaves the snap active; a replica does not repair). |
| **`test_c_node_failed_save`** | An `update-node` with `autolink` of [`C_NODE`](#gclass-c-node) whose save fails (the writes of the key fail in the test's own `__wrap_write()`): the method answers `NULL` and the command `-1`, memory is what the disk has, and no `EV_TREEDB_NODE_LINKED` / `UPDATED` is published. A create with `autolink` whose links cannot be saved leaves the node without them, in memory as on disk, and logs *"Node created, but its links cannot be saved (autolink): the node stays without them"*. A reload from disk agrees, and the retry works. |

**Source:** `tests/c/c_subscriptions/`, `tests/c/c_node_link_events/`,
`tests/c/c_node_initial_load/`, `tests/c/tr_treedb_link_events/`,
`tests/c/tr_treedb_failed_save/`, `tests/c/c_node_failed_save/`

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

The tests added after 7.25.4 (tm order, lost lock, torn md2 tails, NUL in strings, short reads and writes, file ids compared in place (`test_cmp_file_ids`), directories that cannot be listed (`test_unlistable_dirs`), ...) are listed in `tests/c/timeranger2/README.md`.

## TR_MSG & TR_QUEUE

| Binary | Description |
|--------|-------------|
| **`test_tr_msg1`** | Message topics: iteration, key matching, instance retrieval. |
| **`test_tr_msg2`** | Stress variant: 1 000 devices × 100 traces. |
| **`test_tr_queue1`** | Queue topic: enqueue / dequeue with time-based keys over a multi-day period. |
| **`test_tr_queue_backup_failed`** | A backup that fails (the backup name taken by a file): `trq_check_backup()` / `tr2q_check_backup()` answer `-1`, the queue keeps its topic, and appends, reads and acks work after it; nothing leaks. |
| **`test_tr_queue_load_failed`** | A queue (`tr_queue` and the mqtt `tr2q`) whose load cannot read every pending message: the load returns `-1`, `first_rowid` is kept, and the backup is refused until a load reads them all. |
| **`test_pkey2_empty`** | `msg2db_append_message()` refuses a record whose `pkey2` is empty and writes nothing. |
| **`test_msg2db_load_failed`** | An id whose history did not load whole: reloaded newest first up to the damage; a `pkey2` whose newest message is in the damage is absent (`msg2db_id_incomplete()`), never an older message. |

**Source:** `tests/c/tr_msg/`, `tests/c/tr_queue/`, `tests/c/tr_msg2db/`

## TreeDB

| Binary | Description |
|--------|-------------|
| **`test_tr_treedb`** | Schema creation, user/department/compound structures, node CRUD, and state snapshots (foto files). |
| **`test_tr_treedb_load_failed`** | A topic whose keys cannot all be read: treedb loads the other keys, remembers the ones that failed, and refuses a create of such an id, and snapshot operations when `__snaps__` did not load whole. The recovery, after the key is deleted, needs no reopen. |
| **`test_c_treedb_literal_wins`** | [`C_TREEDB`](#gclass-c-treedb): a schema from C newer than the schema file in use wins whole; `__system__` is projected from it whole; the operator work it discards is reported once (`withdrawn_at_open`); an unfinished projection is recorded and completed at a later open, also after the process is killed at any write, once or twice. The scenarios are listed in its `README.md`. |

**Source:** `tests/c/tr_treedb/`, `tests/c/tr_treedb_load_failed/`,
`tests/c/c_treedb_literal_wins/`

## gobj-c helpers and commands

| Binary | Description |
|--------|-------------|
| **`test_helpers`** | String, file and directory helpers: `split2()`, `save_json_to_file()`, `rmrdir()` / `mkrdir()` with links, deep trees and entries that vanish during the walk. |
| **`test_rotatory`** | `rotatory_remove_old_files()` and the rotatory log writer: size rotation, a full disk, a clock set back, a write that fails and the file opened again (no newfile callback for the same file), a keep_all size rotation whose rename fails (tried once, every record kept), a fixed name never emptied, an `MM` mask judged by its month. |
| **`test_audit_record`** | The audit record builder of `yuneta_agent`: secrets redacted, `content64` never written, the command word read as the parser reads it (`COMMAND=list-yunos` with a kw `command=delete-yuno` gets the full record). |
| **`test_gbmem_realloc_refused`** | A `gbmem_realloc()` refused because the new size is larger than the largest block leaves the old block valid and tracked: the later free logs nothing and the memory counter stays right. |
| **`test_command_binary_kw`** | A command, and `build_stats()`, whose kw carries a `gbuffer`: the handler's kw holds a reference of its own, and the caller's references are intact after the command (no *"BAD gbuf_decref()"*). |
| **`test_dir_array_nomem`** | A directory listing that cannot keep an entry (no memory), or whose root cannot be opened, answers `-1` with the listing empty, and logs it: `find_files_with_suffix_array()`, `walk_dir_array()`, `get_ordered_filename_array()`. |
| **`test_dir_listing`** | The answer of the agent's `dir-*` commands: a tree that cannot be listed answers `-1` with a comment that names the directory, never an empty list. |

**Source:** `tests/c/helpers/`, `tests/c/gbuffer/`, `tests/c/command_binary_kw/`

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

## Build flag

Tests are enabled by default via `.config` (`CONFIG_MODULE_TEST=y`).
Toggle with `menuconfig` or pass `-DENABLE_TESTS=OFF` to CMake.

## Related

- **Performance benchmarks** live under `performance/c/` — `perf_c_tcp`, `perf_c_tcps`, `perf_yev_ping_pong`, `perf_yev_ping_pong2`, **`perf_auth_bff`** (live throughput over the BFF in the ping-pong style: a run is 10 s by default, about 180 000 ops on the reference box), and the persistence ones: **`perf_timeranger2`** (the open of a store, the create of topics, a tm query before and after `tranger2_mark_tm_order()`), **`perf_tr_treedb`** (treedb writes), **`perf_c_treedb`** (the open of a dynamic-schema treedb by `C_TREEDB` in a store of 40 treedbs) and **`perf_rotatory`** (one record of the log files and of the agent audit). Each prints one line of JSON per result; ctest runs them small, and `performance/c/README.md` keeps the figures of each release.
- **Stress runners** live under `stress/c/` — `stress/auth_bff` drives concurrent BFF login / refresh / logout cycles to expose races between the pending queue, the watchdog and the flush-on-disconnect path.
