# Round 7: jansson, the bytes of the lexer and of the dump

Scope: what round 6 left of jansson in an inter-yuno round trip, now that
`artgins/jansson` is Yuneta's official jansson (its `master` holds the
commits Yuneta carries; `linux-ext-libs` 1.29 pins it). Measured on top of
round 6: what is compared here is round 6 against round 6 + round 7.

## What the profile showed

`callgrind` of an inter-yuno round trip (`perf_ievent`) on round 6, the
cost per call of the small functions every byte goes through:

| Where | Calls per 100 000 round trips | Instructions each | Share of the round trip |
|---|---|---|---|
| `stream_get()`, the read of a byte by the lexer outside the runs (a brace, a colon, a comma, a quote, the first byte of a token), through the source's get function | 28 million | 55 | 4.2% |
| `strbuffer_append_byte()` -> `strbuffer_append_bytes()` -> `memcpy()`, the save of one byte | 28 million | 63 | 4.8% |
| `utf8_check_string()`, the UTF-8 check of every text frame received (`C_WEBSOCKET`), a byte at a time on its ASCII path | 200 000 | 3 500 | 1.9% |
| `dump_string()`, the dump of every string sent, a byte at a time on its ASCII path | 7.1 million | 280 | 5.3% |

The rest of the parse is the objects themselves (`malloc()`/`free()`, the
hash of the keys, the hash table), what the json of a message is.

## Changes applied (one commit each)

The three are commits of the `master` of `artgins/jansson`, pinned by
`linux-ext-libs` 1.30 (the first two) and 1.31 (the third).

