# Round 8: standard C only, and jansson with fewer allocations

Scope: two decisions of the user after round 7. First, **no change of the
audit may depend on the compiler or the processor**, unless there is a major
reason. Second, **go on optimising jansson**. Measured on top of round 7:
what is compared here is round 7 against round 7 + round 8.

## Standard C only

Three changes of earlier rounds depended on something other than standard
C. The first two depended on the compiler. The third depended on the C
library, which the rule does not name, but it did not need the dependency
either.

| Where | What it depended on | Now |
|---|---|---|
| jansson's hash of the keys (round P, `linux-ext-libs` 1.25) | A 64x64 -> 128 bit product: `__uint128_t` where the compiler had it, four 32-bit products where it did not, with twice the instructions. | A hash built on 32-bit words. It mixes its state with the 64-bit product of two 32-bit words, which is exact in standard C (the construction of wyhash32). It is the same code and gives the same values everywhere, and it is 7% to 19% faster than the 128-bit path. |
| `json2gbuf()` (round 6) | `__attribute__((noinline))`, a GCC and clang extension, to keep the switch to a gbuffer out of the callback of every token. | A plain function. The callback is now called once per 4 KB of output, not once per token (see the dump below), so the attribute had nothing left to save. |
| `gbmem`'s small blocks (round 1) | `#ifdef __GLIBC__`: `malloc()` + `memset()` under glibc, `calloc()` anywhere else. | The same rule for every C library. The block is the same zeroed block in all of them; only glibc makes the rule faster, and the comment says so. |

## What the profile showed

`callgrind` of an inter-yuno round trip (`perf_ievent`) on round 7, per
100 000 round trips:

| Where | Calls | Share of the round trip |
|---|---|---|
| The allocator under jansson (`malloc()`, `free()`, gbmem, `jsonp_*`) | 38 million allocations, 381 per round trip | about a quarter |
| ... the pairs of the objects (`hashtable_set()`) | 11.5 million | |
| ... the string tokens of the lexer (keys and values) | 7.1 million | |
| ... the strings made by the SDK (`json_string()`: struct + copy) | 3.4 + 3.4 million | |
| ... the struct of each string parsed | 3.1 million | |
| ... the buckets of each table (`hashtable_init()`) | 3.4 million | |
| The callback of `json2gbuf()` (`dump2small()`), one call per piece of the dump | 35 million, 350 per round trip | 2.6%, its own instructions |
| The loop check of every dump and deep copy (a hashtable keyed by address) | 1.8 million containers | about 3% |
| The lexer: `lex_scan()`, `lex_save_run()`, `lex_get_save()` (74 instructions per call of the last one, the generic read and save of a byte) | 21 million calls of `lex_get_save()` | about 16% |

## Changes applied (one commit each)

Nine commits of the `master` of `artgins/jansson`, pinned by `linux-ext-libs`
1.32, and one of the SDK.

