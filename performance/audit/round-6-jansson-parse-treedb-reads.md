# Round 6: jansson's parse, json2gbuf(), and a benchmark of the treedb reads

Scope: what round 5 left ("Found, not done"), on the user's decision to go
ahead with all of it: the proposal of `json2gbuf()` (a gbuffer of the size
of the dump), the parse of jansson a byte at a time, and the benchmark of
the treedb reads in the tree. Measured on top of round 5: what is compared
here is round 5 against round 5 + round 6.

## What the profile showed

`callgrind` of an inter-yuno round trip (`perf_ievent`) on round 5:

| Where | Share |
|---|---|
| jansson's parse of every frame received: `lex_scan()`, `stream_get()`, `strbuffer_append_bytes()`, `string_get()` and around them | **26%** |
| ... of which: the runs of plain ASCII of a string token were saved at once since round 2 (`linux-ext-libs` 1.25), but only from the source of `json_loadb()`. `gbuf2json()` parses with `json_loads()`, so its bytes still went one at a time through `stream_get()` and a call to `string_get()` | |
| `json2gbuf()`: a gbuffer of 4 KB, zeroed, for every json it dumps (two per round trip) | 2.3% |

## Changes applied (one commit each)

| Commit | Change | What stays the same |
|--------|--------|---------------------|
| `perf(gbuffer): json2gbuf() makes a gbuffer of the size of a small dump` | The proposal of round 5, applied on the user's decision. The dump goes into a buffer on the stack while it fits the 4 KB gbuffer `json2gbuf()` always made, and then into a gbuffer of its size. The first token that would not have fit makes that 4 KB gbuffer with what the stack held, and the dump goes on in it as before. | The bytes; for a dump over 4 KB, the gbuffer and its growth (the same `data_size`). What changes: the free space of the gbuffer of a dump of up to 4 KB (none, where it had the rest of 4 KB). Nothing in the SDK writes into it. Checked against the old one on 100 000 random jsons (six sets of flags) and on dumps of 3 900 to 4 300 bytes, a byte at a time, in three shapes. |
| `linux-ext-libs: jansson's lexer runs for json_loads(), digits, blanks (1.27)` | A commit of Yuneta's fork of jansson (`TAG_JANSSON` moves to it): the runs of the lexer read the source of `json_loads()` too (up to its NUL), and two more kinds of run are taken at once, the digits of a number and the blanks before a token (a newline still goes a byte at a time, for the line count). | Each run does what the byte-by-byte reads did: the same bytes saved, the same position and column, the saved text grown where it was grown (and a growth that fails, failing once, where it failed). Checked: jansson's 215 tests, and a differential fuzz against the previous library (140 000 documents with long numbers, blanks, tabs, CR and newlines, valid and mutated, each loaded with `json_loadb()` and `json_loads()` under five sets of flags, `JSON_DISABLE_EOF_CHECK` among them, the allocator capped at 17 bytes to 2 KB): the same results, errors, lines, columns, positions and allocations. |
| `perf(gbuffer): json2gbuf()'s callback for a big dump, nearly as cheap as before` | The first version called `dump2gbuf()` for every token once the dump no longer fitted the stack, and saved five registers at each call: a big dump cost 1.4% more instructions than before the change. The append is an inline function now (`dump_append()`), and the switch to the gbuffer, once per dump, a function of its own kept out of line. | The same bytes, sizes and growth (the same check). A dump over 4 KB still pays a test per token: +0.3% of the instructions of `perf_tr_treedb_read`, whose `dump_views` dumps 2.4 MB of views ten times. |
| `linux-ext-libs: jansson's lexer takes a run after a byte of its kind (1.28)` | A second commit on the runs of the lexer: they are taken only after a byte of their kind (the blanks before a token after the first one, the digits of a number after its second). The first version called them before every token and for every number, and a compact document with short numbers paid for calls that took nothing. | The same results, errors and positions: jansson's 215 tests and the same differential fuzz. |
| `perf(performance): a benchmark of the treedb reads, perf_tr_treedb_read` | The benchmark of round 5, from the scratch of the audit into `performance/c/`: the reopen of a treedb of 8 000 users, its lists without and with a filter, the views of its nodes as a list and one by one, and the json text of the views. A `RUN_SERIAL` ctest target. | A benchmark: nothing of the SDK. |

