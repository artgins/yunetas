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

# Run tests at once
ctest -j8 -R "c_tcp|c_mqtt|tcps" --test-dir build
```

### Tests that run at once

`ctest -j` runs test binaries at the same time, so no two of them may use
one fixed port (the second fails with *"bind() FAILED"*) or one fixed
directory. Every test binary under `tests/c` and `performance/c` listens on
ports of its own, and a new test takes new ones:

```bash
python3 scripts/check_test_ports.py          # exit 1 when two binaries share a port, or a file cannot be read
python3 scripts/check_test_ports.py --list   # every port and the binary of it
```

The files of one binary may share a port (`main_<name>.c` with `c_<name>.c`,
or everything under `<test>/src/`); a port given at compile time (`-DIDP_PORT`
of `c_task_authenticate`, one per binary from its CMake loop) is not seen by
the script. The `c_mqtt` tests that keep a store make a work dir of their
own run, `<$TMPDIR or /tmp>/<name>.<pid>.<n>`, and remove it at the end
(`tests/c/c_mqtt/test_work_dir.c`). Up to 7.25.20 ten ports were shared (the
`c_tcp`/`c_tcps` families on 7778, the `c_auth_bff` binaries on 18801/18802,
the `yev_events` tests on 3333, among others), and each `c_mqtt` test wiped a
fixed `/tmp/test_mqtt_<name>` at its start. Two whole suites at once on one
machine still share each binary's port.

A directory under `~/tests_yuneta` that two binaries use is declared in their
`CMakeLists.txt`, so ctest never runs them at once. Each binary that holds the
directory takes a `RESOURCE_LOCK` named after it. When one binary fills the
directory and others read it, the writer is the `FIXTURES_SETUP` and the
readers are `FIXTURES_REQUIRED`, so ctest also runs the writer first when you
ask for one reader alone:

```cmake
set_tests_properties("timeranger2/test_topic_pkey_integer" PROPERTIES
    FIXTURES_SETUP tr_topic_pkey_integer
    RESOURCE_LOCK tr_topic_pkey_integer
)
set_tests_properties("timeranger2/test_topic_pkey_integer_iterator3" PROPERTIES
    FIXTURES_REQUIRED tr_topic_pkey_integer
    RESOURCE_LOCK tr_topic_pkey_integer
)
```

The locks in use are `tr_topic_pkey_integer` (with `DEPENDS`: `_iterator6`
appends to the directory, so it runs after the other iterators), `tr_msg`,
`tr_delete_instance`, `perf_topic_integer`, and `inotify_flood` (the two
tests that flood the per-user inotify queue). The benchmarks and the
`tr_topic_pkey_integer` chain are `RUN_SERIAL`: the release trend reads their
times, and other tests that run at the same time change them. So are the
tests that assert a time window of the wall clock (`test_yevent_timer_once1`,
`_once2`, `_periodic1`, `_kept_after_post`, `test_c_timer`, `test_c_timer0`,
`test_static_resolv_numeric`): a stall of a loaded machine moved
`test_yevent_timer_once1` from 1.0 s to 1.8 s, out of its 0.9-1.1 s window. And
`test_fs_watcher_overflow`, which asserts the watcher's cost per directory on
a big tree against a small one: under `-j8` on a loaded node it measured 713
us against 48, where 6 times is the bound. And `test_c_controlcenter_scenarios`,
which asserts the rate of a burst over its 1.5 s tick.

To compare the TIMES of two runs, compare runs of one job count. `yunetas
test` names its log after it (`build/<date>.j<N>.txt`, CLI 0.21.1+), and only
the `RUN_SERIAL` tests run alone under `-j`; the times of the others come from
a serial run:

```bash
yunetas test --serial     # build/<date>.j1.txt
grep "timeranger2/test_topic_pkey_integer \.\.\." build/*.j1.txt
```

A new test that reuses a directory of another test must declare the same
lock.

`scripts/check_test_databases.py` checks it, as `check_test_ports.py` checks
the ports. It reads the directories of each test from its sources (a
`#define` of a `*DATABASE*` or `*STORE*` name, a `build_path()` segment after
`"tests_yuneta"` or after `path_root`, the `"database"` key of a
`C_TRANGER`/`C_NODE`/`C_TREEDB` kw, the domain of
`register_yuneta_environment()`, a literal `/tmp/<path>`) and the properties
from ctest itself, so it needs a configured and built `build/`. Two tests that name one
directory (or one under the other) collide unless ctest keeps them apart: a
shared `RESOURCE_LOCK`, `RUN_SERIAL` on one of them, a `DEPENDS` between them,
or one the `FIXTURES_SETUP` of what the other `FIXTURES_REQUIRED`:

```bash
python3 scripts/check_test_databases.py          # exit 1 on a collision, or a test not read
python3 scripts/check_test_databases.py --list   # every directory and its tests
```

An illustration (the report a third test naming `tr_msg` without the lock
would get; `test_pkey2_empty` uses `tr_msg2db_test` in fact):

```text
~/tests_yuneta/tr_msg COLLISION
    tr_msg/test_tr_msg1: tests/c/tr_msg/test_tr_msg1.c  [tr_msg]
    tr_msg/test_tr_msg2: tests/c/tr_msg/test_tr_msg2.c  [tr_msg]
    tr_msg2db/test_pkey2_empty: tests/c/tr_msg2db/test_pkey2_empty.c
    can run at once: tr_msg/test_tr_msg1 <-> tr_msg2db/test_pkey2_empty
```

Tests that run the same executable with other arguments (the groups of
`test_c_treedb_literal_wins`) are not compared, and a directory built at run
time from a format is not seen.

`scripts/check_test_links.py` fails when a test's `CMakeLists.txt` names an
SDK archive bare (`libtimeranger2.a` instead of
`${LIB_DEST_DIR}/libtimeranger2.a` or `${YUNETAS_KERNEL_LIBS}`): a bare name
is a `-l` search, not a dependency of the link, and the test keeps the old
library when that one changes.

`ctest -j` also needs the inotify limits of `99-yuneta-core.conf` (installed
by the package). With the Linux defaults (128 instances per user), several
treedb tests at once fail with *"inotify_init1() FAILED"*. Two checkouts on
one machine share `~/tests_yuneta`, so do not run two suites at once.

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

An entry matches the keys it names, and only those: `msgset`, `peername` or
any other field of the log can be asked for beside `msg`. **The priority** is
one more (since SDK 7.27.0): the captured log carries `"__priority__"`
(`"ERROR"`, `"WARNING"`, `"INFO"`...), which the logged json does not have.
Ask for it where the severity IS the behaviour under test -- a peer's mistake
that must stay a WARNING, a broken contract that must stay an ERROR; the same
`msg` at another priority then fails the test. An entry that does not name it
matches any priority, as before.

```c
json_t *errors_list = json_pack("[{s:s, s:s}, {s:s, s:s}]",
    "msg", "Cannot link self node", "__priority__", "WARNING",  // a command's: a peer
    "msg", "Field required: 'name'", "__priority__", "ERROR"     // a C caller's
);
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
| **`test_yevent_stop_stale_sqe`** | A ring of 4 entries, wrapped: a timer armed 4 times and stopped `RUNNING` gets exactly one `STOPPED` callback (destroyed in it, no completion after); a connect stopped `RUNNING` on the fd number whose old slots name a READ event leaves that read untouched. No take-back is warned, memory is clean. The loop clears the fd and the event of every ring entry it hands out, and a stop prepares its cancel before it closes the fd. |
| **`test_static_resolv_spoof`** | The static resolver (`yuneta_getaddrinfo()`) accepts a DNS answer only from the nameserver it asked: a forged answer from another source port is dropped by the kernel (the UDP socket is connected). |
| **`test_static_resolv_numeric`** | The static resolver sends no numeric host to DNS: an address of the family asked for is answered directly, one of the other family (`"::1"` in `AF_INET`) answers `EAI_ADDRFAMILY`, and `AI_NUMERICHOST` with a name answers `EAI_NONAME`; as glibc, `"127.0.0.1"` in `AF_INET6` with `AI_V4MAPPED` answers `::ffff:127.0.0.1` and `"::ffff:1.2.3.4"` in `AF_INET` answers `1.2.3.4` (both `EAI_ADDRFAMILY` up to 7.25.20). The IPv4 forms glibc takes as numeric (`"127.1"`, `"10.1.2"`, `"0x7f.1"`, `"0177.0.0.1"`, `"2130706433"`) are answered as glibc answers them, in every family and with `AI_NUMERICHOST` too (up to 7.25.20 they went to DNS); `"1.2.3.4x"`, `"256.1"`, `"08.1"` or a blank around it stay names. Each case must also answer fast. |
| **`test_gobj_post_event`** | [`gobj_post_event()`](#gobj_post_event): an event posted is not delivered at once, it arrives on the next cycle of the loop, in order, each with its own kw. |
| **`test_gobj_dispatch`** | A gobj destroyed while an event is dispatched to it or published by it. A subscriber that destroys the PARENT of its publisher is an ERROR (*"Destroying a gobj while its tree is publishing"*), the destroy is done, the publication stops (the next subscriber hears nothing) and the freed publisher is not read. With the `machine` trace on, a gobj that destroys itself in its own action, one that publishes its last event and then destroys itself, and one destroyed by its host in its `EV_STOPPED`, are no error. A subscriber of `EV_STATE_CHANGED` that destroys the gobj whose FSM is moving it: the action of the event is not run (an ERROR). A gobj that ends itself in its own `mt_start`, `mt_stop` or `mt_play`: the walks of `gobj_start_tree()` / `gobj_stop_tree()` do not go into it, no ERROR. A publisher that destroys itself in an action a subscriber sends it, and a gobj whose action sends to a peer that sends back the event that destroys it, are an ERROR (*"one of its actions is still running"* for the second). Up to 7.26.9 the first case was a SegFault, and under ASan the others read or wrote freed memory. A RUNNING gobj destroyed whose `mt_stop` publishes `EV_STOPPED` at once, and whose host destroys it there: the outer `gobj_destroy()` returns, the gobj freed once (up to 7.26.9 it went on with the freed gobj: under ASan a use of freed memory at its `obflag`, plain a *"Deleting item with DIFFERENT dl_list_t"* and an abort). Walks of the tree whose callback destroys more than the gobj it was given: the stop of a child destroys its next sibling (in `gobj_stop_tree()` and in `gobj_stop_children()`), the stop of a child destroys the root of the walk (a host that ends itself in its child's `EV_STOPPED`), and the destroy of a running child, inside the destroy of its parent, destroys the next child: each walk goes on from what is left (up to 7.26.9 each read the freed next gobj, under ASan). An action of a gobj that began INSIDE its own `EV_STOPPED` (its host sends it an event from there) and is still running when the gobj is destroyed further in: an ERROR (*"one of its actions is still running"*): only the code the farewell runs inside of is excused. A gobj whose own action walks its parent's children and is destroyed by that walk (its callback, `gobj_destroy_children()`, `gobj_destroy_named_children()` with the gobj's own name): no ERROR, the watch frames of the walk are not the code that destroys. A walk of `gobj_start_tree()`, `gobj_start_children()`, `gobj_stop_tree()` or `gobj_stop_children()` whose child destroys itself AND the next one: the walk goes on from the child after them, which is started or stopped. With a child before them whose start is refused (a required attribute empty), one that stops itself in its `mt_start` and one that starts itself again in its `mt_stop`: none is started or stopped twice (the refusal is logged once). A host that renews its transport in the transport's `EV_STOPPED` (destroys the pair, creates and starts a new one): `gobj_stop_children()` and `gobj_stop_tree()` stop the children they had and end, leaving the new pair running (the first fix of 7.27.0 started again from the first child, and never ended). A `gobj_walk_gobj_children()` / `gobj_walk_gobj_children_tree()` in the six walk types whose callback destroys the child AND its next one: each child left is given once. `gobj_stop_children()` of 8000 children and 4000 such pairs takes about four times what 2000 and 1000 take, not sixteen (the first fix of 7.27.0 was quadratic). `gobj_destroy_named_children()` with a `NULL` or empty name fails with an ERROR (up to 7.26.9 a `NULL` crashed). `gobj_destroy_children()` of a live host that renews its transport in its `EV_STOPPED` destroys the children it had and leaves the new pair; of a gobj being destroyed whose running child creates a sibling in its `mt_stop`, the create is refused with an ERROR (*"Cannot create a child of a gobj that is being destroyed"*) and the destroy ends (the first fix of 7.27.0 took the new children too, and never ended: the test has a 60 s limit). A play whose `mt_play` destroys its gobj fails, and so does a play whose implicit start is refused (a required attribute empty), which leaves the gobj not playing. `WALK_BYLEVEL` in both directions over a tree of leaves and branches gives each child once, in its order, and not a sibling created during the walk with a child of its own, nor that child: the pass over the children's children steps over a leaf without the watch, and must end there as the watch does. |

**Source:** `tests/c/yev_loop/yev_events/`, `tests/c/yev_loop/yev_events_tls/`,
`tests/c/yev_loop/static_resolv/`, `tests/c/gobj_post_event/`, `tests/c/gobj_dispatch/`

## Timers

| Binary | Description |
|--------|-------------|
| **`test_c_timer`** | [`C_TIMER`](#gclass-c-timer) GClass — periodic timeout operations. |
| **`test_c_timer0`** | [`C_TIMER0`](#gclass-c-timer0) GClass — basic (low-level) timer. |
| **`test_c_counter`** | [`C_COUNTER`](#gclass-c-counter): one that expires (its own `C_TIMER`) publishes its final count once, with the count so far, on the next turn, and is gone; one expired from outside does not count what arrives after; one that reaches its count publishes inside the last count; one expired from outside and STOPPED by its owner before its final count is delivered publishes none, and the volatile counter is gone; one that is not volatile, expired from outside, stopped and STARTED again goes on counting in `ST_IDLE` (the expiration of the run before is dropped). Neither says *"Publish event WITHOUT subscribers"* (a stopped counter has no subscribers left) nor *"Event NOT DEFINED in state"*. Up to 7.26.9 the expiration freed the timer under its own publication (under ASan, a read of freed memory in `C_TIMER` and in the kernel). |

**Source:** `tests/c/c_timer/`, `tests/c/c_timer0/`, `tests/c/c_counter/`

## TCP networking

Plain and TLS TCP through the full GObj protocol stack.

| Binary | Description |
|--------|-------------|
| **`c_tcp/test1–4`** | Plain TCP: connect, disconnect, echo, and rapid multi-message burst. |
| **`c_tcp/test5`** | A write that does not start (an empty gbuffer): `C_TCP` drops the connection, frees the write, and a stop reaches `ST_STOPPED`. An empty write gets its `EV_TX_READY` a turn later, connected and, for a client that connects on demand (`timeout_inactivity`), while still disconnected (no connect). |
| **`c_tcp/test6`** | The stop of a client already disconnected (nobody listens): `EV_STOPPED` once, `ST_STOPPED`, and a start after it works (twice). |
| **`c_tcp/test8`** | A `C_IOGATE` with `send_type` 1 sends one gbuffer message to its 3 channels (`C_PROT_TCP4H`): every raw peer gets the whole frame. Up to 7.25.20 the first channel read the shared gbuffer out and the others sent an empty frame. |
| **`c_tcp/test7`** | Data sent to a `C_TCP` while its drop waits in `ST_WAIT_STOPPED`: dropped with no error, and ONE warning with `dropped_msgs` and `dropped_bytes` when the close ends; a second `C_TCP` stopped and destroyed while it still waits says its drop at the destroy; the `disconnect_cause` of a drop (`Local dropping`) and of an inactivity close (`Inactivity timeout`) are what the owner reads at `EV_DISCONNECTED` and after the close, not the cancel of the read. |
| **`c_tcp/test9`** | A `C_TCP` client and a clisrv, both checked connected, are each sent a write, stopped and DESTROYED in one turn, before `EV_STOPPED` (the way every yuno ends): the destroy cancels the write in flight and nothing of the gobj is used after, and the clisrv's socket is closed as the stop closes it -- with data its peer sent still unread, the peer must read the end of the connection (a FIN), not a reset. Up to 7.29.3 the completion of the write ran on the freed gobj (a SegFault in a plain build), and the clisrv's socket was left open; in the fixes after 7.29.3 it was closed with no `shutdown(SHUT_WR)`, and the peer got a RST. |
| **`c_tcp2/test1–4`** | Same scenarios using the newer [`C_TCP_S`](#gclass-c-tcp-s) method (no `child_tree_filter`). |
| **`c_tcps/test1–4`** | TLS TCP: connect, disconnect, echo, and multi-message burst. |
| **`c_tcps/test6`** | A TLS server (`child_tree_filter`) stopped and started again in the same turn while a connection of it lives, and its certificates reloaded (`reload-certs`): the echo of a message on the old connection comes back on it, and, the old one closed, the echo of one on a new connection comes back on the new one. The certificates are copies: a restart with them unchanged reloads nothing, and one after the copy is touched reloads it. Freed memory is poisoned, so a use of a freed ytls fails every run (up to 7.25.20 it did: the restart freed the ytls of the live connection). |
| **`c_tcps/test7`** | A connection accepted into a channel with no `C_TCP` of its own (`C_CHANNEL` -> `C_WEBSOCKET`) gets a volatile `C_TCP`, destroyed by the websocket when it publishes `EV_STOPPED`. A client that sends a request that is not HTTP is dropped from inside that `C_TCP`'s own `EV_RX_DATA`, over TLS and in clear: both clients are disconnected and no `C_TCP` is left. Freed memory is poisoned (up to 7.25.21 a SegFault on both: the `C_TCP` used itself after the publish). `test7_mbedtls` runs it on mbedTLS when both backends are compiled in. |
| **`c_tcps2/test1–4`** | TLS TCP with the newer method. |
| **`c_tcp_inactivity/test1–4`** | `C_TCP`'s `timeout_inactivity`. |
| **`c_tcp_inactivity/test5`** | `C_TCP` with `timeout_between_connections` 0 retries at a 1000 ms floor, with one warning (up to 7.26.9 it never reconnected). |
| **`test_c_tcp_s_ip_lists`** | [`C_TCP_S`](#gclass-c-tcp-s) at accept: a peer in `denied_ips` is refused (with or without `only_allowed_ips`, and the deny wins over `allowed_ips`), with `only_allowed_ips` a peer not in `allowed_ips` is refused, loopback is exempt from both lists; a refusal is logged on the transition (the first one of a cause, then one a minute at most) and counted in the stat `refusedConnxs`: a denied peer that connects 5 times more writes no line, and `refusedConnxs` is 8 (up to 7.25.4 there was no stat, and each refusal wrote a line); a full server (3 channels taken) refuses 3 more with one warning, counted in `noChannelConnxs` (up to 7.25.21 an ERROR each, counted nowhere); the list key of each peername form (`1.2.3.4:80`, `[2001:db8::1]:443`, `[::ffff:1.2.3.4]:80`); the `add-`/`remove-` ip commands store the form a peer is looked up by (`2001:DB8::1` as `2001:db8::1`, `::ffff:203.0.113.7` as `203.0.113.7`, `fe80::2%lo` as `fe80::2%1`) and refuse what is not an ip; a link-local entry without its interface denies on every interface and is refused in `allowed_ips`; and entries stored as typed by 7.25.4 are renamed or dropped at load, with a warning, and saved. |
| **`test_c_qiogate_stats`** | A `C_MQIOGATE` (broadcast) over two `C_QIOGATE` whose peer does not listen: 5 messages sent, its stats say `msgs_in_queue` 5 and `pending_acks` 0 for each child, and so do the children's attributes (up to 7.25.21 only the gate's own `mt_stats` said them, which a `C_MQIOGATE` never asks). Then a gate with `min_free_disk_percentage` 101 refuses 3 messages (`-1` to the sender, `dropped_msgs` 3, one ERROR), the other gate takes them, and with `0` it accepts messages again (up to 7.28.3 a message the queue could not save aborted the yuno). |
| **`test_c_tcp_s_stats`** | The connection stats of [`C_TCP_S`](#gclass-c-tcp-s): one server accepts with `child_tree_filter`, the other with the new method (each channel's `C_TCP` accepts by itself). Raw peers connect, close and connect again: `connxs` is 2, 1, 2 and `tconnxs` 2, 2, 3 on both, read as attributes and in the `stats` answer. Two more servers share one pool of channels and one port on two hosts (`127.0.0.1:7816`, `127.0.0.2:7816`): each counts only its own connections, also after one is stopped and started again in the same turn (it listens again when its stop ends). A new-method server stopped alone and started again in the same turn: its clisrvs go and come with it, and it accepts again. A server whose only clisrv is destroyed while its stop waits for it listens when started; a server destroyed leaves no clisrv naming it, and one made in its place stops and starts again. A gate of the new method stopped and started again with a channel added: the new clisrv is named `clisrv-3`, and the clisrvs started again leak nothing. Up to 7.25.20 both read 0 always (`SDF_STATS` without `mt_reading`, a `connxs` never decremented, and no count at all with the new method), and a clisrv of the new method started again overwrote its first accept event. |
| **`test_c_tcp_s_addr_in_use`** | A [`C_TCP_S`](#gclass-c-tcp-s) whose address is in use at its start, held by a raw listening socket: one `bind() FAILED` (a WARNING), *"Address in use: listening again"*, and when the port is released the retry listens (*"Listening: its address in use was released"*, `ST_IDLE`) and accepts a client; held longer than `listen_retry_msec` (1 ms), the retry is refused too and the start fails (*"yev_create_accept_event() FAILED"*, an ERROR with `errno` 98); with `listen_retry_msec` 0 it fails at once. The yuno's tick is 50 ms and the test's `C_TIMER` subscribes to it before the server's, so the count of refused binds is exact. Up to 7.27.3 the first refusal failed the start: the yuno exited, and its watcher launched it again 2 s later. |

**Source:** `tests/c/c_tcp/`, `tests/c/c_tcp2/`, `tests/c/c_tcps/`, `tests/c/c_tcps2/`,
`tests/c/c_tcp_inactivity/`, `tests/c/c_tcp_s_ip_lists/`, `tests/c/c_tcp_s_stats/`, `tests/c/c_tcp_s_addr_in_use/`

## UDP networking

| Binary | Description |
|--------|-------------|
| **`test_c_udp_s_tx`** | [`C_UDP_S`](#gclass-c-udp-s) sends every datagram of its queue, in order, and drops one it cannot send -- no peer address (an error), refused by the kernel, a peer port `0` (a warning with the peer and `EINVAL`) -- leaking nothing, and goes on listening. Then it is sent a datagram, stopped and DESTROYED in one turn: no error, and the send in flight is canceled (up to 7.29.3 its completion ran on the freed gobj: a SegFault; and an ERROR said *"GObj NOT STOPPED. UV handler ACTIVE!"*). |
| **`test_c_udp_tx`** | [`C_UDP`](#gclass-c-udp), the UDP client (not registered by the SDK: the test registers it), sends every datagram of its queue, in order, publishes `EV_TX_READY` once when the queue is done and one turn after an empty send (never inside the send), counts 3 datagrams and 11 bytes, and its stop ends in `ST_STOPPED` with `EV_STOPPED`. A send, a stop, a send while the stop waits (dropped, ONE warning) and a start in one turn: the start is taken when the stop ends, with no `EV_STOPPED`, and the datagram in flight goes. A second `C_UDP` destroyed with its send in flight frees nothing in use, and logs no error. Against 7.29.3 it did not build (`c_udp.h` declared another name) nor register, and only the first datagram was sent. |
| **`test_c_udp_tx_refused`** | A `C_UDP` sends 50 datagrams to a port of the loopback where nobody listens: its read fails with `ECONNREFUSED` while a send is in flight, and it stops by itself, ending in `ST_STOPPED`, NOT running, with ONE `EV_STOPPED`; a `gobj_start()` then works. Then 25 times a send, a stop (checked in `ST_WAIT_STOPPED`) and a start in one turn: the `C_UDP` must stay running in `ST_IDLE`, with no `EV_STOPPED`. Then 25 times a send, a stop and a start made at its `EV_STOPPED`, when the completion of the cancel of the read can still be to come (the send in flight is what makes this phase red on its own with the read kept across starts): it must run in `ST_IDLE`, with one `EV_STOPPED` per stop. In the fixes after 7.29.3 it stayed in `ST_WAIT_STOPPED` for ever, and the start said *"GObj ALREADY RUNNING"*; and the start (in the stop, or at its `EV_STOPPED`) armed the read again with the completion of its cancel still to come, which failed it: the `C_UDP` stopped by itself at every restart. |
| **`test_c_udp_s_restart`** | [`C_UDP_S`](#gclass-c-udp-s) stopped with a send in flight ends the stop with `EV_STOPPED`; started again after a file took the number of its old socket, it reads and sends again; stopped and started in the same turn (its read still canceling), it reads again. |
| **`test_c_udp_s_rx`** | The pieces of two long frames of two peers, interleaved, come out whole from [`C_GSS_UDP_S`](#gclass-c-gss-udp-s): every datagram is labelled with its peer. The yuno's ip lists: with `only_allowed_ips` a peer that is not allowed is dropped with a warning, a peer in `denied_ips` too (denied wins over allowed), an allowed one and the loopback are heard. A refused peer that insists is said once per cause, not per datagram, and the stat `rxRefusedMsgs` counts every drop. What a peer holds in a `C_GSS_UDP_S` is capped: a peer beyond `max_channels` is dropped (said once), a frame with no NUL within `max_frame_size` is delivered cut, the bytes of all the unfinished frames stop at `max_pending_bytes` (the frame of the peer that passes it is dropped), and the memory grows by far less than 1 MB per source port. A host answers the peers of the frames with `EV_SEND_MESSAGE`: by the label of a frame (no address), by its address (no label), and a send with neither is refused with an ERROR. |
| **`test_c_udp_s_self_stop`** | [`C_UDP_S`](#gclass-c-udp-s) whose next read cannot be started (an `rx_buffer_size` of 0, the host answering in the gbuffer it got) says it with an ERROR and stops by itself; with the answer in flight the stop still ends in `ST_STOPPED` with `EV_STOPPED`. A `udps://` url is refused at the start (an ERROR, the start fails, nothing listens). |
| **`test_c_gss_udp_s_self_stop`** | [`C_GSS_UDP_S`](#gclass-c-gss-udp-s) when its `C_UDP_S` stops by itself (the host keeps a datagram and the next read, of 0 bytes, cannot start): one warning; a send refused with one warning, also in the window where the `C_UDP_S` is still stopping (`ST_WAIT_STOPPED`, a send in flight); the `C_UDP_S` started again after `timeout_base`, and a datagram heard and an answer sent after it; a second self-stop within a minute doubles the backoff; a `C_UDP_S` stopped from outside is not started again; a `timeout_base` of 0 is refused with a warning and the default used. Up to 7.25.20 the service stayed deaf and every send logged *"Event NOT DEFINED in state"*. |
| **`test_c_udp_s_echo`** | A host that answers IN the gbuffer of each [`C_UDP_S`](#gclass-c-udp-s) `EV_RX_DATA` (its kw sent back as `EV_TX_DATA`): with five sends waiting in the queue and two peers talking in the same turn, and with three datagrams read while each answer is in flight, every answer reaches its own sender, whole, with no error. Up to 7.25.4 the gbuffer was cleared and read into again: an answer was sent empty (dropped) or with the bytes and the peer of another datagram. |

**Source:** `tests/c/c_udp_s_tx/`, `tests/c/c_udp_s_restart/`, `tests/c/c_udp_s_self_stop/`, `tests/c/c_gss_udp_s_self_stop/`, `tests/c/c_udp_s_rx/`, `tests/c/c_udp_s_echo/`

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
| **`test_peer_cert`** | Mutual TLS, on every backend compiled in: a client given `ssl_certificate` presents it (the OpenSSL client did not load one before), and [`ytls_get_peer_cert_info()`](#ytls_get_peer_cert_info) tells each end the `cn`, the DNS names and whether the chain was `verified`. A private CA signs the server, a client and, with another CA, a rogue client: mode `"required"` with a client of the CA passes and both ends read each other verified; `"optional"` with the rogue client completes the handshake and reads it NOT verified; `"required"` without a client cert rejects the handshake; a server without CA asks for no cert; a client cert that cannot be loaded makes `ytls_init()` answer `NULL` (not a fatal exit). Checked on mbedTLS too, with a build of both backends. |
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
| **`test9_browser_cancel`** | Browser disconnects in the middle of the round-trip. The BFF drops the stale IdP reply (`responses_dropped` counter) and does not forward it, and the task of the request is gone: the BFF stops it and destroys it (up to 7.26.9 it stayed, a stopped child of the BFF, one per request the browser left). |
| **`test10_kc_silence`** | IdP gives no answer. The `idp_timeout_ms` watchdog fires, answers 504 to the browser, and drains the task (`idp_timeouts` counter). Its ERROR (*"BFF IdP outbound watchdog fired"*) says its `cause`, and what the outbound was then: `connected` 1, `tcp_state` `ST_CONNECTED`. |
| **`test11_cancel_retry`** | Cancel-then-retry: state cleanup between two back-to-back logins on the same browser. |
| **`test12_stale_reply`** | Per-task browser generation blocks cross-user token leak races. |
| **`test13_refresh_expired`** | Expired refresh token → mapped error, no silent `kc_error`. |
| **`test14_method_not_allowed`** | Non-POST on an auth endpoint → 405. |
| **`test15_missing_body`** | Missing body → 4xx with mapped error. |
| **`test16_unknown_endpoint`** | Unknown `/auth/*` path → 404, not 5xx. |
| **`test18_discovery_failure`** | The IdP's discovery document has `issuer` but no `token_endpoint`: the BFF refuses it and does not mark the discovery done. |
| **`test19_logout_no_cookie`** | `POST /auth/logout` with no refresh cookie is refused up front (401, `missing_refresh_token`), before any IdP call. |
| **`test20_discovery_timeout`** | The IdP accepts the discovery GET and never answers: the discovery task ends by `idp_timeout_ms`, one ERROR for its cause (*"OIDC discovery: IdP did not answer in time"*; no request is queued, so no answer adds its own), counted in `idp_timeouts`. |
| **`test21_big_token`** | A browser keeps a cookie whose name and value take at most 4096 bytes. A login answered with an access_token of 4084 bytes (4096 with its name) fits, and its claims are still read (`username`, `email`). Then, on the same connection, an access_token of 4085 bytes and a refresh_token of 4084 (one byte over, with their names) are refused by their size, `500 token_too_large`, the `error` saying the size; a refresh_token of 4083 bytes fits; and an IdP success with no access_token is refused, `502 auth_unexpected_error`. No WARNING that a payload could not be read (warnings captured too). Up to 7.29.3 a token of 4 to 8 KB got a `200` with a cookie a browser drops, and a success with no access_token a `200` with empty cookies. |
| **`test22_discovery_unreachable`** | The IdP cannot be reached (a listener of the test with its accept queue full: the SYN is dropped). The connect watchdog (`idp_connect_timeout_ms`) ends the discovery with one ERROR for its `cause`; the login queued behind it is answered `502 Bad Gateway` on the wire, `auth_service_unavailable`, with the ERROR of that answer. In the fixes after 7.29.3 the failure was three ERRORs; up to 7.29.3 the 502 went out as `500 Internal Server Error`. |
| **`test23_cookie_no_host`** | `cookie_domain` set: a login with a `Host` of the domain gets `Domain=` on its cookies, one with no `Host` gets none, with a WARNING. Up to 7.29.3 a request with no `Host` kept the `Domain`. A `Host` of the domain whose name is 254 bytes (longer than a host name) gets no `Domain`, with a WARNING; one of 253 bytes and a port gets it. Up to 7.29.3 the name of 254 bytes got the `Domain`, the `Host` read cut to 255 bytes with nothing said. |

**Source:** `tests/c/c_auth_bff/` (includes `c_mock_keycloak`, `test_helpers`).

[`C_TASK`](#gclass-c-task), the engine under the two:

| Binary | Description |
|--------|-------------|
| **`test_c_task`** | A job whose timeout fires (its own `C_TIMER`) ends the task with `EV_END_TASK` `result` -2, once, on the next turn, and the volatile task is gone; a task timed out from outside and answered right after does not process the late answer and still ends once with -2; a job answered ends the task with 0 on the next turn, not inside the answer; a job answered INSIDE its action (a local service) processes the answer once and ends with 0 once; a task answered and STOPPED by its owner before its end is delivered publishes no `EV_END_TASK`, and the volatile task is gone; a task that is not volatile, answered, stopped and STARTED again, and answered again, ends once, with the end of the second run (the end of the first is dropped). Up to 7.26.9 the timeout freed the timer under its own publication (under ASan, a read of freed memory in `C_TIMER` and in the kernel; `c_auth_bff/test10_kc_silence` showed it too), and the answer inside the action freed the task under `execute_action()` (a SegFault). |

**Source:** `tests/c/c_task/`.

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
| **`c_mqtt/will_acl`** | Raw MQTT clients against a broker with `enable_acl` on, each step waiting for the packet it depends on: a will to a topic the client's group refuses is not published (a warning), one to an allowed topic is; a retained PUBLISH with payload reaches a later subscriber. Its memory check caught the payloads `C_NODE` leaked (two wills and the retained one; it needs `CONFIG_DEBUG_TRACK_MEMORY`). |
| **`c_mqtt/queued_in`** | A raw MQTT client: the incoming QoS 2 messages of a persistent session reloaded beyond `max_inflight_messages` wait queued, and each goes in flight when a PUBREL frees a slot, with its OWN packet id (`PUBREC 3, PUBCOMP 1, PUBREC 4, PUBCOMP 2, PUBCOMP 3, PUBCOMP 4`), as in mosquitto. Up to 7.25.4 the quota test was inverted and a NEW id was sent: the client's PUBREL found nothing, and the message was never released. |
| **`c_mqtt/client_queues`** | An MQTT CLIENT with persistent queues against a raw broker: a QoS 2 message received again with DUP=1 replaces the copy waiting for its PUBREL and is delivered once, a later message with the same packet id is delivered as itself, and no *"QoS mismatch"* is logged; an outgoing message that expires while queued gives its slot to the next queued one. |
| **`c_mqtt/out_flight`** | A raw MQTT 5 client with Receive Maximum 2 against a broker with `max_inflight_messages` 0: two QoS 1 messages in flight, the others after the acks; `list-queues qos=1` lists only the pending message; a PUBCOMP of a QoS 1 message is a WARNING *"QoS mismatch"*, a DISCONNECT 0x82, and the message stays pending. |

**Source:** `tests/c/c_mqtt/`

## Subscriptions & link events

| Binary | Description |
|--------|-------------|
| **`c_subscriptions/test1–3`** | GObj event subscribe / unsubscribe lifecycle. `test3`: a repeated HARD subscription is one (the second [`gobj_subscribe_event()`](#gobj_subscribe_event) returns the one there with a warning; each event arrives once), [`gobj_unsubscribe_event()`](#gobj_unsubscribe_event) leaves it with a warning, [`gobj_unsubscribe_list()`](#gobj_unsubscribe_list) with `force` removes it; a repeated `__own_event__` subscription, and a repeated `__rename_event_name__` one with a `__global__`, are overridden (one subscription, each event once), and [`gobj_unsubscribe_event()`](#gobj_unsubscribe_event) with the same kw removes them; a renamed subscription over a plain one, and two renames of one event, are two subscriptions (each event arrives once under each name), and each kw withdraws only its own (7.25.5 replaced one with the other); [`gobj_unsubscribe_list()`](#gobj_unsubscribe_list) removes the subscription it is given, so a stale plain one removes no live filtered one, and a top-level `renamed_event` in a find kw filters nothing; in the other order a plain subscription over a renamed one replaces it, and a plain unsubscribe removes both; one withdrawn by the `mt_subscription_deleted` of an entry before it is not taken for a hard one kept; a subscription the publisher refuses leaks nothing (up to 7.25.20 each refusal leaked it). |
| **`test_c_node_link_events`** | `EV_TREEDB_NODE_LINKED` / `UNLINKED` events at the [`C_NODE`](#gclass-c-node) GClass level, and an `update-node` with `autolink`: the links it repeats publish nothing, a moved link is one unlink and one link, and a ref that cannot be linked (missing parent, or a hook that links into another column) is logged while the record is still saved; and `set-link-events` switching a live treedb from the parent's `UPDATED` to `LINKED`/`UNLINKED` and back; and `delete-node` of a key with `pkey2` instances, reopened: a lookup of the primary's value is the primary, a refused delete leaves every instance. |
| **`test_c_node_commands`** | What the read commands of [`C_NODE`](#gclass-c-node) answer: `treedb-info`, `node` (and one not there), `instances`, `pkey2s`, `jtree` (`rename_hook`), `parents` / `children` with and without `options` (without them they logged an ERROR per option up to 7.25.22), `hooks`, `links`, `print-tranger`, and the snaps on the data (activated, the treedb opened again shows the node as it was; deactivated, as it is). What a command names is the peer's data, refused with a WARNING (never the treedb's ERROR): an unknown topic or one that breaks the name rule, a link or hook that is not a column with that flag (`departments^top^name`), a hook that does not link the child's topic (`link-nodes` / `unlink-nodes` by `departments^top^users` of a department: up to 7.26.9 *"Topic child not defined in hook desc links"*, an ERROR), an `update-node` of a node that does not exist, without `create` (up to 7.26.9 an ERROR with a stack and the whole kw dumped), a record whose `id` is not a string or a `content64` that is not an object, a `content64` that is not json (an ERROR with a stack and, up to 7.26.9, the decoded text in clear), a record with a NUL in its id or down it (`"dx\u0000evil"` took the key `dx`, up to 7.26.9), `create_only` of a node that exists, and `instances` by a `pkey2` that is not a secondary index (`id` walked the primary index as instances, 9 errors per node). Every refusal is asserted as a WARNING (`__priority__`), and the create of a subscriber inside a peer's create as an ERROR: the treedb publishes outside the peer's window. `instances` with no `node_id` and no `filter`; `import-db` of wrong records and an unknown topic, counted; an id with a `/`. What only the treedb can judge -- a link that closes a cycle, a node linked to itself, an unlink of a child that does not hang there, a create without a required field -- is said by the treedb as a WARNING, the record marked as a peer's (`treedb_set_peer_data()`). A parameter of the wrong type (`node_id` given a number) is refused by the command parser, a WARNING. An rt list of the topic (`tranger2_open_rt_mem()`) is fed outside the peer's window, like the subscribers. `treedb_node_children()` / `treedb_parent_refs()` with no options; an `import-db` of a record with no fkey logs nothing. |
| **`yuno_skeleton/templates`** | The templates of `yuno-skeleton` instantiated (answers on stdin) and built against `outputs/` with no warning: `yuno_standalone` with a `gclass_child` and a `gclass_service` in it, run so that each one's timer (a pure child) reaches its `ac_timeout`, and `yuno_citizen`; the `register_yuno_and_more()` of both templates hands back what its registrations answer (`result += register_c_<name>()`), so a gclass refused stops the yuno (7.27.0+). Red with a template timer that is a plain child, or with `MSGSET_INTERNAL_ERROR`. Since 7.28.7 also: the root gclass of both yuno templates carries the SERVICE subscription block; `c_project` builds; `js_gclass` and `js_gclass_service` load in node against this tree's gobj-js and their timers fire; `project` and `project_commercial` build from their root with a citizen yuno in `yunos/`, with the right `LICENSE` and a `gui/` whose `package.json` is json, whose `.js` parse and which keeps only i18next's `{{name}}`/`{{n}}`; no template leaves a marker without a value; an input that runs out is an error. It tests the tree it is in (its `YUNETAS_BASE` is its own source dir), never another tree or the installed `yuno-skeleton`. |
| **`yuno_skeleton/cloners`** | `yclone-gclass`, `yclone-project` and `ymake-skeleton` (one engine, `utils/c/yclone-project/clone_tree_dir.c`) on a yuno made by `yuno-skeleton` (7.28.7+): a key is replaced as a whole word (`time` -> `clock` leaves `C_TIMER`, `set_timeout`, `EV_TIMEOUT`) and in one pass (`sktcln` -> `sktcln_two` is never `sktcln_two_two`); a binary is copied byte for byte, modes and symlinks are kept, `.git` and `build` are not cloned, a destination inside the source is refused; each clone builds and says its new role, and a template made by `ymake-skeleton` goes back through `yuno-skeleton` and builds. 21 of its checks fail against the 7.28.6 binaries. |
| **`test_c_node_initial_load`** | The `initial_load` seed of [`C_NODE`](#gclass-c-node): records first and links second whatever the topic order, every seed immutable, a declared link refused to `unlink-nodes`, to an autolink update that omits it and to a `force` delete of its parent, and a second start that creates nothing. `print-tranger` walks any `path` ([`kw_find_path()`](#kw_find_path) in a loop): a segment longer than 255 bytes and a segment under a string are answered `-1` *"path not found"*, with no log (they were ERRORs of `kw_find_path()`, then WARNINGs of a walker of its own), and a string at the end of the path is answered as it is. |
| **`test_tr_treedb_link_events`** | Low-level link/unlink callback mechanism in `tr_treedb`. |
| **`test_tr_treedb_failed_save`** | A write whose save fails is taken back in memory: with the files of a key read-only, a failed update, link, replace of a single fkey, `treedb_replace_links()`, `treedb_autolink()` and unlink each answer a refusal, leave the nodes and hooks as the disk says, and tell no event. With the files writable the same writes work, and a reload says what memory said. Also: the update with its links (`treedb_update_node_and_links()`, with and without fields); a failed clean, where a stale ref (its hook no longer exists, or fills another column since a schema re-pointed it) goes back into its field and is never linked; refused forced deletes (a child that cannot be saved, a key that cannot be deleted) that change nothing, in memory or on disk; a take-back that puts every child back in its place in the hooks of its parents (a list hook and a dict hook keep the order they had); `treedb_autolink()` refusing a ref whose hook fills another column, with nothing moved; a refused forced delete whose put-back of a child fails too (the child stays unlinked, in memory as on disk, the ERROR says so, and the events of its unlink are told; the test fails the writes of that key in its own `__wrap_write()`); a forced delete that tells its unlinks after the node left the indexes, with a subscriber that cannot find the node nor save it, and a reload that does not bring it back; a write that cannot be taken back whole says so; and two active snaps at open (a deactivation that cannot be saved leaves the snap active; a replica does not repair). |
| **`test_c_node_failed_save`** | An `update-node` with `autolink` of [`C_NODE`](#gclass-c-node) whose save fails (the writes of the key fail in the test's own `__wrap_write()`): the method answers `NULL` and the command `-1`, memory is what the disk has, and no `EV_TREEDB_NODE_LINKED` / `UPDATED` is published. A create with `autolink` whose links cannot be saved leaves the node without them, in memory as on disk, and logs *"Node created, but its links cannot be saved (autolink): the node stays without them"*. A reload from disk agrees, and the retry works. |
| **`test_c_subscription_authz`** | The subscription authorization gate (`enable_subscription_authz`): three `C_IEVENT_CLI` connect over websocket to a `C_IEVENT_SRV` of the same yuno and subscribe to the `EV_TREEDB_NODE_*` feed of a real [`C_NODE`](#gclass-c-node). Gate off: every peer is subscribed. Gate on: a peer without `read` is refused and it is logged, a `reader` is subscribed (the checker is asked `read`), an event not flagged `EVF_AUTHZ_SUBSCRIBE` needs nothing; a node update reaches only the accepted subscriptions; the realtime feed of a real [`C_TRANGER`](#gclass-c-tranger) (`EV_TRANGER_RECORD_ADDED`) is guarded the same way (open to any user up to 7.25.4); the six channel commands of `C_IOGATE` with a `channel_name` that matches nothing answer with no channel (up to 7.25.4 they looped for ever); the `authzs` trace prints the kw it checked (up to 7.25.4 a kw already freed); a peer that asks `__hard_subscription__`, `__own_event__` and `__rename_event_name__` gets a plain subscription (only `__first_shot__` is kept), which goes with its session and reaches neither a later subscriber's loop nor the next user of the channel; every channel disabled and enabled again serves a new session; and the gate stops orderly (no *"Destroying a RUNNING gobj"*). |
| **`test_c_node_authz`** | The permission every [`C_NODE`](#gclass-c-node) command asks for, reads and writes, with a checker that refuses one principal: a refused command answers `-403` and does nothing. |
| **`test_c_ievent_srv_peer_subs`** | What a remote peer may put in a subscription, and hold, through `C_IEVENT_SRV`: one peer writes its frames by hand, with a `__global__` that forges keys of the event and sets framework keys and a `gbuffer`, a `__local__` and a stray key; a plain client subscribes with a `__filter__` and a `__global__`; two local subscribers, one with a `__local__` and a `__global__`. Each gets what IT asked and nobody else's (up to 7.25.4 the publish shared one kw: the plain client got nothing, the later local subscriber got the forgery without the secret and the peer's `__md_iev__`, and the `gbuffer` crashed the yuno), and the publisher's kw comes back as it went. `max_subscriptions` and `max_subscription_size` refuse; the dropped keys, the caps, a withdrawal of nothing (repeated, with a 2 KB key: one line, capped) and a repeated authz refusal are each logged once; a client withdraws a subscription with a `__global__`; a `__username__` and routing stamps (`input_channel`, `input_service`) that the peer puts in a command, a stats request, an event or a subscription are overwritten by the gate's (up to 7.25.4 the peer's `__username__` reached the service); an event that carries a `gbuffer` reaches two remote subscribers (each on a twin with its own reference) and a local one, and when the publish returns the publisher holds the only reference, with no *"BAD gbuf_decref()"*. The stored subscription holds only the reversed hop of the peer's frame, not a 4 KB key of the peer's own in its `__md_iev__` (up to 7.25.4 the whole `__md_iev__`, not measured), and a routing bigger than `max_subscription_size` is refused, logged once; a subscription repeated 3 times is kept as it is (the publisher's `mt_subscription_added` is not called again), one warning; 3 commands to a service the channel may not reach and 2 that name no command get negative answers and one warning each; a frame with no `__md_iev__` (50 KB) is one capped warning and closes the channel. And the server sends the plain client (an anonymous yuno) two events of its own through the gate of his channel (`gobj_send_event(<C_IEVENT_SRV>, EV_X, kw)`): with no `__service__` it reaches the subscribers of his transport, with `__service__` the service named; up to 7.26.9 both closed his channel (*"It's not my name"*: the gate addressed them to its own label for an anonymous yuno, `^peer-sock`). |
| **`test_c_ievent_srv_identity_card`** | The identity cards a raw websocket peer sends to `C_IEVENT_SRV` before its session: a card without `__md_iev__`, one for another `dst_role` (with a 4 KB key) or `dst_yuno`, one without `src_role` or `src_service`, one for a `dst_service` that does not exist, one whose `jwt` is not a string, and a command before any card that carries credentials (a `password` key, keys named like secrets down its kw, `token=...` in its command line), and an event no gclass has as public, carrying credentials and a JWT under a plain key, a secret in its NAME and 64 KB of kw (*"Frame of an event that is not public, refused"*, said once by `iev_create_from_gbuffer()`, the name masked, and a `frame_size` of what was received; up to 7.26.9 its kw was traced raw, its name was written in clear and the size was the masked copy's), a frame of json that is not an inter-event (*"Frame is not an inter-event ..."*, masked) and a frame that is not json (*"frame is not json"*, its dump masked), both with a JWT that a window of 256 bytes cuts after its header (up to 7.26.9 both were *"Bad json"* and a raw dump that showed the JWT's payload). Each is ONE warning (`MSGSET_PROTOCOL`, the kw capped, no stack, the jwt and every credential never in the log) and closes the channel; the last, good card is answered `EV_IDENTITY_CARD_ACK` and opens the only session. Up to 7.25.20 each was an error with the whole kw, and a second error, *"event UNKNOWN in not-session state"*; the card with a non-string jwt was accepted. |
| **`test_c_ievent_cli_peer_faults`** | The other side: what `C_IEVENT_CLI` does with the faults of its server. A raw websocket server (no `C_IEVENT_SRV`) answers each identity card of a real `C_IEVENT_CLI` with the next fault: a session event (`EV_MT_STATS`) before the session, then, after a good `EV_IDENTITY_CARD_ACK`, a frame for another `dst_role`, one for another `dst_yuno`, a subscription (`__subscribing__`) and an unsubscription (`__unsubscribing__`), the names the server writes carrying a credential (`password=...`); the last card is answered with the ACK alone. Each fault is ONE warning (`MSGSET_PROTOCOL`, no stack, the names masked and capped, never the credential) and closes the channel; the client connects again. Up to 7.26.9 each was an ERROR with the names in clear, both subscriptions said *"subscription event ignored"*, and the session event before the session went to `mt_inject_event()` (*"Not in session"*, an ERROR) before its own. In the last session the server then sends `EV_ON_OPEN` and `EV_ON_CLOSE` (events of the transport itself, which `C_PROT_HTTP_SR` declares public) and the ACK again: each is refused with ONE warning (*"event of the transport itself, or out of its place, refused"*), the channel stays, and the app hears none of them (up to 7.26.9 the first two reached it as the client's own open and close, and the ACK was sent back to the server). And `EV_TREEDB_NODE_UPDATED` five times: with a `dst_service` that is no service, and with a service that does not have it as public, each dropped with ONE warning (*"service of the event not found, event dropped"*, its name masked; *"event not public in its service, event dropped"*); with the client's own name (what its identity card gave the server), with that name in capitals, and with none, the three published to the client's subscribers, with no log (7.27.0; up to 7.26.9 all of them were published, an empty or unknown name with an ERROR and a stack and the name in clear, the not-public one silently). Built twice: `test_c_ievent_cli_peer_faults_child` makes the client a child with no `as_service`, which `gobj_find_service()` does not find, so the client compares its own name itself, case-blind as the lookup is (a case-sensitive comparison dropped the name in capitals there only). |
| **`test_c_node_paged_nodes`** | The paging of `C_NODE`'s `nodes`: no `limit` answers the plain list; a `limit` answers `{total_rows, pages, data}`; `from` is 1-based; a page past the end is empty with the right total; `from` / `limit` given as strings work. |
| **`test_c_tranger`** | [`C_TRANGER`](#gclass-c-tranger) commands: `topics`, `list-keys` (counts and time spans), `open-iterator` and its pages by `t` and by `tm`. `add-record` appends (a dict or its json text, `write`, master-only; it was a stub). The pushes of a live list carry its `list_id` as `rt_id`. A handle of one session is refused to another (`close-rt`, `close-iterator`, `close-list`, `get-page`, `get-list-data` answer `-403`), and through the agent's one link (a `C_IEVENT_CLI`) a handle of one user is refused to another user and to a relayed command of no user, and not to the same user. `add-record` asks `write` and answers `-403` on a refusal (a counting authz checker). What a peer hands is refused BEFORE the tranger, a WARNING each (`__priority__`), and the same from C code an ERROR: an unknown topic or a name with `/` or `..`, a system topic to delete, a `create-topic` of a wrong shape (an unknown `system_flag`, two key types, no `pkey`, a name longer than `NAME_MAX`, a FILE of the database, a new `topic_version` over a system topic, over a treedb's topic or without `jn_cols`; `jn_cols={}` taken for a topic with no columns), a new topic named as what a store keeps (`.hidden`, `__x`, `x.json`, `x.bak`), a `jn_var` of what the tranger keeps or types, a record without its pkey or with a pkey that is a path, a record with a NUL in a string of it or in its json text (the pkey `"K\u0000evil"` was appended under `K`, up to 7.26.9), a `__t__` or a tkey time past 44 bits (an integer, or a DATE: `"@99999999999999 +0000"`, and `"@20000000000 +0000"` on an `sf_tm_ms` topic; a date the row keeps is appended), a key that is a path, a time bound that is a dict; a `print-tranger` path that is not there answered `-1` with no log, whatever its length; a peer's write into a treedb's topic or a new topic in its store; `check-json` asks `read` and answers what it finds, logging nothing (with `max_refcount=1`, a `0`: `-1` is a fault); a replica answers READ-ONLY to `delete-key`, `delete-topic` and the `create-topic` of a new topic, with no ERROR. `delete-key-files` by date, by an integer and of every key, its `purged` in `list-keys`, and its refusals (a `before` in the future, a text that is no date, none, a key not there); a multi-key page across a purge keeps its positions, and a filtered one is closed saying so. See its `README.md`. `add-record` of a `__t__` in a purged period is refused before the tranger, with the cause in the answer, also for a key flagged `unlisted` (its `.purged` unreadable at the open); `delete-key-files` of a treedb's topic and of a mask that does not sort is refused from C code too. `archive-key-files` is refused with no `archive_directory` attribute, and with one makes a batch. |
| **`test_c_tranger_replica`** | [`C_TRANGER`](#gclass-c-tranger) as a REPLICA of a store a master in the same process purges, with no loop: a filtered multi-key iterator purged after its open closes the part the purge was applied in before it says the iterator was closed; an unfiltered one over a key the replica numbered short (a md2 it could not read) keeps its positions when the purge renumbers the key (`renumbered`); `delete-key-files` answers READ-ONLY; one-key and multi-key iterators with absolute rowid bounds (`from_rowid=3, to_rowid=4`) page the same rows, under their new g_rowids, after a purge that renumbers the key and closes no part. |
| **`test_c_agent_find_new_yunos`** | The rows of the agent's `find-new-yunos`, against a real [`C_NODE`](#gclass-c-node) with the agent's schema: a yuno already registered at the new release (a single one, and a `yuno_multiple` one) is marked *"already registered, pending promotion (deactivate-snap)"*, not listed as new. |
| **`test_c_controlcenter_scenarios`** | The control center's scenarios, on its own source (`c_controlcenter.c`), between a `__top_side__` of fake web clients and an `__input_side__` with one agent connected (a real `C_IEVENT_SRV` in session, answered as the agent does). Freed memory is poisoned and held in a quarantine, so a read after a free reads the poison: `save-scenario` with the scenario as a string answers with its id (up to 7.25.20 the id was read from the parsed json after it was freed); a step parameter that starts with `__` (`__md_iev__`, `__username__`) is refused by the save, and a scenario written without the step checks (as 7.25.14 saved it) is refused by `run-scenario` naming the step, with nothing sent to the agent; a step's answer counts only from the agent of the step: the marks of a step (`cc_run`, `cc_step`) that another client puts in a `command-agent` do not reach the agent, and an answer a client injects is dropped with a warning (up to 7.25.20 the answer through `command-agent` ended the run); what an agent sends back -- the answers of `command-agent` and `stats-agent`, the PTY of `open-console` -- reaches a channel only while it holds the connection that asked (up to 7.25.20 all of it reached the next client of a closed channel's name), and the agent's close drops the client of its console mirror only if it is still that connection; and a web client that injects any of those events itself, routed to another client, reaches nobody (up to 7.25.20 all six kinds were delivered); several consoles mirrored through one agent's channel each drop their client when the agent goes, a console opened again belongs to its last client, and closing one console forgets only it (up to 7.25.20 one slot per agent channel); and the switch of a yuno (7.28.6): a switch without its `off` is refused, `on` is refused while the scenario does not run, runs its steps in order to its own yuno, `off` sends them at once, and neither changes the state of the scenario. |

**Source:** `tests/c/c_subscriptions/`, `tests/c/c_node_link_events/`,
`tests/c/c_node_initial_load/`, `tests/c/tr_treedb_link_events/`,
`tests/c/tr_treedb_failed_save/`, `tests/c/c_node_failed_save/`,
`tests/c/c_subscription_authz/`, `tests/c/c_ievent_srv_peer_subs/`, `tests/c/c_ievent_srv_identity_card/`, `tests/c/c_ievent_cli_peer_faults/`, `tests/c/c_agent_find_new_yunos/`,
`tests/c/c_controlcenter_scenarios/`,
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
| **`test_delete_key_propagation`** | `tranger2_delete_key()` and who hears it (rt_mem and rt_disk feeds, a real follower); a key directory that cannot be moved out of `keys/` is not deleted nor announced, keeps its rows and its filtered iterators; one moved out that cannot be emptied is deleted all the same, announced once, and the directory left is swept at the master's next open. A key directory whose `stat()` fails with `EIO` is not "not found": `-1`, nothing deleted or announced. A `disks/` whose `opendir()` or `readdir()` fails is logged (by `--wrap=stat,opendir,readdir`). A feed opened after another heard a delete, and a second delete of a key heard first by a feed opened in flight, are each told once, by the delete sequence in the signal's name (`.d<seq>.<key>`); a key too long for the name (`.h<seq>.<sha256>`) is told to each of two feeds; after every feed heard, nothing of the deletes is kept. The record `delete_seq.json`: made at the master's open, the sequence written before the key leaves `keys/` (read from inside the `rename()` that moves it out), kept taken by a delete that fails (never given back), a record that cannot be written refusing the delete, a signal left by the master heard at its next open (an older one removed unheard), an unreadable record refusing the delete, a follower with no record saying so. |
| **`test_rt_disk_multi_feed`** | Several rt_disk feeds on one key of a follower: each wake-up serves only the feed whose directory fired. |
| **`test_rt_disk_emfile`** | A follower that runs out of descriptors (EMFILE) while it reads new key directories: the reads that fail are owed, and the next batch of events of the feed (a record of another key) reads them; every key ends level with the master (up to 7.27.6 they stayed behind for good). |
| **`test_out_of_order_append`** | An append whose `__t__` belongs to an earlier file of the key goes into that file's cell; every record is served once. |
| **`test_pkey_path_traversal`** | A string primary key is one directory component: a key with `/` or a leading `.` is refused. |
| **`test_topic_path_traversal`** | A topic name is one directory component: `.`, `..`, a `/` or a `` ` `` is refused, and a replica cannot delete or back up a topic. |
| **`test_read_never_exits`** | A failed READ never ends the process, whatever `on_critical_error` says. |
| **`test_iterator_index`** | The id index of a topic's iterators: found by `(id, creator)`, a closed one is not, a duplicate is refused. |
| **`test_date_bound_units`** | A date in `from_t`/`to_t` is taken to milliseconds on an `sf_t_ms` topic, and one in `from_tm`/`to_tm` on an `sf_tm_ms` topic; a day by `t` and by `tm` selects that day on a topic with `t` in ms and `tm` in seconds, and on the reverse. Up to 7.27.4 the `t` bounds asked `sf_tm_ms`. An end bound takes its whole second (23:59:59.500 is in the day). |
| **`test_delete_key_files`** | Retention, the master: `tranger2_delete_key_files()` deletes the old files of a key and the records left keep their `g_rowid`; the next append gets the next one, and a reopen says the same. The rowid bounds of a match_cond (`0`, a rowid of the purged rows, the base, `-1`, more rows back than the key has, past the end) and the positions of a page count from the base; iterators opened before the purge, filtered or not, page what is left. An append into a purged period is refused; a cut in the future and a mask with no year (`"queue"`, `%m-%d`) are refused; the hard link a dead feed left goes with its file, and one that cannot be removed keeps the files of the period until the next open of the master removes both; a key purged whole keeps its base and goes on numbering. A purge cut by a crash (a `.purged` written by hand over the files) is skipped by a replica and finished by the master's open; a `.purged.new` left is removed; a `.purged` that is not json flags the key `unlisted` (load_failed, purge and append refused) until it is put right; `tranger2_delete_key()` takes the base with the key (a failed delete of a key purged whole, or of a key whose directory cannot be moved out of `keys/`, keeps it). Pages across a purge keep their positions, filtered or not, and the size counts them (a key purged whole too); `tranger2_mask_sorts_as_time()` on 38 masks, and a treedb topic refused; a delete of a key with a base that cannot move its directory keeps the key, and a good one leaves no directory aside. A filtered iterator opened with a negative rowid bound (`from_rowid -4`, `-5..-2`) keeps its rows when the key grows and its old files are then purged. |
| **`test_archive_key_files`** | Retention that keeps what it takes: `tranger2_archive_key_files()` hard-links the files a purge takes into a batch with the schema of the topic and a manifest (`rows`, `first_g_rowid`, `last_g_rowid`); the topic is numbered as after a delete; a second purge makes a second batch that starts after the first. The guards (no directory, relative, missing, a file, inside the tranger or holding it, another filesystem, a batch name taken) delete nothing; each batch has its ready mark; a base that cannot be written removes the batch and leaves no mark; a `.new-*` left by a crash is removed; a replica and a reopened master read the base. |
| **`test_delete_key_files_follower`** | Retention, the followers: a follower with an rt_disk feed hears `.p.<key>` and drops the cells of the files purged, takes the base, and its feed goes on with the master's `g_rowid`; a key too long for the name is signalled `.q.<sha256>`; a follower with no feed hears nothing and finds the purge before its next load or page, also for an iterator opened before it. A file the follower flagged unreadable and the master purged is no flag for ever; a signal left in a feed directory does not stop the next one; a feed callback reading its key while a purge waits does not apply it under the cell of the new records; a filtered pager opened over a flag pages the master's numbering once the purge drops it; a base the follower cannot read while the master appends: the feed gets the master's g_rowid, the live rows are read while it waits, and the base is applied when it can be read. A filtered pager of the follower with a negative rowid bound keeps its rows across appends and a purge; the purge that renumbers a flagged follower says so with a WARNING, and its filtered pagers with rowid bounds, negative (`-3..-1`) and positive (`4..6`), keep the same rows, renumbered. |
| **`test_purge_replica_reads`** | Retention, a replica that reads at a bad moment (`--wrap=fopen,--wrap=open`): a base it fails to read with EMFILE is read again at the next load, one it fails to read with EACCES is not read again at every load; the master purges a key between the replica's listing and its open of a md2 file, and the replica numbers the key as the master, with no flag. |
| **`test_late_record`** | A record whose `__t__` is below the times of its md2 file is served by a time-range query. |
| **`test_stop_reopen`** | A tranger stopped and used again: no fd closed twice, a topic opened again revives it. |
| **`test_append_md_contract`** | `tranger2_append_record()` drops a `__md_tranger__` the record carries in, and returns the new metadata in the out-param. |
| **`test_str2system_flag`** | Every name of `tranger2_str2system_flag()` maps to its own bit. |
| **`test_tm_order`** | A `__tm__` that does not grow with `__t__`: a tm condition filters every row (no file left out, no early end); negative bounds, relative to the key's last record (its `tm`, not the highest one) and to each key's own in a list of several keys; a store with the old tm markers (7.25.5..7.25.22) answers the same. |
| **`test_lost_lock`** | A master that lost its lock while stopped writes nothing; it takes the lock again or goes on as a replica. |
| **`test_topic_var_replace`** | `topic_var.json` is replaced through a `.new` file and a rename, never truncated. |
| **`test_key_reborn_pages`** | An iterator open while its key is deleted and written again forgets the old segments. |
| **`test_open_list_history`** | A keyless `tranger2_open_list()` names a key that does not load in `load_failed_keys` and goes on. |
| **`test_unreadable_at_open`** | A store damaged before the start: the key is flagged at the cache build, and every load says `load_failed`. |
| **`test_uncommitted_append`** | An append whose md2 row is never written: ignored with a warning, cut back, a short write logged as such. |
| **`test_torn_md2_tail`** | An md2 that ends in a part of a row: cut back by a master, read whole by a replica; the shape 7.25.4 left after a torn row is flagged, not cut. |
| **`test_md2_short_write`** | An md2 row written short whose cut back fails: the append is refused, and the next one cuts and goes in. |
| **`test_md2_read_error`** | A read of the first or last md2 row fails at the cache build: the file is flagged until it reads again. |
| **`test_nul_escape_record`** | A record whose string holds NUL characters is read back, also after a restart. |
| **`test_torn_tail_check_fails`** | A check of a torn tail that cannot RUN refuses the append and does not flag the file; records larger than the reader's memory block are flagged, not cut. |
| **`test_cmp_file_ids`** | `cmp_file_ids()` orders two md2 file ids as their names sort, for fixed cases and 2 000 000 random pairs. |
| **`test_unlistable_dirs`** | A key directory or `keys/` that cannot be listed: the key is flagged, the topic does not open, and both recover once the directory can be listed. |
| **`test_unlisted_relist_once`** | A key directory whose `readdir()` fails is flagged once, and listed again only when it changes. |
| **`test_tr_dt_unknown`** | A topic on a filesystem whose `readdir()` gives no `d_type`: the keys are found with `lstat()` in `keys/`, and the cache is whole. A key whose `lstat()` fails with `EIO` fails the listing, and the topic does not open (by `--wrap=lstat`). A symbolic link in `keys/` is not a key, with `d_type` or without it. A key directory of mode `r--` is flagged unreadable the same way without `d_type` as with it (up to 7.25.4, without `d_type`, it loaded EMPTY, with no log). |

| **`test_archive_shipping`** | The archive of `tranger2_archive_key_files()` shipped by the yunos `archive_sender` and `archive_keeper`, in one process. Native, over `tcp://127.0.0.1:7755`: two batches found by the scan when the sender plays, a third told by its ready mark, and a fourth the keeper already has (shipped again, and answered `stored` at its `end`: the keeper hashes the batch it has, and the content is the same). rsync, to a local directory, with `max_archive_bytes` of one batch and two waiting: the oldest is removed unshipped (a WARNING), the other arrives with its manifest. Every batch that arrives is whole, and no batch nor mark is left in the archives. A batch of the same name planted in the backup first, with the same files and sizes and other bytes, is refused (its sha256 are not of the batch that arrived) and kept untouched, and the sender keeps its own. The link is mutual TLS (`tcps://`, a CA and certificates made by the test with `openssl`): a sender with the client certificate of another node, and one without a certificate, both naming the node, are refused, and keep their batch. |
| **`test_archive_shipping2`** | `archive_keeper` against a fake `archive_sender`: the driver writes the frames of the native transport by hand over `tcp://127.0.0.1:7758` and checks each answer in order. Refused: an `end` whose sha256 is not of the bytes that arrived, one with fewer sha256 than files, one not in lowercase hex, data of a file before the files before it are whole (a frame of that batch already on its way is answered, and not counted again), an empty batch, and a batch of the name of one kept with the same files and sizes, other bytes and their own right sha256 (the kept batch untouched). Stored: a good batch, and the same again as already here. Over a second link: a batch in flight on the first is refused to it and stored with the first link's bytes (up to 7.29.1 the first link was answered `stored` with an empty file), and a batch whose link went silent for `stale_session_timeout` is taken over by the other link. A link silent for `stale_session_timeout` whose batch declared 3/4 of the free space is dropped at the `begin` of another batch, and its reserved space is free again (up to 7.29.2 it was reserved for ever, and every `begin` was *"does not fit"*). A refused batch leaves nothing in the backup, nor its copy aside; the test fails without the check of the sha256. |

**Source:** `tests/c/timeranger2/`, `tests/c/tr_dt_unknown/`,
`tests/c/archive_shipping/`. Each case of the timeranger2 tests is described
in `tests/c/timeranger2/README.md`.

## TR_MSG & TR_QUEUE

| Binary | Description |
|--------|-------------|
| **`test_tr_msg1`** | Message topics: iteration, key matching, instance retrieval. |
| **`test_tr_msg2`** | Stress variant: 1 000 devices × 100 traces. |
| **`test_tr_queue1`** | Queue topic: enqueue / dequeue with time-based keys over a multi-day period. |
| **`test_tr_queue_backup_failed`** | A backup that fails (the backup name taken by a file): `trq_check_backup()` / `tr2q_check_backup()` answer `-1`, the queue keeps its topic, and appends, reads and acks work after it; nothing leaks. And a backup whose new topic cannot be created after the move (its `mkdir` fails, `ENOSPC`): the backup is moved back, the queue keeps its topic whole, and the next call backs it up. A create that fails only at the `mkdir` of its `keys/` answers `NULL` and leaves nothing, and a backup that meets it keeps the queue's topic. A backup that fails and cannot open the topic again (`topic_desc.json` of mode `0` for a moment): the queue has no topic, a read and an ack fail (said once), and the queue takes its topic again by name as soon as it can be opened, and backs up at the next call (`tr_queue` and `tr2q`). While the topic still cannot be opened, the next calls log nothing. The same when what stopped the open is outside `topic_desc.json` (`keys/` of mode `0`): said once, and the topic is taken again once `keys/` can be listed, or as the tranger has it open again. And a tranger with `on_critical_error` `LOG_OPT_EXIT_ZERO` (the broker's queues): a backup whose create fails does not exit, and moves the backup back; a plain create whose `keys/` cannot be made exits (in a child process) only after it removed what it made, and the next create makes the topic whole. The CRITICAL *"Cannot create TimeRanger subdir. mkrdir() FAILED"* names the real cause (*"No space left on device"*, not *"Success"*). |
| **`test_tr_queue_load_failed`** | A queue (`tr_queue` and the mqtt `tr2q`) whose load cannot read every pending message: the load returns `-1`, `first_rowid` is kept, and the backup is refused until a load reads them all. |
| **`test_tr2q_queued`** | The queued messages of an mqtt queue: one whose content cannot be read stays queued (`tr2q_move_from_queued_to_inflight()` answers `-1`), and `tr2q_check_backup()` does not back up while a message is queued. |
| **`test_pkey2_empty`** | `msg2db_append_message()` refuses a record whose `pkey2` is empty and writes nothing. |
| **`test_msg2db_load_failed`** | An id whose history did not load whole: reloaded newest first up to the damage; a `pkey2` whose newest message is in the damage is absent (`msg2db_id_incomplete()`), never an older message. |
| **`test_msg2db_topics_left_out`** | A topic `msg2db_open_db()` leaves out (a name that breaks the name rule, no `pkey2`) is said once and gets no list and no index, also when the tranger had it open before (up to 7.26.9 it got both, without a word). |
| **`test_msg2db_column_default`** | A message does not spend the `default` of a column: twenty messages carrying every field log nothing and leave the schema's defaults (a string, an integer, a boolean; persistent and volatile) unchanged; a message without the volatile fields gets their defaults, also at the reload (up to 7.28.7 the first message that carried a field freed its default, and every one after it logged *"BAD json_decref()"*). |

**Source:** `tests/c/tr_msg/`, `tests/c/tr_queue/`, `tests/c/tr_msg2db/`

## TreeDB

| Binary | Description |
|--------|-------------|
| **`test_tr_treedb`** | Schema creation, user/department/compound structures, node CRUD, and state snapshots (foto files). |
| **`test_tr_treedb_delete_instance`** | Instances of a `pkey2` key, each scenario in its own database with a reopen: `treedb_delete_instance()` and a tombstone that fails; a delete of a parent sees the children of every instance, a delete of a child unhooks every instance, a deleted instance leaves the hooks of its parents (the primary takes its place) and hands its children to the primary, a refused forced delete puts every instance back, a save of a node no index holds is refused, and after a reopen the instance of the primary IS the primary node (an update through it lands, and survives the next save of the primary); a save of a node whose `pkey2` value changed in place is refused, and a create indexes a defaulted `pkey2` by the value its record holds. The instances of a CHILD: an unlink clears the ref in every instance of the child key, a delete of a parent sees (without `force`) and clears (with it) the instances that name it and no hook holds, a forced delete leaves no ref in any instance, what those did is put back when the write does not go, and a relink of an instance its parent does not hold logs nothing. With a snap active, a key created after it (instances and no primary) is deleted quietly, and its create takes the slot; with two `pkey2`s a save keeps the primary in its slot; the gc holds the asset an instance names. A forced delete, and a delete taken back, keep the newest record of each key they save on the instance that wrote it, so the primary of the next reload does not change; a relink through a dict hook over a string fkey leaves the sibling instance in the old parent's hook; a snap shot that clones a tagged record writes the newest record of the key again after the clone. |
| **`test_tr_treedb_load_failed`** | A topic whose keys cannot all be read: treedb loads the other keys, remembers the ones that failed, and refuses a create of such an id, and snapshot operations when `__snaps__` did not load whole. The recovery, after the key is deleted, needs no reopen. |
| **`test_tr_treedb_failed_save`** | See *Subscriptions & link events*. |
| **`test_tr_treedb_files`** | The `file` column and `__assets__`: the bytes are not in the record, the id is the hash of the bytes (a client that lies is refused), and the index is in memory. A `file` column that is not `fkey`, or whose id holds a `^` (it is in the hook the column derives, `as_<topic>_<col>`: every link of a file failed), is refused before the topic exists. |
| **`test_tr_treedb_hook_hygiene`** | Hooks of a versioned (`pkey2`) parent: an unlink through the reverse hook, a link made twice kept once, a forced delete of a parent with many children. |
| **`test_tr_treedb_hook_rename`** | A hook renamed in the parent's schema: the child's `fkey` mark is derived, never read from disk, so the links load under the new name; a ref that names the old hook, in an array or a STRING fkey column, is removed by a clean or a forced delete. |
| **`test_tr_treedb_immutable`** | Immutable topics and records: a delete is refused, also with `force`, and the mark survives updates and a reload. |
| **`test_tr_treedb_relink`** | A link into a single-valued fkey replaces the old one, and the old parent's hook forgets the child. |
| **`test_tr_treedb_rowid`** | The id a `rowid` topic hands out is never one that exists, also after deletes and updates. |
| **`test_tr_treedb_schema_parse`** | `parse_schema()` of a column with no `flag` (it crashed), and the columns and keys [`treedb_create_topic()`](#treedb_create_topic) refuses before the topic exists: a column both `hook` and `fkey`, no `id` column, no columns, bad pkey2s (named twice, `id`, a backtick, the tkey: an ERROR, a WARNING inside the window of a peer's data), a list id longer than `NAME_MAX` (an ERROR always, no directory on disk), a `^` in the name of the topic or of a hook and a pkey2 of no column; and a node `id` that is not a string. |
| **`test_tr_treedb_snap`** | `treedb_shoot_snap()`, `treedb_activate_snap()` and `__clear__`, and the primary promoted at the next reload. |
| **`test_tr_treedb_snap_clone`** | A record already tagged by a snap gets a clone for the new snap, and memory agrees with a reload. |
| **`test_tr_treedb_update_instance`** | An update is seen through the `pkey2` index at once (`treedb_get_instance()`, `treedb_list_instances()`). A NUL in the id or in a pkey2 value is refused by a create and by an update from C, with an ERROR, and nothing is written (*"Invalid id or pkey2 value: it holds a NUL"*); so is a NUL in an fkey reference, at the start or after its three parts, by a create and by `treedb_update_node_and_links()` (*"Invalid fkey reference: it holds a NUL"*; up to 7.26.9 the create saved the part with no parent, or linked it to the parent C read). |
| **`test_c_assets`** | [`C_ASSETS`](#gclass-c-assets): the asset id is the sha256 of its bytes, the same bytes are one asset, `get-asset` inline or as a signed url, orphans, `import-assets`, `gc-assets` and its refusal while a snap is active, the `-403` of the authz gate; the bytes of the gbuffer door go into a record of the command's own, and the sender's `record` keeps no gbuffer. |
| **`test_c_treedb_system_schema`** | `__system__`, the meta-treedb of [`C_TREEDB`](#gclass-c-treedb): the projection of a schema at open, `save-schema`, `apply-schema`, `delete-treedb`, and the warnings of a store that runs a topic ahead of its file. What a peer hands [`C_TREEDB`](#c-treedb-peer-data) is refused before anything is made, a WARNING each, and the same from C code an ERROR (Tests 13d2 to 13d7): a `treedb_name` of the wrong type, a treedb not open here, unknown and system topics to delete, topics of a wrong shape (no columns, a FILE of the store, `.blobs`, pkey2s, names too long for the lists of the indexes or for `__system__`, two `file` columns of one `__assets__` hook), a `filename_mask` that is a path, names of the system (`__system__`), a peer's treedb name that is hidden or a file of a store (`.x`, `..x`, `__x`, `x.json`, `x.bak`), a peer's schema whose `id` is not the name of its treedb, `exit_on_error`, `import_root` and `impose_c_schema` from a peer, one name per topic, values and versions of a wrong type, seeds without a string id, a store that is not the treedb's or is held, `delete-treedb` of `x/../<T>`; `save-schema` and `apply-schema` of a topic the treedb would refuse. A C caller's `open-treedb` opens as before, a topic already on disk with bad pkey2s or a list id past `NAME_MAX` included (each fault an ERROR), and so does its `create-topic`; `close-treedb force=1` from a peer only for a treedb that same user opened, and never, playing or paused (a real `gobj_pause()` that keeps the treedbs, as `mqtt_broker` does), for a treedb the C code opened; a peer's `open-treedb` of a treedb the code opened and closed is refused; a peer's topic that a msg2db of the store holds or names in its schema file is refused to `create-topic`, `delete-topic` and `open-treedb`, while a topic the treedb dropped comes back with its records; two `main_topic` marks, or one on a topic not hooked to itself; a peer's `save-schema` past version 2147483647; a tkey date past 44 bits refused to `create-node` and `update-node` (and to C code), and appended cut, with an ERROR, by `tranger2_append_record()`; the seeds of an external open are WARNINGs at every start of their `C_NODE`. See its `README.md`. |
| **`test_treedb_schema_fidelity`** | Every real schema of the tree through `__system__` and back: each attribute the literal declares comes back with the same value. |
| **`test_c_treedb_literal_wins`** | [`C_TREEDB`](#gclass-c-treedb): a schema from C newer than the schema file in use wins whole; `__system__` is projected from it whole; the operator work it discards is reported once (`withdrawn_at_open`); an unfinished projection is recorded and completed at a later open, also after the process is killed at any write, once or twice. `save-schema` of a node of two parents (its place not written, `places_not_written`; unshared again by an edit of the other parent, it is no draft of the parent that remains), of two topics of one name (refused, `data.twins`), and of a draft that shifts its siblings (one save publishes them all, and `saved-schema`'s `draft_changed` names them first); an open that installs a newer literal and does not open keeps the saved schema; a save after an edit of a topic an older release left publishes it whole, its `id` column included. The scenarios are listed in its `README.md`. ctest runs them in seven groups, each a test of its own in a store of its own (`test_c_treedb_literal_wins/early`, `/late1`, `/late2`, `/dcd`, `/dcn`, `/o4`, `/o4xa`; `--group=<name>`), so `ctest -j` runs them at the same time. |

**Source:** `tests/c/tr_treedb/`, `tests/c/tr_treedb_delete_instance/`, `tests/c/tr_treedb_load_failed/`,
`tests/c/c_treedb_literal_wins/`, `tests/c/tr_treedb_*/`, `tests/c/c_assets/`,
`tests/c/c_treedb_system_schema/`, `tests/c/treedb_schema_fidelity/`

## gobj-c helpers and commands

| Binary | Description |
|--------|-------------|
| **`test_helpers`** | String, file and directory helpers: `split2()`, `save_json_to_file()`, `rmrdir()` / `mkrdir()` with links, deep trees and entries that vanish during the walk. |
| **`test_rotatory`** | `rotatory_remove_old_files()` and the rotatory log writer: size rotation, a full disk, a clock set back, a write that fails and the file opened again (no newfile callback for the same file), a keep_all size rotation whose rename fails (tried once, every record kept), a fixed name never emptied, an `MM` mask judged by its month; a new file whose open fails keeps its newfile callback for the next open that works (on a full disk too), the file of the day before is never size-rotated at a new name, and the keep_all rename retry runs on the monotonic clock (a wall clock set back or forward does not move it). The size limit is in bytes: a file of exactly the limit is not rotated, one 2 bytes over it is at the next record. `exit_on_fail` is for [`rotatory_open()`](#rotatory_open) only: with `TRUE`, a later open that fails (a new day, a removed file, the reopen after a failed write, a truncate) prints one line, is tried again at the next record and never exits (each case in a child process); the open itself still exits with `TRUE`, and returns `NULL` with `FALSE`. |
| **`test_audit_record`** | The audit record builder of `yuneta_agent`: secrets redacted, `content64` never written, the command word read as the parser reads it (`COMMAND=list-yunos` with a kw `command=delete-yuno` gets the full record), a secret inside a JSON text inside a JSON text redacted (up to 8 levels of escapes; deeper is written as its size and sha256), whatever quotes come before it or cut it (`x="{\"password\":…}"`, `'{\"password\":…}'`, `password=\"two words\"`), and the credentials after `Basic `; an escaped key with a blank or a slash (`\"secret key\":`), a quote inside a quoted secret (`password="a\" S"`, the shell's `'\''`), a JWT before a `.`, a write-attr whose names carry `\u` escapes or that comes as JSON text in a kw string; a deterministic fuzz of 20 000 generated commands (the secret in 24 quoting shapes among 17 kinds of noise, keys with blanks, slashes and `\u` escapes, write-attrs) writes no secret; what `mask_secrets_inline()` masks the record does not write -- the same function, `mark_secrets_inline()` (7.27.0+; its own JWT was three parts exactly): a JWE, a JWT of two parts, a `name: value`, every word of a password with blanks, and 20 000 random texts; a masked run that ends a text of any length modulo 8 (the map of masked bytes is not read past its end, which AddressSanitizer caught); hard shapes stay linear, under ASan too (up to 7.26.9 the search for `attribute` was quadratic there, see below). The routing of a peer (`user`, each hop of `source`, `console_purpose`, the console name of `write-tty`) is redacted like any other string, and a field of it longer than 1024 bytes is written as its size and sha256. |
| **`test_sha256`** | [`sha256_digest()`](#sha256_digest) / [`sha256_hex()`](#sha256_hex) and the incremental [`sha256_init()`](#sha256_init) / [`sha256_update()`](#sha256_update) / [`sha256_final()`](#sha256_final) / [`sha256_final_hex()`](#sha256_final_hex): the vectors of FIPS 180-4 (the empty input, `"abc"`, the two messages of 56 and 112 bytes, a million `a` in parts of 1000), given whole and a byte at a time; every length from 0 to 300 bytes cut in two at every place and in parts of 1 to 70 bytes gives the digest of the whole; a context init'ed again starts from nothing; a buffer of 64 bytes is refused (logged), and a refused `sha256_final_hex()` leaves the context to a final that works. |
| **`test_build_path`** | [`build_path()`](#build_path): segments joined with one `/`, and a `..` clamped at the root (no path escapes the base). |
| **`test_gbuffer_guards`** | A NULL gbuffer given to its pointer accessors answers NULL, not a crash, and so does `config_gbuffer2json()` of one (the agent's `create-config` of a `content64` that is not base64 was a SIGSEGV up to 7.26.9); bytes from a peer that are not json are a warning (`gbuf2json_from_peer()`, `anystring2json_from_peer()`), never logged as they came (`json_error_for_log()` drops the bytes jansson quotes near the error); a `json2gbuf()` that cannot dump the json whole (a cycle, with or without a gbuffer of the caller; a gbuffer too small) logs an ERROR and hands back the start (up to 7.28.7 it said nothing). |
| **`test_glogger_utf8`** | The logger never writes a record that is not valid UTF-8 JSON: an invalid byte is escaped as `\u00XX`, valid multibyte text stays readable. |
| **`test_trace_tab`** | [`tab()`](#tab), the indentation of a machine trace line, at a REAL depth (an action that sends the same event to its own gobj, 12 levels down): four spaces per level, one at depth 0, never more than `bflen - 2`, only spaces; a buffer of 1 or 0 bytes or `NULL` logs an error and is not written past its size (up to 7.28.7 it was overrun, and `NULL` crashed). `trace_machine()` at depth 12 carries the 48 spaces: it took the indentation through a 40-byte buffer, which capped it at 38. |
| **`test_command_authz`** | The per-command authz gate (`SDF_AUTHZ_X`): off by default, a command without the permission is refused `-403` when on, a command to itself and a granted one run; the permission is asked BEFORE the parameters (a bad parameter without it is `-403`, with it `-1`), and the checker is asked for the command's name, not its line. |
| **`test_command_delete_user`** | `delete-user` of `C_AUTHZ`: a user with roles needs `force`, an immutable seed user is never deleted. |
| **`test_command_shutdown`** | `shutdown` answers first and stops the yuno after the answer left. |
| **`test_register_refused_gclass`** | A gclass of the yuno that `gclass_create()` refuses (a `SDF_SECRET` attribute named `vault`) stops the yuno before it runs: the test forks the yuno and checks it exited with `-1` (255), the `exit(-1)` of `yuneta_entry_point()` after its ERROR. Up to 7.26.9 the yuno ran (its driver played and the child answered 7). |
| **`test_jwt_alg_confusion`** | The JWT algorithm-confusion forgery: an `HS*` token verified with the bytes of an RSA/EC public key is refused; the `jwks_*` keyring API accepts a NULL set, `jwks_item_count`, `jwks_find_bykid` (and a NULL kid), `jwks_item_add` and `jwks_item_free_bad` included (not guarded upstream either). |
| **`test_gbmem_realloc_refused`** | A `gbmem_realloc()` refused because the new size is larger than the largest block leaves the old block valid and tracked: the later free logs nothing and the memory counter stays right. |
| **`test_command_parameter_types`** | What a command parameter takes, whatever gives it -- the command line, kw or its default ([`command_parser()`](#command_parser)): a string, an object, an array of that shape; an integer, a real, a boolean as a number or a boolean, never an object or an array, and text read as one (`yes`, `off`, `1`; not `maybe`); a `DTP_JSON` anything, text that is not json a string unless it starts as json and breaks; a `null` is no value, and so is an empty text in kw for a number (absent with no default); the line has the last word over a refused kw value. The handler gets the EXACT type (`1` to a boolean is `true`, `true` to an integer 1, `2.0` is 2, `2.7` refused), and a number as text is one of the JSON grammar, the whole text, and in its range (`nan`, `inf`, `.5`, `5.`, `+5`, `007`, past 64 bits refused; `-0`, `1E5` taken), read in the `"C"` locale whatever `LC_NUMERIC` is (`2.5` with the `LC_NUMERIC` of `es_ES`, when that locale is installed). `command_parameter_from_text()`. Each refusal before the handler, `-1` naming the parameter: an ERROR from C code, a WARNING from a peer (`__username__`). |
| **`test_command_binary_kw`** | A command, and `build_stats()`, whose kw carries a `gbuffer`: the handler's kw holds a reference of its own, and the caller's references are intact after the command (no *"BAD gbuf_decref()"*). |
| **`test_dir_array_nomem`** | A directory listing that cannot keep an entry (no memory), or whose root cannot be opened, answers `-1` with the listing empty, and logs it: `find_files_with_suffix_array()`, `walk_dir_array()`, `get_ordered_filename_array()`. |
| **`test_dir_listing`** | The answer of the agent's `dir-*` commands: a tree that cannot be listed answers `-1` with a comment that names the directory and sends to the log (it does not read the process-global last message), never an empty list. |
| **`test_dir_read_error`** | A directory that opens and cannot be READ (`readdir()` fails, `EIO`, by a `--wrap=readdir`) fails the listing: `find_files_with_suffix_array()`, `walk_dir_array()` (root or subdirectory) and `walk_dir_tree()` answer `-1`, empty, logged. `re`/`pattern` `NULL` lists every entry; a root of mode `0` given to `walk_dir_tree()` is logged. A SUBdirectory whose `opendir()` fails with `EMFILE` (a transient cause, by a `--wrap=opendir`) fails the walk; one with `EACCES` is skipped with a warning. A callback that returns `FALSE` in a subdirectory stops the WHOLE walk. Paths longer than `PATH_MAX` fail the walk (*"Path too long"*; up to 7.25.4 a crash), also in `find_files_with_suffix_array()` without `d_type` (the `readdir()` wrap hides it); a tree of 1100 levels fails it too. An entry whose `lstat()` fails with `EIO` fails the walk and `find_files_with_suffix_array()` without `d_type` (by a `--wrap=lstat`); one whose `lstat()` fails with `EACCES` is listed by `find_files_with_suffix_array()` without `d_type`, as with it; a subdirectory whose `opendir()` fails with `ENOTDIR` or `ELOOP` is skipped with a warning. `rmrcontentdir()` and `rmrdir()` of a directory whose `readdir()` fails answer `-1` and log *"readdir() FAILED"*. A failed `mkrdir()` leaves the cause in `errno` after its own log (`ENOTDIR`, `ENAMETOOLONG`). |
| **`test_secret_attrs`** | A `SDF_SECRET` attribute is read, written and saved as usual and SHOWN masked, wherever it is shown: `view-gobj` (`C_IEVENT_SRV`'s `http_cookie`), `view-config` (a service's kw, a child's, a `global` key, the `__json_config_variables__` that feed one), the `create_delete2` trace, the `commands` trace with `ev_kw` (a key=value, a positional and a kw secret, the kw the parser expands, the `command` that `command-yuno` carries), a secret by its VALUE under any name (a JWT, the token after `Bearer `), the first word of a line out of session, a peer's remote log line (capped, masked), the frame of an event that is not public (one WARNING). A json that does not parse is never logged as it came (`anystring2json()`, `string2json()`, `json_config_string2json()`, an attribute written as text): not its bytes, not the bytes jansson quotes near the error; an HTTP json body that breaks and an inter-event without its routing are the peer's, a WARNING, masked (7.27.0+). A capped log line masks the password of a URL whose `@` lies past the bytes it judges, at every cap (7.27.0+). The masking is linear, by its slope: each shape at 16, 64 and 256 KB, the ratio of the times under 16^1.15 (a quadratic one gives 256). And the persistent-attrs file (`dbsimple.c`): made `0600` when it is loaded; a planted symlink or hard link not read nor written through; a file of another user not loaded (root's only if nobody else can write it, the chain of directories closed from `/`); a save that stops half way writes the old content back. |
| **`test_secret_traffic`** | The `C_TCP` `traffic` trace of a frame sent with `"__secret__": true` (an SMTP `AUTH` line) prints `<N bytes hidden>`, never its bytes, whether the frame was written at once or queued while the client connected; a normal frame is still dumped. A RECEIVED frame's dump masks what can be told in its bytes, the password of a URL whose authority reaches the end of the bytes included (7.27.0+: the `@` may come in the next read). |
| **`test_command_secret_positional`** | `gclass_create()` refuses a command table where a required secret (`SDF_SECRET`, or a secret's name) is followed by another required parameter: a secret written with blanks would spill into it, shown in the traces. A required secret as the last required parameter is accepted. A `SDF_SECRET` attribute must have a secret's name (`vault` refused, `vault_secret` accepted). |
| **`test_attr_types`** | A value written to an attribute (`gobj_create()` from the config, `gobj_write_attr()`) is read as its declared type by the grammar of the command parser ([`command_parameter_from_json()`](#command_parameter_from_json)): a json string given to a boolean, an integer, a real, a list or a dict is read from its text (`"yes"` true, `"42"` 42, `"2.5"`, `"[1, 2]"`); a number as the exact type (an integer to a real is its real, `true` to an integer 1, `2.0` 2). What does not read as one (`12abc`, `2.7` to an integer, an object to an integer, `nan`, `maybe`, a dict to a list, an empty text, a null) is refused with one ERROR naming the attribute, its type and why, the value masked and capped (a `SDF_SECRET` one never shown), and the attribute keeps its value. Up to 7.26.9 a string was read from its JSON dump, quotes included: `"42"` was 0, `"true"` false. And every case of `kernel/js/gobj-js/tests/fixtures/attr_types_parity.json`, which gobj-js's `tests/attr_types.test.js` reads too: C and JS read an attribute value the same way. The table is read from the `kernel/js/gobj-js` submodule of the source tree the test was built from (its path is compiled in, `YUNETAS_BASE` is not read): a tree without it (a tarball of yunetas alone) runs the other checks, says so on its output (*"SKIP the parity table of gobj-js: ... not found"*), and exits 77, which ctest reports as *Skipped* (`SKIP_RETURN_CODE`), not as *Passed*. |

**Source:** `tests/c/helpers/`, `tests/c/gbuffer/`, `tests/c/command_binary_kw/`,
`tests/c/command_parameter_types/`,
`tests/c/build_path/`, `tests/c/glogger_utf8/`, `tests/c/gobj_trace_tab/`, `tests/c/command_authz/`,
`tests/c/command_delete_user/`, `tests/c/command_shutdown/`, `tests/c/register_refused_gclass/`, `tests/c/libjwt/`,
`tests/c/secret_attrs/`, `tests/c/command_secret_positional/`, `tests/c/attr_types/`

## Keyword matching

| Binary | Description |
|--------|-------------|
| **`test_kw1`** | `kw_match_simple()` with strings, booleans, integers, and floats; and the formats of the logger: the precision caps a value, the length modifiers (`%zu`, `%llu`, `%jd`, `%hhd`), a `%` with no conversion, and `%m`, which takes no argument (up to 7.26.9 it took one, and the key after it crashed the process); `utf8_prefix_len()`, and a cap of the logger and of a `json_sprintf()` comment that never cuts a UTF-8 character; `utf8_columns()` and `utf8_prefix_columns()`, and a table cell padded by characters that is as wide as an ASCII one; `kw_find_path()` of a path of 100 segments and of a 300-byte segment (found, nothing logged), and `json_refcounts_report()` of a shared node, a cycle, and a cycle whose refcounts are corrupted below the truth (it ends, each json counted once, not `truncated`, and REPORTED as corrupted at the path of the reach too many), a dict and a string `json_object_set_new()`'d into two places (corrupted, each), and a sound json held by the caller and two hooks (none); a list of a million integers walked whole under the default `MAX_BLOCK` (up to 7.26.9 a frame was reserved for every child, 32 MB for the root: `truncated`, one json counted), and 40 000 records, more frames than one chunk holds; `json_nul_fault()` of a NUL down a record, of a clean one, and of a json 3000 levels deep. |
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

`cmake --build build --target <test>` alone, without step 1, links the test
against the OLD installed copy: the root `build/` does not build the library
(unless it was configured with `-DENABLE_SDK=ON`). Step 2 holds only because
the test names the archive by its full path (`${YUNETAS_KERNEL_LIBS}`, or
`${LIB_DEST_DIR}/libfoo.a`): a bare `libfoo.a` is a `-l` search, and the test
is not relinked at all. Up to 7.26.3 this very test, and 17 others, named
them bare; `scripts/check_test_links.py` fails on one.

Before 7.25.5 step 2 could also print only *"Built target"* and keep the old
library. `install()` keeps the mtime of the file it copies, in whole seconds,
which is the time the archive was BUILT: a test linked between that moment
and the install looked newer than the archive. `tools/cmake/project.cmake` now
stamps an archive that changed with the time of its install, rounded up to
the next whole second. An archive that did not change is not copied
(*"Up-to-date"*) and relinks nothing.

## A test that opens more than 1024 files

systemd starts what it launches, a desktop terminal included, with a soft limit
of 1024 open files, whatever the hard limit is. A test run from that terminal
fails with *"TOO MANY OPEN FILES"*, `"current soft limit": 1024`, while the
same test passes from another shell. A process can raise its own soft limit
up to the hard one without privileges, so a test that opens many files (a
topic with more than 1024 keys, many treedbs) calls
[`raise_open_files_limit()`](#raise_open_files_limit) first:

```C
int result = raise_open_files_limit();
result += do_test();
```

`test_iterator_index`, `test_c_treedb_literal_wins` and `perf_c_treedb` do.
To give every program started from the desktop a higher soft limit, set it
for the systemd user manager, then log in again:

```bash
sudo mkdir -p /etc/systemd/user.conf.d
printf '[Manager]\nDefaultLimitNOFILE=1048576:4000000\n' | \
    sudo tee /etc/systemd/user.conf.d/nofile.conf
