# Performance audit of the C SDK (2026-10)

A review of the C code of the SDK in search of speed, done in rounds. The rule
of the audit: **no change of functionality**. A change is applied on this
branch only when what the code does stays the same (same results, same files,
same bytes on the wire, same events, same logs, same errors); an idea that
would change behaviour, format or API is written down as a proposal, with its
expected gain, and is not applied.

Every round says what was looked at, what the profile showed, what was changed
(each change a commit of its own, so it can be taken or left alone), what it
gained, measured, and what was found and not done.

The audit was closed on 2026-10-24, after fourteen rounds and the proposals.
What it found and did not do is gathered in [What is left](#what-is-left),
at the end of this page.

| Round | Scope | Document |
|-------|-------|----------|
| 1 | `gobj-c`: the allocator, the publication of events, subscriptions, `gobj_change_state()`, `gobj_match_gobj()`. TCP echo **+32%**, TLS echo +26%, BFF logins +12% | [round-1-gobj-core.md](round-1-gobj-core.md) |
| 2 | `timeranger2` and `tr_treedb`: the cache of the keys, file names, `pread()`, the cols of a topic, `md2json()`, the start time of the process. Appends **+16%**, reads +13%, treedb writes -13%, forced deletes -18%. Proposal, not applied: a jansson patch, record reads **x2**, appends +25% | [round-2-timeranger2-treedb.md](round-2-timeranger2-treedb.md) |
| 3 | The network path: the payload timer of every frame (`C_PROT_TCP4H`, `C_WEBSOCKET`, `C_PROT_MQTT2`), the TLS read buffer, `json_pack()` per message, `C_IOGATE`, the websocket unmask. TCP echo **+45%**, TLS echo **+36%**, with an append +27/+28% | [round-3-network.md](round-3-network.md) |
| 4 | Inter-yuno messages (`C_IEVENT_*` over websocket), HTTP (the BFF), MQTT, with two benchmarks written for it: the MQTT message builder, the flags of a treedb node view, the event of an inter-event found by name, `kw_set_dict_value()`, `json2gbuf()`, jansson's allocator under `C_AUTHZ`. MQTT **+18%**; an inter-event is jansson's (2%) | [round-4-inter-yuno-http-mqtt.md](round-4-inter-yuno-http-mqtt.md) |
| P | The proposals the rounds left for a decision, applied on the user's decision (all but one, not safe as proposed): jansson from Yuneta's fork (ASCII fast paths, a faster hash), subscriptions read from a C record, `C_TIMER`'s subscription kept, the TLS read buffer of the size read, the treedb open in O(keys) and without the O(children^2) hook scan, `C_WEBSOCKET`'s frame in one write, the broker's session without a copy. TLS echo **+21%**, TCP echo **+19%**, inter-yuno **+47%** (from round 4), MQTT **+39%** (from round 4), appends **+50%**, reads **+74%**, BFF +26%, treedb open **x12** with a long history, **x19** with 8 000 children in one hook. Against 7.27.7: TCP echo **x2.1**, TLS echo x1.9 | [proposals-applied.md](proposals-applied.md) |
| 5 | What was left of a subscription (nothing to change: the proposals took it off the path of a message), and the reads of a treedb: the options of a view read once and its entries built without `json_pack()`, `decode_parent_ref()` without `split2()`, the flags of a filter read once; jansson's loop key without `snprintf()`. The views of a treedb **-30%** of instructions, **-11% to -16%** of time | [round-5-subscriptions-treedb.md](round-5-subscriptions-treedb.md) |
| 6 | What round 5 left, on the user's decision: `json2gbuf()` with a gbuffer of the size of a small dump (the proposal), jansson's lexer runs for `json_loads()` (what every message received goes through), digits and blanks, in the fork; `perf_tr_treedb_read` in `performance/c/`. Inter-yuno **-17%** of instructions, **+19%** of round trips/s; reads with an iterator -6% of instructions | [round-6-jansson-parse-treedb-reads.md](round-6-jansson-parse-treedb-reads.md) |
| 7 | jansson, in the fork, at the bytes of its lexer and of its dump: a byte read and saved without the generic calls, eight bytes of ASCII at a time in the UTF-8 check and in the dump of a string (a first version tested for a word on every byte and cost more on keys and on non-ASCII text: corrected by a third commit). Inter-yuno **-10%** of instructions, **+7%** of round trips/s; appends **-21%** of instructions, **+9% to +12%** of appends/s; reads with an iterator -6% of instructions | [round-7-jansson.md](round-7-jansson.md) |
| 8 | Standard C only, on the user's decision: jansson's hash on 32-bit words (it used `__uint128_t` where the compiler had it), and no `__attribute__` nor `__GLIBC__` in the SDK's changes. jansson with fewer allocations (the first buckets of a table, short strings, the first table of an array, string tokens), a dump that gathers its output before its callback and checks its loops without a table, and a lexer that reads string tokens and integers without the generic byte calls. Inter-yuno **-21%** of instructions, **+17%** of round trips/s; BFF logins **+12%**; reads with an iterator **+17%**; the views of a treedb **-13% to -35%** of time | [round-8-jansson.md](round-8-jansson.md) |
| 9 | On the user's decision: jansson with Yuneta's braces (byte-identical), jansson's blocks not zeroed (`gbmem_get_json_allocators()`), jansson's dump written into its buffer in place and its bytes classed by a table, a NUL looked for only after escapes, its counts of references without atomics (Yuneta has no threads); and, from the profile, a masked websocket frame unmasked into a gbuffer of its size (it was 4 KB) and the mask eight bytes at a time. The ESP32 copy of jansson assessed, not changed. Inter-yuno **-12.4%** of instructions, **+13%** of round trips/s; MQTT **+6.4%**; BFF logins +4.2% | [round-9-jansson-memory-websocket.md](round-9-jansson-memory-websocket.md) |
| 10 | The lookups of a treedb, on the user's decision: the columns of a topic read once while its `cols` is the same json (each column's flags as bits, its type), and the write and the view of a node asking them of that reading. Treedb writes **-16%** of instructions, reads **-19%**, the open of a treedb of a dynamic schema -17%; an update of a node **-21% to -36%** of time, a view **-15% to -17%** | [round-10-treedb-columns.md](round-10-treedb-columns.md) |
| 11 | The lookups that a local change can take away, on the user's decision: timeranger2 keeps the file id of the last time for every time of its unit (numeric masks only) and takes the key's totals from the cell the append just wrote; the treedb reads a node's metadata and a topic's pkey2s directly where the path readers would answer them. Appends **-7.6%** of instructions, treedb writes **-5.7%**; an update in memory -12% of time. What is left (the treedb's events, a topic's static fields, a column reading handed down) is written down for a decision | [round-11-append-treedb-lookups.md](round-11-append-treedb-lookups.md) |
| 12 | On the user's decision, from what round 11 left: the columns of a topic read once for a whole write of a node, kept in a slot of the operation (its `node_write_t`, or the create's) and given back before the append runs its callbacks; `treedb_save_node()` and `treedb_store_files()` keep their signature and their logs. Asks of the columns per update 5 -> 1; treedb writes **-2.0%** of instructions, within the noise in time | [round-12-treedb-columns-per-write.md](round-12-treedb-columns-per-write.md) |
| 13 | On the user's decision, the two items round 11 left: a timeranger2 topic's fixed fields (`system_flag`, `pkey`, `tkey`, `filename_mask`) read once per topic json, in slots that hold it; and the state of a treedb's events (its json, callback, user data, held and deferred events) kept in a C structure that follows the json's only writers and holds its tranger. Treedb writes **-7.8%** of instructions (5.2 million lookups fewer), appends -2.8% | [round-13-topic-fields-treedb-events.md](round-13-topic-fields-treedb-events.md) |
| 14 | The last round, on the user's decision: the paths rounds 10 to 13 did not touch profiled again, and the one local item found there applied: the name of an MQTT property looked up, with the same `strcasecmp()`, only among the names of its length (it went through all 27, three times per message). MQTT **-3.3%** of instructions. What else was found is written down below, in [What is left](#what-is-left) | [round-14-mqtt-property-names.md](round-14-mqtt-property-names.md) |

