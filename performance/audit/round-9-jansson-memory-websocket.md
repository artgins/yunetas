# Round 9: jansson's blocks not zeroed, its dump written in place, a websocket frame unmasked in its own size

After round 8 the user went ahead with what it had left open: Yuneta's
braces in jansson's sources, an allocator that does not zero jansson's
blocks, the next round of jansson, and whether the ESP32 copy of jansson
should become this library. The round looked again at the profile of
round 8, and two more costs came out of it, in `C_WEBSOCKET`, and one that
`callgrind` does not show: jansson's atomic counts of references.

Base: round 8 (`linux-ext-libs` 1.32, jansson `a81a51c`), on the machine
and with the method of the [README](README.md).

## jansson with Yuneta's braces

jansson's `.clang-format` takes `InsertBraces: true`, and the whole repo
was formatted with it (`ea1f8ae`): braces added and nothing else. The
library built before and after (Release, `-g0`) is byte-identical, every
object file and `libjansson.a`; jansson's 215 tests and
`test/differential/run.sh a81a51c` pass. `src/dtoa.c`, David Gay's, stays
as he wrote it. `src/lookup3.h`, which nothing included since jansson's
hash stopped using lookup3 (round P), is gone (`68d918c`).

## What the profile showed

The profiles of round 8 (`callgrind`, instructions), read again:

- **The zeroing of jansson's blocks.** `gbmem`'s malloc zeroes every block
  (a `memset()` up to 1 KB, `calloc()` above), and jansson allocates
  through it: 89% of the blocks of `perf_ievent` are jansson's (22.5 M of
  25.3 M). The `memset()` of a small block was 3.2% of the instructions of
  `perf_ievent`, 2.2% of `perf_mqtt`, 3.0% of the reads with an iterator,
  5.0% of the BFF logins. jansson never reads a byte it has not written:
  it asks for `malloc()`'s contract, and upstream's default allocator is
  `malloc()`.
- **The bytes of a string in jansson's dump.** `dump_string()` was 5.0%
  of `perf_ievent` by itself. After a word of eight bytes that was not all
  plain (a quote in the payload, the end of a key), each byte was tested
  through its codepoint against the escapes, `/` and `JSON_ENSURE_ASCII`,
  14 instructions or so; and each piece (a quote, a run, an escape) was a
  call to `dump_to_dump_buffer()`, 8.8 M of them, 1.5% with the copy.
- **A NUL looked for in every string loaded**: `memchr()` from
  `parse_value()`, 0.3% of `perf_ievent`.
- **A 4 KB block for each masked websocket frame.** `unmask_data()` made
  the gbuffer of the unmasked payload of 4 KB, whatever the payload: a
  `calloc()` and its `memset()` of 4 KB, and, because a request over 1 KB
  is "large" for glibc, a consolidation of its free lists each time. 975 M
  instructions for 100 000 frames, **3.7%** of `perf_ievent`, about 9 750
  per frame received. Every frame a browser sends is masked.
- **The mask, a byte at a time**: `mask()` and the unmask, a xor and a
  `% 4` per byte, 1.4% (`ac_send_message()`) and 1.2%
  (`frame_completed()`) of `perf_ievent`.
- **Atomic counts of references.** jansson is built with the compiler's
  atomic builtins, so `json_incref()` and `json_decref()`, inline in every
  caller, are a locked instruction (`lock add`) each. `callgrind` counts it
  as one instruction, but it costs some twenty cycles, and Yuneta has no
  threads: one yuno per core. The code of `perf_ievent` has 2 491 locked
  instructions; 484 without jansson's counts.

## Changes applied (one commit each)

In Yuneta:

1. **gobj-c: jansson's blocks not zeroed.** A new function,
   `gbmem_get_json_allocators()`, gives the pair to hand
   `json_set_alloc_funcs()`: the tracking and the limits of
   `gbmem_malloc()` (the largest block, `CONFIG_DEBUG_TRACK_MEMORY`), the
   same free, and no zeroing. `yuneta_entry_point()` (every yuno), `C_AUTHZ`
   (which sets jansson's allocator again after libjwt), the tools of
   `utils/c` and the benchmarks of `performance/c` take it. `gbmem_malloc()`
   keeps zeroing: Yuneta's code relies on it. In a build with
   `CONFIG_DEBUG_TRACK_MEMORY`, a block of jansson is filled with `0xA5`, so
   that a read of a byte jansson did not write reads garbage in the tests
   too, as it could on a node. (The body of the malloc is shared by the
   two, `inline`, so neither pays a call or a test for the other.)
2. **root-linux: a masked websocket frame unmasked into a gbuffer of its
   size**, not of 4 KB: the payload is already in a gbuffer, so its size is
   never above the largest block. And **the mask eight bytes at a time**:
   the key, from the position of the first byte, twice in a 64-bit word,
   and each word of the data read and written with `memcpy()`. The bytes of
   both are in the order of memory, so the xor is the same on any byte
   order: standard C. Checked against the byte loop on every length up to
   200, every offset of the key and every alignment (2 880 000 cases, and
   again under AddressSanitizer and UndefinedBehaviorSanitizer).

In jansson (`artgins/jansson`, `master`):

3. **The dump writes its pieces into its buffer in place** (`01f0e80`).
   Every dump goes through `json_dump_callback()` and its buffer, but the
   internal functions still received it through the shape of a callback,
   so every piece cost a call. They take the buffer now, and a piece is
   copied into it inline when it fits; one that does not goes to a function
   that hands the buffer over first, as before. **The bytes of a string
   are classed by a table** of 256 (always escaped, escaped with
   `JSON_ESCAPE_SLASH`, a byte of a UTF-8 sequence): a plain byte is a load
   and a test.
4. **A NUL is looked for only in a string that had escapes** (`dd191c6`):
   the lexer refuses a control byte in a string, so only a `\u0000` escape
   can put a NUL there, and a string with escapes is the one decoded.
5. **A build option for counts without atomics** (`20fd432`):
   `JANSSON_THREAD_SAFE_REFCOUNT`, ON by default as before. OFF, jansson
   counts references with `++` and `--`, the case it already had for a
   compiler without the builtins, and does not define
   `JANSSON_THREAD_SAFE_REFCOUNT`. **`configure-libs.sh` builds jansson
   with it OFF** (`linux-ext-libs` 1.33): Yuneta has no threads, there is
   no `pthread_create()` in its C code, and a json value of a yuno is
   never shared by two. It also takes a dependency on the compiler's
   builtins out of every caller of jansson.h. Decided on the A/B below,
   where it is the larger part of the gain of MQTT and of the BFF.

### How they were checked

- **jansson reads no byte it has not written**: its 215 tests, the seven
  programs of `test/differential/` (10 million cases for the strings
  alone) and the fuzz (three seeds of 3 000 documents, and one under an
  allocator that refuses blocks over 300 bytes) under `valgrind
  --tool=memcheck`, whose `malloc()` does not zero: no error.
- **Nor does any code of Yuneta that reads what jansson allocated**: the
  whole suite of the SDK ran with a temporary variant that filled every
  block of jansson with `0xA5` in every build (not only with
  `CONFIG_DEBUG_TRACK_MEMORY`). It failed what it fails on this machine
  without the change (9 tests, each one with the same messages as in
  round 8), and 3 more that ran out of inotify instances: this new
  container allows 128 per user. With the limit raised the 3 pass, with the
  variant, and one of them failed in the same way without any change of
  this round.
- **jansson answers as before**: `test/differential/run.sh a81a51c` (the
  commit of round 8) for each commit of jansson, and its 215 tests, also
  built with `JANSSON_THREAD_SAFE_REFCOUNT=OFF`.
- **The suite of the SDK** on the result (`linux-ext-libs` 1.33, every
  yuno rebuilt), 319 tests: the 9 failures of this machine, each one with
  the same messages as in round 8, and nothing else.

### What a caller can see

- A new public function of gobj-c, `gbmem_get_json_allocators()`
  (documented in the API reference, with an example). A program that sets
  jansson's allocator itself, and does not start through
  `yuneta_entry_point()`, should take it in place of
  `gbmem_get_allocators()`; with the old one it works as before, zeroing.
- The blocks jansson allocates are not zeroed (filled with `0xA5` with
  `CONFIG_DEBUG_TRACK_MEMORY`). Nothing reads them but jansson, which
  writes before it reads.
- The gbuffer of a masked websocket frame has the size of its payload,
  where it had 4 KB: the same bytes, and a gbuffer grows when appended to.
- jansson: none, for its callers. The dump hands its callback the same
  pieces, the loads refuse the same strings with the same errors.
- Built by `configure-libs.sh` 1.33, jansson does not define
  `JANSSON_THREAD_SAFE_REFCOUNT`: a json value must not be shared by
  threads. Nothing in Yuneta does, nor reads the macro.

## Results

### Instructions (`callgrind`, deterministic), round 8 -> round 9

The steps: 9a is the allocator, 9b adds jansson's dump and NUL check, 9
adds `C_WEBSOCKET` (and the allocator inline). The counts without atomics
are not in these figures: `callgrind` counts a locked instruction as one.

| Benchmark | Round 8 | 9a | 9b | Round 9 |
|-----------|--------:|---:|---:|--------:|
| `perf_ievent` (inter-yuno) | 26.24 G | -2.3% | -4.2% | **-12.4%** |
| `perf_mqtt` | 16.31 G | -2.5% | -2.6% | -3.0% |
| `perf_tcp_test4` (TCP echo) | 9.70 G | -1.6% | -1.8% | -1.9% |
| `perf_tr_treedb` (writes) | 15.42 G | -1.0% | -1.0% | -1.1% |
| `perf_tr_treedb_read` | 8.82 G | -1.6% | -2.6% | -2.5% |
| `perf_timeranger2` | 30.78 G | -2.0% | -2.9% | -3.0% |
| `test_topic_pkey_integer` | 8.23 G | +0.4% | -0.3% | -1.0% |
| `test_topic_pkey_integer_iterator2` | 3.23 G | +0.7% | +0.2% | -0.4% |

The two tests of timeranger2 set jansson's allocator themselves, with the
pair that zeroes, so the first step only added to them the call of the
shared body (+0.4%, +0.7%); with the body `inline` that is gone. In
`perf_ievent` the last step, almost all of it the websocket change, takes
8.6%: the frames of its client are all masked.

### Time (A/B, alternated), round 8 -> round 9

Three binaries in turn, the order turned every round, with a `sync` and a
2 s pause before each run: round 8, round 9 with jansson's counts atomic
(as 1.32 built it), and round 9 as `linux-ext-libs` 1.33 builds it, the
counts without atomics. Mean ± standard deviation; t is the change over its
standard error (|t| < 3 is noise). The network benchmarks ran 16 rounds,
the disk ones 8.

| Benchmark | Round 8 | Round 9, atomic counts | Round 9 (1.33) | Change, t |
|-----------|---------|------------------------|----------------|-----------|
| Inter-yuno round trips/s (`perf_ievent`) | 21 747 ± 766 | 23 983 ± 1 741 | 24 580 ± 1 276 | **+13.0%**, 7.6 |
| MQTT QoS 0 round trips/s (`perf_mqtt`) | 30 236 ± 1 399 | 30 798 ± 1 691 | 32 174 ± 870 | **+6.4%**, 4.7 |
| BFF logins (`perf_auth_bff`, K msg/s) | 8.68 ± 0.27 | 8.78 ± 0.44 | 9.05 ± 0.27 | **+4.2%**, 3.9 |
| TCP echo round trips/s (`perf_tcp_test4`) | 54 577 ± 2 344 | 53 270 ± 1 409 | 55 738 ± 1 990 | +2.1%, 1.5 |
| `perf_tr_treedb_read`, `get_view` (µs of CPU) | 4.27 ± 0.32 | 4.03 ± 0.28 | 3.85 ± 0.12 | **-9.9%**, -3.5 |
| `perf_tr_treedb`, `update_saved` (µs of CPU) | 7.71 ± 0.38 | 7.13 ± 0.16 | 7.44 ± 0.27 | -3.6%, -1.7 |

The change is round 9 (1.33) against round 8. Atomic or not, the counts
are the same instructions to `callgrind`; in time they are the larger part
of the gain of MQTT (+1.9% with atomic counts, t 1.0) and of the BFF
(+1.1%, t 0.7), and a fifth of that of the inter-yuno path (+10.3%, t
4.7, with atomic counts). The other cases of `perf_timeranger2`,
`perf_tr_treedb` and `perf_tr_treedb_read`, and the two tests of
timeranger2, moved within their noise, which on this VM was large for
`perf_timeranger2` (its `build_appends` varied by ±45% between runs of one
binary). No case is slower beyond its noise.

## The ESP32 copy of jansson

The ESP32 port has a copy of its own
(`kernel/c/root-esp32/components/esp_jansson/jansson`): jansson 2.14
(`684e18c` of jansson's history) with the lexer fix applied by hand, and
one function more, `json_integer_value_pointer()`, which nothing in the
tree calls. It has none of the changes of rounds P to 9.

What was checked: jansson's `master` compiles with the two configuration
headers of that copy (`jansson_config.h`, `jansson_private_config.h`), on
this machine (`gcc -Wall -Wextra`: no warning but upstream's own). The API
is the same, 2.14's being a part of it; `json_integer_value_pointer()`
would go. `dtoa.c` would be compiled and not used (`DTOA_ENABLED` is not
set there), as now.

Three things stand in the way, and they are decisions:

1. **The stack.** `json_dump_callback()` keeps its buffer of 4096 bytes on
   the stack (round 8). The task of the yuno on the ESP32 has 8 KB
   (`c_esp_yuno.c`), that of the transport 4 KB: a dump there would take
   half of it, or all. The size of that buffer would have to be a setting
   of the build (a few hundred bytes on the ESP32).
2. **The words of eight bytes** (`uint64_t`, rounds 7 and 8) are two
   operations on the 32-bit cores of the ESP32: correct, but what they gain
   there is not measured.
3. **Nothing here builds or runs ESP-IDF**: the change would reach a device
   untested.

How it would be done, if decided: the component takes `src/` of jansson at
`TAG_JANSSON` (copied by a script, as `repos2clone.sh` clones it for
Linux), with its two configuration headers kept, plus the size of the dump
buffer; then a build and a run on a device.

**Decided (user, 2026-10-24): not touched for now.** Kept in `TODO.md`.

## Found, not done

- **Allocation is now the largest cost of a message.** `malloc()` and
  `free()` of glibc, with their internals, are some 18% of the instructions
  of `perf_ievent`, and their cost in time is larger than their
  instructions (cache misses). jansson takes 225 blocks per round trip
  there: 100 pairs of objects, 65 strings, 31 objects, 17 integers, 6
  arrays, and 6 buffers of the parser. Two ways, both decisions:
    - **Slots for the first pairs in the object** (jansson), as round 8
      did for the buckets of its table and the table of an array: an object
      would carry, say, four slots for pairs with a key of up to 23 bytes.
      With 3.2 keys per object in a message, most pairs would take no
      block: some 5% of the instructions of `perf_ievent`, estimated. The
      price is memory: some 320 bytes more in every object, used or not,
      and a treedb holds hundreds of thousands of them. **Discarded for
      now (user, 2026-10-24).**
    - **Restore gbmem's own memory manager**, the pools of blocks by size
      of yuneta V6 (from MAWK's `zmalloc`), whose switch is still there
      (`USE_OWN_SYSTEM_MEMORY`, `FALSE` in every yuno) and whose code was
      not carried to v7. One thread, so no locks: a block taken and given
      back in a few instructions, for every block of Yuneta and jansson.
      What a pool keeps is not given back to the system: a process stays
      at the size of its peak. **FUTURE (user, 2026-10-24)**, kept in
      `TODO.md`.
- **The lookups of a treedb.** In `perf_tr_treedb`, `hashtable_get()`,
  `hash_str()`, `json_object_get()` and `strlen()` are 38% of the
  instructions: 48 million lookups of a key, 11.8 million of them from
  `kw_find_path()`. jansson's lookup is already short (some 25
  instructions for a key found first in its bucket); what costs is how
  many of them Yuneta makes. A round on `tr_treedb` and `kwid`.
- **The dump of an integer** goes through `snprintf()`: 0.3% of
  `perf_ievent`. Not worth its code.
