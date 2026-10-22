# Round 4: inter-yuno messages, HTTP, MQTT

Scope: what a message costs above the transport: the inter-events between
yunos (`C_IEVENT_CLI` / `C_IEVENT_SRV` over `C_WEBSOCKET`, `msg_ievent`),
HTTP (`ghttp_parser`, `C_PROT_HTTP_SR`, through the BFF) and MQTT
(`C_PROT_MQTT2` and `C_MQTT_BROKER`). Measured on top of round 3: what is
compared here is round 3 against round 3 + round 4.

## How it was measured

`performance/c/` has no benchmark for inter-events nor for MQTT, so two were
written for this round, from the tests that already build those paths in one
process (they are not in the tree; they can be added to `performance/c/`):

- **Inter-yuno round trip** (from `tests/c/c_ievent_srv_peer_subs`): a
  `C_IEVENT_CLI` sends `EV_PERF_PING` to the service `echo` of the same yuno
  through `ws://127.0.0.1` (`C_WEBSOCKET` and `C_IEVENT_SRV` on the other
  side); `echo` publishes `EV_PERF_PONG` to the client's remote
  subscription, and the client sends the next ping. 100 000 round trips,
  a payload of ~140 bytes of JSON.
- **MQTT QoS 0** (from `tests/c/c_mqtt` test1): the embedded broker and a
  client in one process; the client publishes on a topic it is subscribed
  to and publishes again when the message comes back. 100 000 messages, a
  payload of ~120 bytes.

HTTP is measured with `perf_auth_bff` (BFF logins).

## What the profile showed

`callgrind`, share of the instructions of each benchmark on round 3:

| Inter-yuno round trip (617 000 instructions each) | |
|---|---|
| jansson: parse every frame received (`gbuf2json_from_peer`) and dump every frame sent (`json2gbuf`) | **50%** |
| malloc/free and libc around it | 32% of the self time |
| The payload timer of `C_WEBSOCKET`, for a frame whose header and payload arrive in two reads (the server writes them in two writes, see below) | **8%** |
| `json2gbuf()`: a `gbuffer_append()` per token jansson hands its dump callback | 3.7% |
| `gobj_find_event_type()`: the event of every inter-event received, found by walking the events of every gclass with `strcasecmp()` | 1.0% |
| `kw_set_dict_value()` of a key of one segment: `split2()` copies the path four times | 1.4% |

| MQTT publication delivered (326 000 instructions each) | |
|---|---|
| The broker copies the subscriber's **session** node for every message it delivers (`gobj_get_node()` -> `node_collapsed_view()`) | **16.5%** |
| ... of which: five `kw_has_word()` per column of the node, each walking the flags of the column | 4.7% |
| `new_mqtt_message()`: a record of the defaults of its ten fields built from a table (`create_json_record()`, parsing the type and the default of each field), then eight of them overwritten; four messages built per round trip | **12.4%** |
| Every json allocated and freed through libjwt's wrappers of the gbmem functions (`C_AUTHZ`'s `jwt_set_alloc()` points jansson at them) | 1.1% |
| `mosquitto_string_to_property_info()`: a property name found with a chain of `strcasecmp()`, one per property | 2.0% |

HTTP (BFF): jansson and malloc again; `llhttp` itself is 1%. The body of
every request goes into a gbuffer of 4 KB, zeroed (5%, overstated by
`callgrind`, which counts a `rep stosb` byte by byte).

The cost of an inter-event is jansson's: the code of the SDK around it is
12% of the instructions. What jansson costs is the proposal of round 2 (a
patched jansson, ready): on this benchmark two quick runs gave +0.2% and
+10% (not measured further), less than on timeranger2 records, because
inter-events are many short keys and strings rather than long ones.

## Changes applied (one commit each)