| Commit | Change | What stays the same, and what does not |
|--------|--------|----------------------------------------|
| `hashtable: a hash of 32-bit words, standard C on any compiler` (jansson `6a58f76`) | See above. | Nothing jansson answers depends on the hash. It spreads seven sets of keys as evenly as the previous hash (\|z\| < 3 in a chi-square test). The avalanche of each input bit on each output bit is within noise, as with the previous hash. Objects of 200 000 keys give the same answers. |
| `hashtable: the first buckets of a table kept in the table` (`5fb7a0b`) | A table keeps its first 8 buckets in itself and allocates them only when it grows past them: for every object, every dump and every strict unpack. | The same results. One allocation fewer per table. |
| `value: a short string with its value in its own block` (`56be000`) | A string whose value fits in a block of 1024 bytes together with its `json_string_t` is one allocation: the value right after the struct. | The same results, checked for strings of 0 to 2000 bytes: set, copied, compared and dumped. A longer value, a value handed over by its owner, and a value set later keep their own allocation, as before. |
| `load: a string token without an allocation of its own` (`0f7e1f0`) | A token without escapes is its saved text between the quotes. A token with escapes is decoded into one buffer of the lexer. A key waits in a stack of keys of the lexer until its value is parsed. | The same results, errors and positions. This was checked with 8 000 loads of nested objects with keys of up to 3 000 bytes, escapes and repeated keys. |
| `value, dump: the loop check of a walk without a hashtable for 32 levels` (`b66106b`) | The containers above the current one are a path: the first 32 in an array, compared one by one, and deeper ones in the hashtable of before. | The same answers, checked for chains of 1 to 2 100 containers with and without cycles, and with shared containers. A recursive update has no depth limit, so a deep walk still costs one lookup per container. |
| `dump: the output gathered in a buffer before the callback` (`a43a0e2`) | `json_dump_callback()`, which every dump goes through, gathers the pieces in a buffer of 4096 bytes. It hands them over when the next piece would not fit, and at the end. A piece is never cut. | The same output from all five dump functions. The callback gets 167 times fewer calls. **One answer changes, in an error case**: see below. |
| `value: the first table of an array in the array's own block` (`25f6c9b`) | An array keeps its first table of 8 entries in its own allocation. | The same results, checked with random appends, inserts, removes and extends across the growth. |
| `load: json_loads() reads its string as a buffer of its length` (`a2bad87`) | `json_loads()` measures its string and reads it through the source of `json_loadb()`. | The same results: the end of the buffer is where the NUL ended the string. |
| `load: a string token and an integer read without the generic byte calls` (`a81a51c`) | A string takes its plain bytes as a run, eight at a time, and takes its closing quote where it is. An integer ends without reading the byte after it when that byte cannot continue the number. | The same results, errors and positions, checked with 333 420 loads of edge cases: integers followed by each of the 256 bytes, special bytes at each place of a string, and tokens across the growths of the saved text. |
| `gobj-c: no dependency on the compiler or the C library in the audit's changes` (SDK) | See above. | The same bytes. |

Every jansson commit was checked with jansson's 215 tests, both plain and
under AddressSanitizer and UndefinedBehaviorSanitizer. Each was also run
through the differential fuzz of the lexer and the dump: 220 000 documents,
five loads each, with their dumps. Under a capped allocator, the fuzz ran
under AddressSanitizer with no error and no leak.

### What a caller can see

- **What a custom allocator sees** (`json_set_alloc_funcs()`): fewer
  allocations, of other sizes. Allocations that could fail before are not
  made now: the buckets of a table, the table of parents of a dump, and the
  allocation of each string token. Under an allocator that fails, the point of a
  failure can move. Without a failure, the results are the same.
