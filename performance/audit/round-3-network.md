# Round 3: the network path

Scope: what a message crosses between the socket and the application:
`yev_loop`, `C_TCP`, `C_CHANNEL`, `C_IOGATE`, the protocols (`C_PROT_TCP4H`,
`C_WEBSOCKET`, `C_PROT_MQTT2`) and `ytls` (OpenSSL). Benchmarks:
`perf_tcp_test4`/`test5` (TCP echo through the whole gobj stack),
`perf_tcps_test4`/`test5` (the same over TLS) and `perf_auth_bff` (BFF
logins, HTTP). Measured on top of round 2: what is compared here is round 2
against round 2 + round 3.

## What the profile showed

`callgrind`, share of the instructions of each benchmark (the TCP echo
profiled on round 1, the TLS echo on round 2; round 2 did not touch this
path):

| Where | TCP echo | TLS echo |
|---|---|---|
| The **payload timer of every frame** in `C_PROT_TCP4H`: armed when a header arrives, cleared when its payload does, nearly always in the same read. Each arm starts a `C_TIMER`, which subscribes it to the yuno (a subscription is a json dict of 10 keys, built from their defaults); each clear stops it and unsubscribes it | **36%** | 17% |
| The **zeroed 32 KB gbuffer of every `SSL_read()`**: two per message received over TLS, the one that holds the data and the one of the last read of the flush, which answers WANT_READ and is thrown away | - | **41%** (see the note below) |
| `json_pack()` for kws of one or two keys: the filter of the destination channels of `C_IOGATE`, built again for every message sent, and the `{"gbuffer": ...}` of every buffer in `C_TCP` and `C_PROT_TCP4H` | 9.5% | 5.7% |
| `kw_set_subdict_value()`: `C_IOGATE` writes two keys of `__temp__` in every message it receives, and each built a default dict to throw away | 5.9% | 2.7% |
| `gobj_match_gobj()`: five `kw_get_str()` and a `kw_has_key()` (a path search and a hash each) for keys the filter of `C_IOGATE` does not have | 3.6% | 1.7% |
| `C_WEBSOCKET` unmasks every frame a client sends one byte at a time: a `gbuffer_get()` and a `gbuffer_append()` per byte | (no websocket benchmark) | |

Note on the TLS figure: `callgrind` counts the `rep stosb` with which glibc
zeroes a large block one byte at a time, so it overstates the share of the
zeroing; the time A/B below is the figure to take.

## Changes applied (one commit each)