## Method

- **Base**: the release 7.27.7 (`1d827e4`), built with the `.config` of a node
  (GCC 13.3, RelWithDebInfo, fully static, OpenSSL, no
  `CONFIG_DEBUG_TRACK_MEMORY`), `linux-ext-libs` 1.24, glibc 2.39 (Ubuntu
  24.04), on a 4-core cloud VM. The figures of this machine are not those of
  the reports in `performance/reports/` (another machine, another glibc, track
  memory on): only the A/B on the same machine is compared.
- **Where the time goes**: `valgrind --tool=callgrind` on the benchmarks of
  `performance/c/`, chiefly `perf_tcp_test4` (TCP echo through the whole gobj
  stack) and `perf_tr_treedb` (treedb writes). The instruction count is
  deterministic, so a change is also measured by how many instructions it
  takes away. It does not count the time inside the kernel (the syscalls).
- **How much it gains**: the binaries of the base and of the change run
  alternated, several rounds, the order turned every round
  (`performance/c/README.md`); mean, standard deviation and t (the change over
  its standard error, |t| < 3 is noise). This VM is noisy, so the instruction
  count is the steadier figure and the wall-clock A/B confirms its direction.
- **That nothing changed**: the whole ctest suite, before and after.

## The proposals

What the rounds found and did not apply, because it changes behaviour,
format or API, touches a third-party library, or needs a structure a
consolidated module does not have yet, was left for a decision. Each one is
described, with its expected gain, in the round that found it. The user
decided to apply them all; what each one changed, what a caller can see and
what it gained are in [proposals-applied.md](proposals-applied.md).

