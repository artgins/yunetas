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

## Proposals waiting for a decision

What the rounds found and did not apply, because it changes behaviour,
format or API, touches a third-party library, or needs a structure a
consolidated module does not have yet. Each one is described, with its
expected gain, in the round that found it.

| Proposal | Gain | Why it waits | Round |
|----------|------|--------------|-------|
| jansson patch: fast paths for ASCII (dump, UTF-8 check, lexer), a ready patch | record reads **x2**, appends +25%, BFF +15% | a patch of a third-party library (`linux-ext-libs` VERSION bump) | [2](round-2-timeranger2-treedb.md) |
| The gbuffer of a TLS read sized to what was read | most of what is left of a TLS read (zeroing 32 KB per message), and 32 KB per message kept by a consumer | the free space of the buffer handed up changes | [3](round-3-network.md) |
| Subscriptions as C structs | ~16% of a TCP echo (hash lookups per publication) | `gobj_find_subscriptions()` returns json: API | [1](round-1-gobj-core.md), [3](round-3-network.md) |
| A faster seeded hash in jansson | part of the 10-14% of `hashlittle` | a patch of a third-party library | [1](round-1-gobj-core.md) |
| A `C_TIMER` keeps its subscription to the yuno for its life | every timer start/stop (the per-frame case is solved in round 3) | the list of subscriptions anybody can see changes | [1](round-1-gobj-core.md) |
| treedb realtime lists marked `only_md` | ~5 000 instructions per treedb save | `only_md` shows in the list and its callback gets NULL | [2](round-2-timeranger2-treedb.md) |
| treedb open in O(keys), not O(history) | the open of a treedb with a long history | a damaged older record is no longer detected at open | [2](round-2-timeranger2-treedb.md) |
| Hooks rebuilt at load without the O(children^2) scan | ~0.5 s of start-up for a parent with 3 000 children | a new structure in a consolidated module | [2](round-2-timeranger2-treedb.md) |
| `C_WEBSOCKET` as a server: a frame in one write, not two | 8% of an inter-yuno round trip (the peer's payload timer), a write and a read per frame | `C_TCP`'s `txMsgs` counts one message where it counted two | [4](round-4-inter-yuno-http-mqtt.md) |
| The MQTT broker reads the subscriber's session without copying it | 19% of an MQTT publication | a change in the consolidated broker | [4](round-4-inter-yuno-http-mqtt.md) |
| One `io_uring_submit()` per turn of the loop; `TCP_NODELAY` | syscalls per operation; latency of small writes | when an operation reaches the kernel; what goes on the wire | [3](round-3-network.md) |
| The same TLS change in mbedTLS | as OpenSSL's | not built here: to be built and tested where mbedTLS is | [3](round-3-network.md) |

And one finding that is not about speed: **the masking key of a websocket
client is predictable** (`srand(time(0))` per frame), against RFC 6455
§5.3: a security fix, for its own commit ([round 3](round-3-network.md)).