| Commit | Change | What stays the same |
|--------|--------|---------------------|
| `perf(c_prot_tcp4h): no payload timer for a payload already received` | When the payload of the frame is already whole in the buffer, the timer is not armed: it would be cleared by the `EV_RX_DATA` the header action sends to itself next, inside the same call, with no turn of the loop to fire it. | It is skipped only when arming and clearing it changes nothing: the timer stopped, enabled, at 0 and not periodic (as the clear of the header left it), the gobj in `ST_WAIT_PAYLOAD`, and no trace on, of any level, on the timer or on the yuno (they would show the start, the stop and the two subscriptions). The yuno's list of subscriptions ends the same: an append and its removal. Otherwise armed as before. |
| `perf(c_prot_mqtt2): ...` and `perf(c_websocket): no payload timer ...` | The same in `C_PROT_MQTT2` (the steady-state header; the handshake keeps its timers) and in `C_WEBSOCKET`. | The same conditions. In `C_WEBSOCKET` with `pingT` the timer is running when the header arrives, so it is armed as before. |
| `perf(ytls): OpenSSL reads into a scratch, not a gbuffer per read` | `SSL_read()` reads into one scratch buffer of the process (a yuno has no threads), and the gbuffer is created only when something was read. | The gbuffer handed to the callback is the same: `rx_buffer_size` (zeroed), the same bytes, filled before the callback (a flush nested in it reads into the scratch again, harmlessly). With the global trace of gbuffers on (it lists every gbuffer created and deleted), or an `rx_buffer_size` of 0 or over half the largest block, it reads into a gbuffer as before. The scratch is freed with the last ytls. `ytls/test_free_inside_callback` (a session freed inside the callback) passes. |
| `perf(c_iogate): the filter of the destination channels built once` | Built in `mt_create()`, freed in `mt_destroy()`. | `gobj_match_gobj()` does not modify the filter (round 1 removed its deep copy). |
| `perf(root-linux): the kw of a gbuffer without json_pack()` | `json_object()` + `json_object_set_new()` in `C_TCP` (rx and the two TLS callbacks) and `C_PROT_TCP4H` (a message received, a message sent). | The same dict. |
| `perf(kwid): kw_set_subdict_value() without a default dict to throw away` | A subdict already there under a path of one segment is taken with `json_object_get()`. | What `kw_get_dict(..., KW_CREATE)` answers for it; anything else (a path of several segments, a missing key, another type) still goes to `kw_get_dict()`, with its creation and its logs. |
| `perf(gobj): gobj_match_gobj() reads only the naming keys a filter holds` | A dict filter is walked once to see which of the six keys that name the gobj it holds; a key that is not there is not read. | `kw_get_str()` answered 0 for a missing key, with no log, and `kw_has_key()` FALSE. A key that is there is read with the same reader, in the same order, so a value of another type logs as it did; a filter that is not a dict is read as before. |
| `perf(c_websocket): unmask a frame by runs` | A run fills what the unmasked buffer has free; a full buffer gets ONE byte, as before. | The same `gbuffer_realloc()` calls, with the same need, so the same doublings and, at the maximum of the buffer, the same logs, byte for byte. Checked against the byte loop on 24 000 random payloads, up to and past the maximum: the same bytes, the same sizes, the same logs. |

## Results

### Instructions (`callgrind`, deterministic), round 2 -> round 3

| Benchmark | Round 2 | Round 3 | Change |
|-----------|---------|---------|--------|
| TCP echo (`perf_tcp_test4`, 180 000 round trips) | 27.45 G | 15.95 G | **-41.9%** |
| TLS echo (`perf_tcps_test4`, 180 000 round trips) | 59.23 G | 35.81 G | **-39.5%** |

Against the base (7.27.7), rounds 1 to 3 together: TCP echo 41.14 G ->
15.95 G, **-61.2%**: the TCP echo takes 2.6 times fewer instructions.

`C_WEBSOCKET` has no benchmark in `performance/c/`. The unmask, timed alone
(a frame unmasked into a new buffer, `-O2`): 100 bytes 0.52 -> 0.15 µs, 1 KB
4.45 -> 0.53 µs, 16 KB 71.7 -> 7.0 µs, 256 KB 1315 -> 238 µs per frame.

### Time (A/B, alternated), round 2 -> round 3

8 rounds, the order turned every round, a `sync` and a 2 s pause before each
run; mean ± standard deviation, t = the change over its standard error.

| Benchmark | Round 2 | Round 3 | Change | t |
|-----------|---------|---------|--------|---|
| TCP echo through the gobj stack (`perf_tcp_test4`, round trips/s) | 53 926 ± 2 389 | 78 259 ± 4 196 | **+45.1%** | 14.3 |
| TCP echo + one append per message (`perf_tcp_test5`) | 39 010 ± 1 934 | 49 593 ± 4 360 | **+27.1%** | 6.3 |
| TLS echo, OpenSSL (`perf_tcps_test4`) | 38 684 ± 2 150 | 52 522 ± 2 878 | **+35.8%** | 10.9 |
| TLS echo + one append per message (`perf_tcps_test5`) | 30 063 ± 1 674 | 38 563 ± 2 269 | **+28.3%** | 8.5 |
| BFF logins (`perf_auth_bff`, K msg/s) | 8.70 ± 0.37 | 8.56 ± 0.24 | -1.7% | -0.9 |