| Commit | Change | What stays the same |
|--------|--------|---------------------|
| `perf(mqtt): new_mqtt_message() builds the message with its values` | The ten fields set directly, in the order the table gave them, with `json_object_set_new_nocheck()` for the literal keys. The table, used only there, is now the comment of the function. | The same json, key for key and in the same order (a topic that is not a string stays `""`, such a `client_id` is left out, as when they were overwritten): checked against the old builder on 600 000 random messages, the same dump every time. |
| `perf(tr_treedb): a view of a node reads the flags of a column once` | `get_col_view_flags()`: the five flags `node_collapsed_view()` reads of each column, in one walk. | What `kw_has_word(gobj, flags, word, 0)` answers for each word, nothing logged: checked against it on 1.5 million random flags (dicts, lists with strings and others, strings, scalars, none). |
| `perf(gobj): gobj_find_event_type() remembers the event types it found` | The last 8 found, by the flag asked, are answered for a name equal but for case. | The walk would find the same one first: they are forgotten whenever a gclass or an event type is added (`gclass_create()`, `_add_event_type()`) or removed (`gclass_unregister()`). A name not found walks, and logs, as before. |
| `perf(kwid): kw_set_dict_value() with a path of one segment without split2()` | A path with no delimiter is the key. | What `split2()` answered for it (one segment, the path); the same `json_object_set()` and the same log if it fails. |
| `perf(gbuffer): json2gbuf() appends a token without a call when it fits` | `dump2gbuf()` does what `gbuffer_append()` does when the bytes fit. | `memmove`, tail and NUL as `gbuffer_append()`; anything else (no room, no buffer) goes to it, with its growth and its logs. |
| `perf(c_authz): jansson back to the gbmem functions after jwt_set_alloc()` | `json_set_alloc_funcs()` with the gbmem functions right after `jwt_set_alloc()`. | They are the functions libjwt's wrappers call, and the ones `entry_point.c` had set for jansson. libjwt keeps its wrappers for its own allocations. |

## Results

### Instructions (`callgrind`, deterministic), round 3 -> round 4

| Benchmark | Round 3 | Round 4 | Change |
|-----------|---------|---------|--------|
| MQTT QoS 0, 100 000 publications delivered | 32.56 G | 27.22 G | **-16.4%** |
| Inter-yuno round trips, 100 000 | 61.68 G | 59.98 G | -2.8% |
| `perf_tr_treedb` | 25.46 G (round 2) | 25.42 G | -0.2% |

### Time (A/B, alternated), round 3 -> round 4

8 rounds, the order turned every round, a `sync` and a 2 s pause before each
run; mean ± standard deviation, t = the change over its standard error.

| Benchmark | Round 3 | Round 4 | Change | t |
|-----------|---------|---------|--------|---|
| MQTT QoS 0, publications delivered/s | 30 968 ± 1 479 | 36 539 ± 2 197 | **+18.0%** | 5.9 |
| Inter-yuno round trips/s | 16 048 ± 549 | 16 378 ± 583 | +2.1% | 1.2 |
| TCP echo (`perf_tcp_test4`) | 79 640 ± 3 734 | 83 113 ± 3 949 | +4.4% | 1.8 |
| BFF logins (`perf_auth_bff`, K msg/s) | 9.11 ± 0.52 | 8.97 ± 0.30 | -1.5% | -0.6 |

`perf_tr_treedb` moved inside its noise in both directions (|t| < 2.1 in
every case; its instructions did not move). The inter-yuno round trip
gained what its instructions said, 2-3%: its cost is jansson's.

### Nothing changed

The whole ctest suite: the 305 tests that are not benchmarks, the same 9
failures as the base, all of the environment (round 1), and the treedb ones
among them compared message by message with the base: identical; the 11
benchmarks of the suite pass.

## Found, not done

- **`C_WEBSOCKET` as a server writes a frame in two writes**, its header
  and its payload (`ac_send_message()`: no mask, so the payload gbuffer is
  sent as it is). The client often reads the header alone, and arms and
  clears its payload timer for it: 8% of an inter-yuno round trip, plus a
  write and a read more per frame. One write (the header and the payload in
  one buffer, at least for small frames) would leave the same bytes on the
  wire, but `C_TCP`'s `txMsgs` would count one message where it counted two,
  and a traffic trace would show one dump: a proposal.
- **The broker copies a session per message it delivers.** `subs__send()`
  asks `gobj_get_node()` for the subscriber's session (a full collapsed view
  of the node) to read a few fields: 19% of an MQTT publication after this
  round. Reading them from the treedb's node without a copy is a change in
  the consolidated broker: a proposal.
- **A subscription is built from its description at every `C_TIMER`
  start** (`gobj_sdata_create()` parsing the default of ten fields): 2.4% of
  an inter-yuno round trip. A subscription built directly would be the same
  json; left, with the rest of the subscriptions (round 1: subscriptions as
  C structs).
- **The property names of MQTT** are found with a chain of
  `strcasecmp()`, one per property (2% of a publication). A table by first letter would answer
  the same; small, left.
- **The 4 KB body buffer of every HTTP request** is zeroed, and handed up as
  it is when the body is not JSON (its size is then visible to who receives
  it): left.
