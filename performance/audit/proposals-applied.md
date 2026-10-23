# The proposals, applied

The rounds left a list of proposals for a decision: changes that make the SDK
faster but change something somebody can see, touch a third-party library, or
add a structure to a consolidated module. The user decided to apply them
all. This document says what each one changed, what it gained, and the two
that were not applied: one that is not safe as it was proposed, and one that
was a wrong finding.

Every proposal is a commit of its own, and its message says what changes for
a caller. They come in two batches:

- **The first two, with two new benchmarks**, applied on top of round 4:
  `C_WEBSOCKET` writes a server frame in one write, and the MQTT broker reads
  the subscriber's session without copying it. `performance/c/` got the two
  benchmarks that round 4 had written outside the tree: `perf_ievent`
  (inter-yuno round trips) and `perf_mqtt` (MQTT QoS 0).
- **The rest**, applied on top of the first batch.

## The first batch

| Commit | Change | What a caller can see |
|--------|--------|-----------------------|
| `perf(performance): two benchmarks, inter-yuno round trips and MQTT QoS 0` | `perf_ievent` and `perf_mqtt`, `RUN_SERIAL` ctest targets with their own ports and directories; they fail on any warning or error logged, or memory not freed. | Nothing: two benchmarks. |
| `perf(c_websocket): a server frame in one write` | A frame of up to 64 KB of payload goes to `C_TCP` in one buffer, its header and its payload. Written apart, the peer often read the header alone, and armed and cleared its payload timer for it. A bigger frame, or a buffer that cannot be made, goes as before. | The bytes on the wire are the same. `C_TCP`'s `txMsgs` counts the frame as one message where it counted two, and a `traffic` trace shows one dump. A secret gbuffer and the kw's `__secret__` are carried over. |
| `perf(c_mqtt_broker): read the subscriber's session without copying it` | `subs__send()` reads only `_gobj_channel` of the session, a plain column (no hook, no fkey, not hidden): it reads it in the treedb's node (`treedb_get_node()`), where it built a collapsed view of the whole session for every message delivered (`gobj_get_node()`). | Nothing: the same value. A treedb gobj on its way out still answers through `gobj_get_node()`, with its logs. |

Instructions (`callgrind`), round 4 -> the first batch:

| Benchmark | Round 4 | First batch | Change |
|-----------|---------|-------------|--------|
| `perf_ievent`, 100 000 inter-yuno round trips | 60.11 G | 54.32 G | **-9.6%** |
| `perf_mqtt`, 100 000 publications delivered | 27.08 G | 22.16 G | **-18.2%** |

## The rest