| Commit | Change | What stays the same |
|--------|--------|---------------------|
| `load, strbuffer: a byte read and a byte saved without the generic calls` (jansson `d54161f`) | `stream_get_fast()`: a byte of plain ASCII that is not a newline nor a NUL, from the source of `json_loads()` or `json_loadb()`, with the stream holding no byte of its own, is read from the source directly, and the stream left as `stream_get()` leaves it (the byte in its buffer for an unget, the position and the column one more). `strbuffer_append_byte()` is inline: the byte and the NUL are written when there is room for both, which is exactly when `strbuffer_append_bytes()` does not grow the buffer. | Anything else goes the way it went (`stream_get()`, `strbuffer_append_bytes()` and its growth and failure). The same results, errors, lines, columns, positions and allocations: jansson's 215 tests, and the differential fuzz of round 6 (180 000 documents, `json_loadb()` and `json_loads()`, five sets of flags, the allocator capped at 5 bytes to 2 KB). |
| `utf, dump: eight bytes of plain ASCII at a time` (jansson `33b0a36`) | `utf8_check_string()` skips eight bytes with no high bit at once; `dump_string()` takes eight bytes that need no escape at once (`plain_ascii8()`: ASCII from 0x20, none `"` nor `\`, nor `/` with `JSON_ESCAPE_SLASH`). | A word that fails goes a byte at a time, as before. The tests on a word are exact: checked against a byte by byte reference on 48 million words (every value of a byte at each of the eight places, and random words, with and without `JSON_ESCAPE_SLASH`). The same bytes, results and errors: jansson's 215 tests, the differential fuzz (220 000 documents with their dumps under five sets of flags, and the UTF-8 check of raw bytes), and strings of 0 to 100 bytes with a special byte (an escape, a control byte, `/`, UTF-8 of two to four bytes, invalid bytes) at each place, checked and dumped under six sets of flags. |
| `utf, dump: a word test only where a word can be taken` (jansson `66f9eea`) | The second commit tested for a word on every byte of the two loops, and so cost more than a byte at a time in two cases, which the `callgrind` of this round showed: a string under eight bytes (most keys, each one checked by `json_object_set()`) paid a compare per byte for a word it could not read, 1.3% more instructions in the TCP echo; and after a word that failed, each of the next bytes read and tested a word again until the loop was past the byte that made it fail, 13% more instructions for a text with a non-ASCII character every few bytes. Now `utf8_check_string()` reads the words, then a byte at a time while the bytes are ASCII (at most up to the byte that made the word fail), then the character that is not; `dump_string()` reads the words, then a byte at a time through the eight bytes of the word that failed (or the last bytes, under eight), then the words again. | The same bytes, results and errors: jansson's 215 tests, the same differential fuzz, the strings with one special byte at each place, strings of 0 to 34 bytes with two special bytes at every pair of places (3.4 million dumps under four sets of flags), and the UTF-8 check of 30 million random strings against the check before the round. |

A parse, a dump and a UTF-8 check of a message of the shape of an
inter-yuno one, with jansson alone: 19% fewer instructions with the first
two commits. Against the library before the round, for strings of five
shapes (keys of 2 to 15 bytes, short, long, a text with a non-ASCII
character every seven bytes, one with a quote every ten), the three
commits check with 5% to 72% fewer instructions and dump with 3% to 79%
fewer:

| Strings | Check: before the round | after the second commit | after the third | Dump: before the round | after the second commit | after the third |
|---|---|---|---|---|---|---|
| Keys, 2 to 15 bytes | 5.11 M | 5.20 M | **4.88 M** | 5.88 M | 5.26 M | **5.31 M** |
| Short, 3 to 30 bytes | 5.67 M | 5.25 M | **4.93 M** | 7.58 M | 5.51 M | **5.52 M** |
| Long, 200 to 600 bytes | 33.14 M | 10.13 M | **9.33 M** | 91.08 M | 23.66 M | **19.37 M** |
| A non-ASCII character every 7 bytes | 21.89 M | 24.70 M | **19.72 M** | 41.99 M | 47.53 M | **40.93 M** |
| A quote every 10 bytes | 12.94 M | 6.50 M | **6.07 M** | 44.64 M | 44.46 M | **38.06 M** |

(2 000 strings of each shape, five times: `json_stringn()` of each, which
checks it, and `json_dumps()` of the array; instructions by `callgrind`.)

## Results

### Instructions (`callgrind`, deterministic), round 6 -> round 7

With the three commits (`linux-ext-libs` 1.31). The column "1.30" is the
same build with the first two commits only: the regression that the third
commit corrects.

| Benchmark | Round 6 | 1.30 | Round 7 | Change | Where |
|-----------|---------|------|---------|--------|-------|
| Inter-yuno round trips (`perf_ievent`) | 37.16 G | 33.60 G | 33.41 G | **-10.1%** | the parse of the frames received (`strbuffer_append_bytes()` 1 306 M -> 31 M, `string_get()` 343 M -> 2 M), `utf8_check_string()` 1 257 M -> 478 M, `dump_string()` 1 969 M -> 1 468 M |
| Appends (`test_topic_pkey_integer`) | 11.93 G | 9.55 G | 9.39 G | **-21.3%** | the dump of every record: `dump_string()` 2 420 M -> 521 M, `utf8_check_string()` 827 M -> 187 M |
| Reads with an iterator (`test_topic_pkey_integer_iterator2`) | 4.64 G | 4.36 G | 4.36 G | **-6.1%** | the parse of the records read |
| `perf_timeranger2` | 35.69 G | 35.27 G | 35.05 G | -1.8% | |
| `perf_tr_treedb_read` | 10.60 G | 10.49 G | 10.42 G | -1.7% | |
| `perf_tr_treedb` | 17.29 G | 17.07 G | 17.04 G | -1.4% | |
| MQTT QoS 0 (`perf_mqtt`) | 17.88 G | 17.91 G | 17.70 G | -1.0% | `utf8_check_string()` 573 M -> 397 M (1.30: 625 M), the keys and strings of the json of every message |
| TCP echo (`perf_tcp_test4`) | 10.60 G | 10.74 G | 10.58 G | -0.2% | `utf8_check_string()` 242 M -> 213 M (1.30: 381 M). This benchmark runs as many round trips as its wall clock lets it, so its total moves with its loop and its timers too |

### Time (A/B, alternated), round 6 -> round 7

The order turned every round, a `sync` and a 2 s pause before each run; mean
± standard deviation, t = the change over its standard error. A first A/B
(16 rounds for the network benchmarks, 8 for the disk ones) left the
inter-yuno round trip and the appends under the |t| = 3 of the method, and
two cases of the disk ones apparently slower; a second one, of 16 rounds,
was taken of those four benchmarks:

| Benchmark | Round 6 | Round 7 | Change | t |
|-----------|---------|---------|--------|---|
| Inter-yuno round trips/s (`perf_ievent`), second A/B | 12 774 ± 431 | 13 711 ± 704 | **+7.3%** | 4.5 |
| ... first A/B | 12 541 ± 688 | 13 230 ± 878 | +5.5% | 2.5 |
| Appends/s (`test_topic_pkey_integer`, second A/B), first key | 165 278 ± 12 091 | 184 960 ± 18 408 | **+11.9%** | 3.6 |
| ... second key | 143 480 ± 8 402 | 156 662 ± 13 325 | **+9.2%** | 3.3 |
| ... first A/B, first key / second key | 160 355 / 140 836 | 182 997 / 155 399 | +14.1% / +10.3% | 2.3 / 2.2 |
| Reads with an iterator (`test_topic_pkey_integer_iterator2`), ops/s, first A/B | 222 726 ± 24 694 | 239 396 ± 23 692 | +7.5% | 1.4 |

The rest moved inside their noise (|t| < 2): MQTT -0.9%, TCP echo -2.2%,
BFF logins +2.1%, and every case of `perf_timeranger2`, `perf_tr_treedb`
and `perf_tr_treedb_read` in both directions. The two that the first A/B
showed slower, `perf_tr_treedb` `create_link_half` (+21.8%, t = 1.8) and
`perf_timeranger2` `create_topic` (+16.2%, t = 1.6), came out -1.8% and
-7.8% in the second (their instructions fell, 1.4% and 1.8% of their
benchmarks).

### Nothing changed

The whole ctest suite, 319 tests: the same 9 failures as the base, all of
the environment (round 1), and the treedb and `secret_attrs` ones among them
compared message by message with round 6: identical. The suite ran once with
`linux-ext-libs` 1.30 and once with 1.31.

## Found, not done

- **jansson's allocations are zeroed for nothing.** Yuneta hands jansson
  `gbmem`'s allocator, which zeroes every block (`malloc()` + `memset()`,
  or `calloc()`), and jansson initialises all it allocates (upstream runs on
  plain `malloc()`): 40 million `memset()` calls per 100 000 inter-yuno
  round trips, about 3% of the trip. An allocator for jansson that does not
  zero (through `gbmem`, so that the tracking and the limits stay) would
  answer the same for correct code, but memory nobody wrote would no longer
  read as zero: a bug that reads past what jansson wrote would show where it
  is masked today. A proposal.
- **A text with a non-ASCII character every few bytes** is checked with
  fewer instructions than before the round (19.7 M against 21.9 M in the
  table above) but not in less time: in a loop of checks alone it took 4%
  to 10% more time than the byte at a time, because each such character
  ends the byte loop at an unpredictable place (a mispredicted branch).
  A word test that jumps straight to its first high byte (the count of
  trailing zeros of the word) took 27% to 34% less time on that shape, and
  a little more on the others; it needs a compiler builtin and the byte
  order of the machine, which jansson does not assume. Not done: the
  messages of the SDK are ASCII in their keys and nearly all their values.
- **What is left of an inter-yuno message is its json**: the objects of the
  parse (allocations, the hash of every key, the hash table) and the dump.
  Fewer objects per message would mean another format of the inter-event,
  not a faster jansson.