## Results

### Instructions (`callgrind`, deterministic), round 5 -> round 6

| Benchmark | Round 5 | Round 6 | Change | Where |
|-----------|---------|---------|--------|-------|
| Inter-yuno round trips (`perf_ievent`) | 44.81 G | 37.16 G | **-17.1%** | the parse of the frames received, 42.6% -> 27.0% of the round trip; `json2gbuf()` 17.9% -> 15.4% |
| Reads with an iterator (`test_topic_pkey_integer_iterator2`) | 4.92 G | 4.64 G | **-5.8%** | the digits of the records parsed at once |
| `perf_timeranger2` | 35.83 G | 35.69 G | -0.4% | |
| Appends (`test_topic_pkey_integer`) | 11.93 G | 11.93 G | 0.0% | |
| MQTT QoS 0 (`perf_mqtt`) | 17.89 G | 17.88 G | 0.0% | it parses no json per message |
| `perf_tr_treedb` | 17.28 G | 17.29 G | +0.1% | |
| `perf_tr_treedb_read` | 10.57 G | 10.60 G | +0.3% | a test per token of a dump over 4 KB (`dump_views`: 2.4 MB, ten times) |
| TCP echo (`perf_tcp_test4`) | 10.54 G | 10.60 G | +0.6% | not in the code changed: spread over the loop and its timers, which this benchmark runs as many times as its wall clock lets it |

### Time (A/B, alternated), round 5 -> round 6

The order turned every round, a `sync` and a 2 s pause before each run; mean
± standard deviation, t = the change over its standard error. This VM was at
its noisiest in this round (10% of standard deviation on the inter-yuno
round trip), so the two figures that mattered were taken again with 16
rounds:

| Benchmark | Round 5 | Round 6 | Change | t |
|-----------|---------|---------|--------|---|
| Inter-yuno round trips/s (`perf_ievent`), 16 rounds | 9 560 ± 1 011 | 11 368 ± 800 | **+18.9%** | 5.6 |
| BFF logins (`perf_auth_bff`, K msg/s), 16 rounds | 4.79 ± 0.39 | 4.76 ± 0.44 | -0.6% | -0.2 |

In the first 8 rounds the BFF showed -8.1% (t = -2.1): the 16 rounds put it
back in its noise. The rest, 8 rounds, moved inside their noise (|t| < 2):
MQTT +2.9%, TCP echo -2.1%, the appends +0.6% and -0.3%, the reads with an
iterator +7.3% (t = 0.9; their instructions say -5.8%), `perf_timeranger2`
and `perf_tr_treedb_read` in both directions.

### Nothing changed

The whole ctest suite, 319 tests with `perf_tr_treedb_read`: the same 9
failures as the base, all of the environment (round 1), and the treedb ones
among them compared message by message with round 5: identical. Two runs
were made with the first versions of the two last changes and one with the
final ones. In the first, `c_mqtt/will_acl` failed once (in 1.4 s, its
normal time, not a guard timeout); it passed in the other two full runs,
alone 8 times out of 8 and with the MQTT, TCP and traffic tests in parallel
15 times out of 15. Its cause was not found (the run kept no output: the
later runs kept it with `--output-on-failure`, and it did not fail again).

## Found, not done

- **jansson's parse is still a third of the inter-yuno round trip after
  this round** (27%): what is left is a call per structural byte (`{`, `:`,
  `,`, quotes) through `stream_get()`, the copy of every string out of the
  saved text, and the objects themselves. A lexer that reads its source
  directly, without the stream of one byte at a time, would answer the
  same, but it is a rewrite of jansson's lexer in the fork, not a run more.
- **`json2gbuf()` for a dump over 4 KB** pays a test per token (+0.3% of
  `perf_tr_treedb_read`). Not worth more code: a list of thousands of
  nodes is dumped once per answer.
- **`c_mqtt/will_acl` failed once** in a full run (see above), and could not
  be made to fail again: noted, not chased further in this round.