| Commit | Proposal | Round | What a caller can see |
|--------|----------|-------|-----------------------|
| `linux-ext-libs: jansson from artgins/jansson, with two speed changes (1.25)` | jansson is cloned from Yuneta's fork, [`artgins/jansson`](https://github.com/artgins/jansson), at a fixed commit (`TAG_JANSSON` in `repos2clone.sh`): upstream v2.15.1 and one commit per change Yuneta carries. They replace `patches/jansson/`. Two of them are proposals: **fast paths for ASCII** (the string dump, the UTF-8 check and the lexer of a string token) and **a faster seeded hash** of the keys (the construction of wyhash in place of lookup3's `hashlittle()`); the third is the fix that was `patches/jansson/0001` (the lexer stops when a save fails). | [2](round-2-timeranger2-treedb.md), [1](round-1-gobj-core.md) | Nothing: the same results, bytes and errors (jansson's 215 tests; a differential fuzz against the previous library, the same output and the same allocations, call for call). The order of the keys does not depend on the hash (an object iterates in insertion order). `linux-ext-libs` 1.25: every node that builds from source reruns `./extrae.sh && ./configure-libs.sh` and rebuilds the SDK and its yunos. |
| `perf(tr_treedb): the id index loads the newest record of each key only` | The open loads the id index of a topic backward and keeps the first record of each key; it now ends the load of that key there (`load_id_callback()` answers -1, "break the load" of the key), instead of reading and parsing its whole history. | [2](round-2-timeranger2-treedb.md) | A damaged OLDER record of a key is no longer read at the open, so it no longer fails the key (`load_failed`); the newest one, which is the node, still does. The pkey2 lists still read the whole history. |
| `perf(tr_treedb): the treedb's own feeds ask for the metadata only, once loaded` | The realtime lists the open leaves on each topic (the id index, each pkey2, `__snaps__`, `__graphs__`) read nothing of a record appended after the load, yet every append built for them the `__md_tranger__` of the record's content (`md2json()`, ~5 000 instructions per save). Once loaded, their `match_cond` says `only_md`. | [2](round-2-timeranger2-treedb.md) | Those lists show `only_md` in their `match_cond`, and their callback gets a NULL record for a realtime append (it never read it). |
| `perf(tr_treedb): the hooks of the load indexed, not scanned per link` | Every link loaded at the open asked whether the parent's hook array already held the child, scanning the whole array: C^2/2 comparisons for a parent of C children. During `load_all_links()` each hook array has a temporary index, built the first time it is asked about and kept up as children are appended. | [2](round-2-timeranger2-treedb.md) | Nothing: it answers what the scan answers (an entry whose key does not fit marks the array to be scanned as before). Outside the load nothing changes. |
| `perf(gobj): a subscription built with its values` | `_create_subscription()` builds the dict of a subscription directly, where `gobj_sdata_create()` set ten defaults parsed from text and then overwrote three. | round 4 | Nothing: the same dict, key for key and in order (checked against the old builder). |
| `perf(gobj): the record of a subscription, read once, for publications and searches` | A publisher keeps, next to each subscription dict, a C record of it (`subs_rec_t`: subscriber, publisher, event, renamed event, flags, and references to `__config__`, `__global__`, `__local__`, `__filter__`), read from the dict when the subscription is made. A publication walks the records, where it read 3 to 8 keys of every subscription by hash on a json copy of the list; subscribe and unsubscribe search the records. | [1](round-1-gobj-core.md), [3](round-3-network.md) | The API is the same: the subscriptions are still the dicts `gobj_find_subscriptions()` and the rest hand out. What changes: a value written into such a dict after the subscription is made is not seen by the framework (a key replaced; a dict changed in place, like the content of a `__filter__`, still is). Nothing in the SDK writes them. Documented in `gobj_find_subscriptions()`. |
| `perf(c_timer): a C_TIMER keeps its subscription to the yuno for its life` | A `C_TIMER` subscribes to the yuno's `EV_TIMEOUT_PERIODIC` at its first start, and the subscription goes with it when it is destroyed; every start subscribed it and every stop unsubscribed it. A stopped timer ignores the tick (its `t_flush` is 0). | [1](round-1-gobj-core.md) | The yuno's subscriptions list every `C_TIMER` that has ever run, stopped ones included; a stopped timer gets `EV_TIMEOUT_PERIODIC` (a `machine` trace of it shows it); a timer keeps the place it took at its first start among the subscribers of the tick. Documented in `C_TIMER`'s page. |
| `perf(ytls): the gbuffer of a TLS read is the size read; mbedTLS reads into the scratch too` | OpenSSL: the gbuffer handed up after a read into the process's scratch is created of the size read (able to grow up to `rx_buffer_size`), not of `rx_buffer_size` (32 KB, zeroed). mbedTLS: the scratch read that OpenSSL got in round 3, and a gbuffer only when something was read, of the size read. | [3](round-3-network.md) | The free space of the gbuffer handed up: a consumer that appends to it grows it, up to the same maximum, instead of finding 32 KB free (none in the SDK does). mbedTLS is not in this machine's `.config`: built in a scratch `ytls` with both backends, and the TLS tests that run on mbedTLS linked to it pass. |
| `fix(c_websocket): the masking key of a client frame from getrandom()` | The masking key of every frame a client sends came from `srand(time(0))` + `rand()`: every frame of the same second carried the same key, predictable from the clock, against RFC 6455 §5.3 (and the process's own `rand()` sequence was reset at every frame). The keys come now from `getrandom()`, 64 at a time. | [3](round-3-network.md) | The keys (unpredictable now). If `getrandom()` fails, it logs an ERROR and the frame gets a key from a weak generator, as it had before. A security fix, not a speed one. |
| `perf(ghttp_parser): the body buffer of a request the size of its first part` | The gbuffer that accumulates the body of a request is created of the size of its first part, up to 4 KB, where it was always 4 KB, zeroed; it grows with the rest as before. | [4](round-4-inter-yuno-http-mqtt.md) | The size of the gbuffer of a body under 4 KB that is handed up (no free space where it had the rest of 4 KB). |

## Not applied

- **One `io_uring_submit()` per turn of the loop** (round 3) **is not safe
  as it was proposed.** It was written (operations armed inside a cycle of
  the loop submitted together before the loop waits, and before anything
  looks at what the kernel has not taken) and the suite rejected it, for two
  reasons:
  - **Writes cannot wait.** Two writes of one socket submitted in one batch
    can complete out of order (one that would block goes async, the next
    one does not), and a writer may reuse its buffer once its write was
    submitted. `test_yevent_traffic2` received corrupted data, and
    `c_tcp_inactivity` and `test_c_ievent_cli_peer_faults` failed with it.
  - **Reads cannot wait either, while an fd can be closed behind the
    loop's back.** A read re-armed in a callback can be followed, in the
    same callback, by the `close()` of its fd: `C_TCP` closes its socket
    itself, not through `yev_loop`. A deferred read then reaches the kernel
    for an fd that is closed, or already reused by another connection:
    with only the reads, accepts, polls and timers deferred,
    `test_yevent_traffic3` failed.

  Deferring only accepts, polls and timers would save little: what an echo
  pays is a read and a write. The batching would need every close of an fd
  to go through `yev_loop`, a change in `C_TCP` and in every gclass that
  closes its own fds. Reverted; not proposed again.
- **`TCP_NODELAY` is already set.** Round 3 said it was not: it is, by
  `set_tcp_socket_options()` (`helpers.c`), which `yev_loop` calls on every
  socket it accepts and every socket it connects. Nothing to do.

## Results

The "before" of this batch: the branch with the first batch (`6dd2092`),
with the jansson of `linux-ext-libs` 1.24. It is built from the old `.c` of
every file the proposals changed (`gobj.c`, `c_timer.c`, `c_websocket.c`,
`ghttp_parser.c`, `tr_treedb.c`, `openssl.c`), linked ahead of the
libraries, and the old `libjansson.a`.

### Instructions (`callgrind`, deterministic), the first batch -> all

| Benchmark | Before | After | Change | Where it went |
|-----------|--------|-------|--------|---------------|
| TLS echo (`perf_tcps_test4`) | 35.20 G | 17.20 G | **-51.1%** | the 32 KB gbuffer created and zeroed per TLS read: 36% -> 1% |
| `test_topic_pkey_integer_iterator2` (reads) | 11.22 G | 4.92 G | **-56.1%** | jansson's parse of ASCII (as measured in round 2) |
| `test_topic_pkey_integer` (appends) | 18.90 G | 12.09 G | **-36.0%** | jansson's dump and parse of ASCII, the hash |
| `perf_tr_treedb` | 25.37 G | 17.44 G | **-31.3%** | the open, 20.7% -> 1.0% (the newest record per key); `md2json()` for the treedb's feeds, 6.5% -> 0.2%; jansson |
| TCP echo (`perf_tcp_test4`) | 15.34 G | 10.61 G | **-30.8%** | `json_object_get()`, 34.7% -> 13.9% (the subscription records); the hash, 13.4% -> 5.5%; the json copy of the list per publication, 3.5% -> 0 |
| MQTT QoS 0 (`perf_mqtt`) | 22.16 G | 17.88 G | **-19.3%** | the hash and the reads of subscriptions |
| Inter-yuno round trips (`perf_ievent`) | 54.32 G | 45.51 G | **-16.2%** | jansson's dump of ASCII (`utf8_iterate()` gone), the hash |
| `perf_timeranger2` | 39.49 G | 36.29 G | -8.1% | jansson's dump of ASCII; the hash gains less with long keys |

The new hash gains most where the keys are short (the TCP echo: 2.06 G ->
0.84 G) and least with long ones (`perf_timeranger2`: 4.04 G -> 3.60 G).
`perf_ievent` and `perf_mqtt` are built here with their auto-kill off, which
`callgrind`'s slowness would trip; their "before" is the same build of the
first batch. The BFF (`perf_auth_bff`) runs for a time, not a count, so it
is compared by time only.

Against the release 7.27.7, after the four rounds and the proposals: the TCP
echo takes **41.14 G -> 10.61 G** (3.9 times fewer), `perf_tr_treedb`
**30.35 G -> 17.44 G** (-42.5%).

### Time (A/B, alternated), the first batch -> all

Being measured.

### The open of a treedb

Being measured.

### Nothing changed

The whole ctest suite: the same 9 failures as the base, all of the
environment (round 1), with the same messages; the treedb ones compared
message by message with the base: identical. The benchmarks of the suite
pass, `perf_ievent` and `perf_mqtt` among them.