```

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

# 2. configure a separate tree WITH the sanitiser, and with the SDK in it
#    (ENABLE_SDK: the root tree builds only the tests by default)
cmake -S . -B build_asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_SDK=ON \
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

**A time limit fails under ASan that is not a slow algorithm.** ASan's
interceptor of `strstr()` / `strcasestr()` measures the WHOLE haystack at every
call (its `StrstrCheck`), so a scan that calls one per match turns quadratic
under ASan only. Up to 7.26.9 the agent's audit record looked for `attribute`
with `strcasestr()`, and `helpers/test_audit_record`'s 1 MB shapes took 20 s
against 0.24 s for 100 KB and failed their limits, while the plain build took
0.04 s; it now searches within the length it was given (0.06 s for 1 MB under
ASan). If another test shows it, `ASAN_OPTIONS=intercept_strstr=0` tells the
two apart -- the scan is linear again and every other check of ASan stays
on --, and the fix is a search bounded by the length, not the option. A test of
speed is better written as a SLOPE than as a limit: `secret_attrs/test_secret_attrs`
masks each text at 16, 64 and 256 KB and asks the ratio of the times, which a
load or a sanitizer leaves alone (it took 1 MB in under 500 ms up to 7.26.9,
and failed under ASan with the machine loaded).

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

- **Performance benchmarks** live under `performance/c/` — `perf_c_tcp`, `perf_c_tcps`, `perf_yev_ping_pong`, `perf_yev_ping_pong2`, **`perf_auth_bff`** (live throughput over the BFF in the ping-pong style: a run is 10 s by default, about 180 000 ops on the reference box), and the persistence ones: **`perf_timeranger2`** (the open of a store, the create of topics, a tm query), **`perf_tr_treedb`** (treedb writes), **`perf_c_treedb`** (the open of a dynamic-schema treedb by `C_TREEDB` in a store of 40 treedbs) and **`perf_rotatory`** (one record of the log files and of the agent audit). Each prints one line of JSON per result; ctest runs them small, and `performance/c/README.md` keeps the figures of each release.
- **Stress runners** live under `stress/c/` — `stress/auth_bff` drives concurrent BFF login / refresh / logout cycles to expose races between the pending queue, the watchdog and the flush-on-disconnect path.