| Proposal | Round | Decision |
|----------|-------|----------|
| `C_WEBSOCKET` as a server: a frame in one write, not two | [4](round-4-inter-yuno-http-mqtt.md) | applied (first batch) |
| The MQTT broker reads the subscriber's session without copying it | [4](round-4-inter-yuno-http-mqtt.md) | applied (first batch) |
| jansson: fast paths for ASCII (dump, UTF-8 check, lexer) | [2](round-2-timeranger2-treedb.md) | applied, in Yuneta's fork of jansson |
| A faster seeded hash in jansson | [1](round-1-gobj-core.md) | applied, in Yuneta's fork of jansson |
| The gbuffer of a TLS read sized to what was read; the same in mbedTLS | [3](round-3-network.md) | applied |
| Subscriptions as C structs | [1](round-1-gobj-core.md), [3](round-3-network.md) | applied as a C record next to each dict, without changing the API |
| A subscription built with its values | [4](round-4-inter-yuno-http-mqtt.md) | applied |
| A `C_TIMER` keeps its subscription to the yuno for its life | [1](round-1-gobj-core.md) | applied |
| treedb realtime lists marked `only_md` | [2](round-2-timeranger2-treedb.md) | applied |
| treedb open in O(keys), not O(history) | [2](round-2-timeranger2-treedb.md) | applied |
| Hooks rebuilt at load without the O(children^2) scan | [2](round-2-timeranger2-treedb.md) | applied |
| The masking key of a websocket client from a strong source (a security fix) | [3](round-3-network.md) | applied |
| The body buffer of an HTTP request sized to its body | [4](round-4-inter-yuno-http-mqtt.md) | applied |
| One `io_uring_submit()` per turn of the loop | [3](round-3-network.md) | **not applied**: not safe as proposed (a write submitted late can be reordered, a read submitted late can find its fd closed by `C_TCP`) |
| `TCP_NODELAY` | [3](round-3-network.md) | **nothing to do**: it is already set on every accepted and connected socket |

Round 5 left one more for a decision, applied with round 6:

| Proposal | Round | Decision |
|----------|-------|----------|
| `json2gbuf()`: the gbuffer of the size of the dump, not 4 KB zeroed whatever it holds (2.3% of an inter-yuno round trip) | [5](round-5-subscriptions-treedb.md) | applied in [round 6](round-6-jansson-parse-treedb-reads.md) |

## What is left

What the audit found at its close and did not do, with what it would give
and what it would take. The figures are instructions (`callgrind`) on the
benchmarks of `performance/c/`, after the last round that touched each path
(round 9 for the TCP echo, the inter-yuno messages and the BFF; round 14 for
MQTT; round 13 for the treedb). Each one is a decision.

### The allocator: the one large item

glibc's `malloc()` and `free()` (with `_int_malloc()`, `_int_free()` and
their helpers) are a large part of every path that is not the treedb's, and
the one cost a single change would reduce on all of them:

| Benchmark | glibc's allocator |
|-----------|-------------------|
| `perf_tcp_test4` (TCP echo through the gobj stack) | 12.8% |
| `perf_ievent` (inter-yuno round trips over websocket) | 16.9% |
| `perf_mqtt` (MQTT QoS 0 through the embedded broker) | 21.9% |
| `perf_auth_bff` (BFF logins) | 20.8% |

Every block of Yuneta and of jansson goes through `gbmem` to glibc. The
remedy is to restore gbmem's own memory manager, the pools of blocks by
size of yuneta V6 (from MAWK's `zmalloc`), whose switch is still there
(`USE_OWN_SYSTEM_MEMORY` in `gbmem_setup()`, `FALSE` in every yuno) and
whose code was not carried to v7. One thread, so no locks: a block taken and
given back in a few instructions. The price: what a pool keeps is not given
back to the system, so a process stays at the size of its peak. **FUTURE
(user, 2026-10-24)**, kept in `TODO.md`; the assessment is in
[round 9](round-9-jansson-memory-websocket.md).

### gobj-c: an attribute found by its name, at every access

`gobj_read_integer_attr()`, `gobj_write_integer_attr()`,
`gobj_read_bool_attr()` and their kind find the attribute by its name every
time: a read first compares the name with `__trace_level__`
(`strcasecmp()`), then `gobj_hsdata2()` asks `gobj_has_attr()`, which walks
the attribute table of the gclass comparing names with `strcmp()`
(`gclass_attr_desc()`) and goes down the bottom gobjs when the gclass has no
such attribute; then the value is looked up in the gobj's data. In
`perf_tcp_test4` the finding is **some 3%** of the instructions (what
`gobj_hsdata2()` costs under the integer reads 1.45%, the integer writes
0.97%, the boolean reads 0.44% and `gobj_match_gobj()` 0.21%). Most of it
is `C_TIMER`: each `clear_timeout()` writes its `msec`, and its
`mt_writing()` reads `msec` back, by name, some 720 000 times in the run;
the rest is `C_PROT_TCP4H` reading its settings at each frame header.
`C_TIMER` goes through the attribute on purpose (writing `msec` is the one
door to the timer, and `set_timeout()`/`clear_timeout()` are sugar over
it), so the remedy is not to go around the attribute but to find it faster.

The remedy: an index of a gclass's attributes by name, made when the gclass
is registered (its table does not change after), so that the walk is one
lookup. `gobj.c` is the consolidated core of the framework, and the gain is
small.

### gobj-c: the type of `EV_STATE_CHANGED`, at every change of state

`gobj_change_state()` asks `state_changed_has_audience()` whether anybody
would see the change, and that looks up the type of `EV_STATE_CHANGED` with
`gobj_event_type()`: a walk of the gclass's events, then of the global ones.
A protocol changes state twice per frame, so in `perf_tcp_test4` it is
**2.4%** of the instructions (2.16 million lookups). The remedy: the answer
kept in the gclass. Its events can still be added after it is registered
(`gclass_add_event_type()` is public), so the kept answer must be dropped
there. Same core, same small gain.

### The treedb: the lookups, spread thin

After rounds 10 to 13 the lookups of a key are still a third of a treedb
write, but no reader of them is large: the path readers 4.6%, a node's
metadata 4.2%, timeranger2's append 3.5%, a key's cache 2.7%, and a long
tail. Each one would give 1% to 3%, and most of them read jsons that callers
hold and change, where a reading kept needs every writer to say so. Not
proposed; the detail is in [round 13](round-13-topic-fields-treedb-events.md).

### Decided earlier

- **Slots for the first pairs inside a jansson object** (some 5% of an
  inter-yuno message, for some 320 bytes more in every object): **discarded
  for now** (user, 2026-10-24), see [round 9](round-9-jansson-memory-websocket.md)
  and `TODO.md`.
- **The ESP32 copy of jansson**, which is upstream 2.14 and has none of the
  changes of these rounds: **not touched for now** (user, 2026-10-24), see
  [round 9](round-9-jansson-memory-websocket.md) and `TODO.md`.
- **One `io_uring_submit()` per turn of the loop**: not applied, not safe as
  proposed (see the table of proposals above).

### What `callgrind` does not see

The instructions counted are the process's own. The time a message spends
in the kernel (the syscalls, the network stack, the disk) is not in them,
and on the network paths it is a large part of a round trip. The A/B of
each round measured time as well, and on this VM its noise (10% to 25% of a
case) hides most changes under some 10%.
