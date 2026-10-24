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