- **A dump whose callback fails once now always fails.** `do_dump()` does
  not check what `dump_string()` answers for a key. So a callback that
  failed on the piece of a key and then succeeded again gave a dump without
  that key, and `json_dump_callback()` answered 0. The test of this round
  found it 6 152 times in jansson 1.31. Now the buffer stops at the first
  failure and the dump answers -1, as the documentation of the callback
  says ("On error, the function should return -1 to stop the encoding
  process"). For `json_dumps()`: a strbuffer that could not grow on a key
  gave a text without that key, and now gives NULL.
- **The chunks a dump callback gets** are bigger: up to 4096 bytes of whole
  pieces, so each one is still valid UTF-8. jansson promises nothing about
  their size. When a dump fails, `json_dumpf()`, `json_dumpfd()` and
  `json_dumpb()` still leave in their output all that the dump wrote before
  the failure.

## Results

### Instructions (`callgrind`, deterministic), round 7 -> round 8

| Benchmark | Round 7 | Round 8 | Change |
|-----------|---------|---------|--------|
| Inter-yuno round trips (`perf_ievent`) | 33.41 G | 26.24 G | **-21.5%** |
| Reads with an iterator (`test_topic_pkey_integer_iterator2`) | 4.36 G | 3.23 G | **-25.8%** |
| `perf_tr_treedb_read` | 10.42 G | 8.82 G | **-15.4%** |
| Appends (`test_topic_pkey_integer`) | 9.39 G | 8.23 G | **-12.3%** |
| `perf_timeranger2` | 35.05 G | 30.78 G | **-12.2%** |
| `perf_tr_treedb` | 17.04 G | 15.42 G | **-9.5%** |
| TCP echo (`perf_tcp_test4`) | 10.58 G | 9.70 G | -8.3% |
| MQTT QoS 0 (`perf_mqtt`) | 17.71 G | 16.31 G | -7.9% |

Where the inter-yuno round trip lost its 7.2 G of instructions:

| Where | Round 7 | Round 8 | Change |
|---|---|---|---|
| The allocator (`malloc()`, `free()`, their internals, `memset()`, gbmem, `jsonp_*`) | 10.21 G | 6.19 G | -4.02 G |
| The lexer (`lex_scan()`, `lex_save_run()`, `lex_get_save()`) | 5.35 G | 3.86 G | -1.49 G |
| The dump (`dump_string()`, the callback of `json2gbuf()`) | 2.52 G | 1.62 G | -0.90 G |
| The hash and the hashtable (`hash_str()`, `hashtable_set()`, `hashtable_get()`) | 3.17 G | 2.54 G | -0.63 G |

In the reads with an iterator, the lexer went from 1.78 G to 0.74 G (-58%).

### Time (A/B, alternated), round 7 -> round 8

The order turned every round, with a `sync` and a 2 s pause before each
run. The figures are mean ± standard deviation; t is the change over its
standard error. The network benchmarks ran 16 rounds, the disk ones 8. The
VM was quieter in this round than in the last two.

| Benchmark | Round 7 | Round 8 | Change | t |
|-----------|---------|---------|--------|---|
| Inter-yuno round trips/s (`perf_ievent`) | 14 093 ± 648 | 16 549 ± 567 | **+17.4%** | 11.4 |
| BFF logins (`perf_auth_bff`, K msg/s) | 5.47 ± 0.27 | 6.12 ± 0.30 | **+11.9%** | 6.5 |
| Reads with an iterator (`test_topic_pkey_integer_iterator2`), ops/s | 275 523 ± 9 577 | 322 451 ± 18 893 | **+17.0%** | 6.3 |
| `perf_tr_treedb_read`, `dump_views` (µs of CPU per user) | 2.76 ± 0.17 | 1.79 ± 0.29 | **-35.2%** | -8.2 |
| ... `view_departments` | 118.1 ± 9.3 | 89.4 ± 10.6 | **-24.3%** | -5.8 |
| ... `view_users` | 7.35 ± 0.35 | 6.36 ± 0.43 | **-13.5%** | -5.1 |
| ... `get_view` | 4.90 ± 0.25 | 4.48 ± 0.23 | -8.4% | -3.4 |
| ... `reopen` | 60.8 ± 1.7 | 56.6 ± 2.8 | -6.9% | -3.6 |
| `perf_tr_treedb`, `reopen` (µs of CPU per node) | 33.0 ± 1.6 | 30.3 ± 0.8 | -8.0% | -4.3 |
| ... `delete_parent` | 3 829 ± 173 | 3 546 ± 155 | -7.4% | -3.4 |
| `perf_timeranger2`, `read_open_replica` (µs per read) | 17.01 ± 0.31 | 14.63 ± 0.24 | **-14.0%** | -17.1 |
| ... `read_page100_replica` | 304.3 ± 14.1 | 269.1 ± 4.3 | -11.6% | -6.7 |
| ... `read_page1_replica` | 8.63 ± 0.26 | 8.02 ± 0.11 | -7.1% | -6.1 |
| ... `tm_build_appends` | 4.33 ± 0.18 | 4.00 ± 0.13 | -7.7% | -4.2 |
| MQTT QoS 0 round trips/s (`perf_mqtt`) | 22 825 ± 1 351 | 24 025 ± 1 233 | +5.3% | 2.6 |
| Appends/s (`test_topic_pkey_integer`), the two keys | 214 494 / 179 827 | 222 586 / 186 982 | +3.8% / +4.0% | 1.5 / 2.4 |
| TCP echo round trips/s (`perf_tcp_test4`) | 41 175 ± 1 516 | 42 152 ± 1 526 | +2.4% | 1.8 |

The other cases of `perf_timeranger2`, `perf_tr_treedb` and
`perf_tr_treedb_read` moved within their noise (|t| < 3), most of them
faster. No case is slower beyond its noise.

### Nothing changed

The whole ctest suite, 319 tests, gave the same 9 failures as the base. All
of them come from the environment (round 1). The treedb and `secret_attrs`
failures were compared message by message with round 7, and they are
identical.

## Found, not done

- **jansson's allocations are zeroed for nothing** (round 7, a proposal):
  gbmem zeroes every block it gives, and jansson initialises all it
  allocates. The `memset()` calls are still about 3% of the inter-yuno
  round trip.
- **The dump of a string** (`dump_string()`, about 6% of the inter-yuno
  round trip) still makes three calls to the buffer per string (the two
  quotes and the run). The lexer was the bigger part, and it went first.
- **The pairs of an object** are still one allocation each: 115 per
  inter-yuno round trip.