The BFF goes through HTTP, not through the framed protocols or TLS of this
round, and moved inside its noise. Against the base, indicatively (the base
was measured in round 1's A/B, another session of the same machine): the
TCP echo went from 39 651 to 78 259 round trips/s, about twice as fast.

### Nothing changed

The whole ctest suite: the 305 tests that are not benchmarks, the same 9
failures as the base, all of the environment (round 1); the 11 benchmarks of
the suite pass. `ytls/test_free_inside_callback` (a TLS session freed inside
the callback that hands up what was read) and the `c_tcp`, `c_tcps`,
`c_websocket`, `c_prot_tcp4h`, `c_mqtt`, `c_auth_bff` and ievent tests are
among those that pass.

## Found, not done

- **The gbuffer of a TLS read could be the size of what was read.** It is
  still `rx_buffer_size` (32 KB, zeroed) for a message of any size: the
  zeroing is most of what is left of the TLS read (34% of the instructions
  of the TLS echo, overstated by `callgrind`), and a gbuffer kept by a
  consumer (a queue) holds 32 KB per message. Sized to what was read, the
  bytes are the same, but the free space of the buffer is not: a consumer
  that appends to the buffer it receives would grow it instead of finding
  32 KB free, and at the limit would reach its maximum one byte earlier
  (a grown gbuffer keeps one byte for its NUL). No consumer in the SDK
  appends to it; still a visible change of the buffer handed up, so a
  proposal.
- **mbedTLS** has the same loop (a gbuffer of `rx_buffer_size` per
  `mbedtls_ssl_read()`, the last one thrown away). It is not built here
  (`.config` of a node: OpenSSL), so it was not changed: the same change
  applies, to be built and tested where mbedTLS is.
- **Publishing reads every subscription by hash.** A publication reads 5 to
  8 keys of each subscription of the publisher (a json dict): 14 million
  `json_object_get()` in the TCP echo, ~16% of its instructions after this
  round. The fix is subscriptions as C structs: an API change (round 1).
- **`gobj_change_state()`** still costs ~7% of the TCP echo after this
  round: to decide whether the `EV_STATE_CHANGED` it would publish reaches
  anybody (round 1), it reads 5 keys of each subscription and looks the
  event type up linearly, three times. Caching the answer would need to be
  invalidated on every subscribe, unsubscribe and destroy: left.
- **`kw_incref()` / `kw_decref()`** look up every registered binary field
  (`gbuffer`) by hash on every call: ~6% of the TCP echo. Needs a way to
  know a kw carries no binary field without looking: left.
- **The masking key of a websocket client is predictable** (not a speed
  issue, found on the way): `dev_urandom()` in `C_WEBSOCKET` reseeds
  `rand()` with `srand(time(0))` for every frame it sends as a client, so
  every frame of the same second carries the same key, and the process's
  own `rand()` sequence is reset on each frame. RFC 6455 (§5.3) requires a
  masking key that cannot be predicted, from a strong source of entropy.
  Not changed here: it is a security fix, for its own commit.
- **One `io_uring_submit()` per operation.** `yev_loop` submits every read
  and write as it is armed: a system call each. Submitting once per turn of
  the loop would save most of them, but would change when an operation
  reaches the kernel (and where its submit error is seen): a proposal, not
  measured.
- **`TCP_NODELAY`** is not set on the sockets of `C_TCP`, so Nagle's
  algorithm can hold a small write while an earlier one is unacknowledged.
  Setting it changes what goes on the wire and when: a proposal, not
  measured.

## Next

Not looked at yet, and the subject of the next round (candidates from a
first reading, not verified): the inter-yuno path
(`msg_ievent` finds the event of every message received by name, scanning
the public events of every gclass; `C_WEBSOCKET` writes a message in two
writes), the HTTP parser and `C_PROT_HTTP_SR` (headers and body copied
several times), and the publication of `C_PROT_MQTT2` (the broker and an
ACL `json_pack()` per message).
