# Round 5: subscriptions, and the reads of a treedb

Scope: what was left of the cost of making a subscription (2.4% of an
inter-yuno round trip in round 4), and the reads of a treedb: its lists, the
views of its nodes (`node_collapsed_view()`, what a `list-nodes` or a
`get-node` answers through), and its open. Measured on top of the proposals
(`proposals-applied.md`): what is compared here is the branch with the
proposals against the branch with round 5.

## How it was measured

- **Subscriptions**: a benchmark in the scratch of the audit (not in the
  tree): a subscription made and withdrawn on a publisher that already holds
  M subscriptions, as every `C_TIMER` start and stop did on its yuno, and a
  publication to M subscribers.
- **treedb reads**: a benchmark in the scratch of the audit: 8 000 users
  linked to 200 departments (each user in one to three), the treedb closed
  and opened again, then every user listed, listed with a filter, viewed
  (`node_collapsed_view()`, the default options) one by one and as a list,
  and every department viewed (40 users in its hook).
- **Inter-yuno and MQTT**: `perf_ievent` and `perf_mqtt` (round 4).

## What the profile showed

**Subscriptions.** The proposals already took the subscription off the path
of a message: a `C_TIMER` keeps its subscription to the yuno for its life,
so the 2.4% of round 4 is gone from the inter-yuno round trip (no
subscription is made per message any more), and a publication reads C
records. What is left, on the scratch benchmark (one run of each build, CPU time
per operation):

| | Round 4 | With the proposals |
|---|---|---|
| Subscribe and unsubscribe, publisher with no other subscription | 3.1 µs | 2.8 µs |
| ... with 10 | 6.7 µs | 2.4 µs |
| ... with 100 | 17.6 µs | 4.8 µs |
| ... with 1 000 | 129 µs | 28 µs |
| Publication to 10 subscribers | 3.2 µs | 0.7 µs |
| ... to 100 | 24.6 µs | 8.9 µs |
| ... to 1 000 | 335 µs | 64 µs |

What a subscription still costs is a search of the publisher's
subscriptions (a duplicate is looked for before making one, and the one to
withdraw is looked for), linear in their number, and the json of the
subscription. It happens when a gobj subscribes, not per message: nothing
was changed for it.

**treedb reads**, `callgrind`, share of the instructions of the benchmark:

| Where | Share |
|---|---|
| `node_collapsed_view()`, the view of a node, of which: | **57%** |
| ... `json_pack()` of every entry of an fkey or a hook (`{"id", "topic_name"[, "hook_name"]}`) | **19%** |
| ... `kw_get_bool()` of up to six options, read again for every entry | 8.7% |
| ... `decode_parent_ref()`: every fkey ref cut with `split2()` (five allocations), then copied again | 6.2% |
| The open (8 000 nodes; its record per key and its links) | 11% |
| A filtered list: `kw_has_word()` twice for the flags of every column of the filter, for every node | 1.3% |

The open is now the read of one record per key and its links (the
proposals): what is left is the read of the files and the parse of each
record.

**Inter-yuno**: jansson writes the key of every object and array it dumps or
deep-copies (to catch a loop) with `snprintf("%p")`: 15 printf calls per
message, 1.8% of the round trip.

## Changes applied (one commit each)

| Commit | Change | What stays the same |
|--------|--------|---------------------|
| `perf(tr_treedb): a view reads its options once and builds its entries directly` | The style of the entries of an fkey or a hook (only_id, list_dict, refs, the default) is read once for the whole view when it cannot change between them and reading it logs nothing: the options are a dict and each one is absent or a boolean, a number, a string or null (`view_style_is_constant()`); empty options are the default at once. Each entry is built as `json_pack("{s:s, ...}")` built it, without parsing a format (`strings_dict()`). | Options of another type, or options that are not a dict, are read for every entry, as before, with their logs. The same keys in the same order, and nothing built when a value is NULL or not UTF-8. Checked against the previous `tr_treedb.c`: the view and the parent refs of every node of a treedb with hooks and fkeys, with 26 sets of options (valid, numbers and strings as booleans, of the wrong type, not a dict): the same json and the same logs. |
| `perf(tr_treedb): decode_parent_ref() without split2() for a ref that fits` | The three parts of `topic^id^hook` are found in place, as `split2(pref, "^")` finds them (`strtok_r()`: runs without `^`, the empty ones skipped), copied once and checked. | A ref that is not three parts, a part too long for its buffer, or a NULL buffer goes through `split2()` as before, with its logs. Checked on 200 000 random refs (runs of `^`, empty parts, backticks, UTF-8 and broken UTF-8, parts of up to 600 bytes, buffers of 8 bytes, NULL buffers): the same answer, the same buffers, the same logs. |
| `perf(tr_treedb): a filtered list reads the flags of a column once` | `match_node_simple()` reads the flags with `get_col_view_flags()`, the one walk the view uses since round 4. | It answers for "fkey" and "hook" what `kw_has_word(gobj, flags, word, 0)` answers, and logs nothing (checked in round 4 on 1.5 million random flags). 30 filtered lists (plain columns, fkeys and hooks as a string, a list and a dict, a column that does not exist, a dict as a value): the same nodes, the same logs. |
| `linux-ext-libs: jansson's loop key without snprintf() (1.26)` | A commit of Yuneta's fork of jansson: the key of the loop check written by hand, the same text glibc's `"%p"` writes ("0x" and the hex digits); `TAG_JANSSON` moves to it. | A buffer too short for it, or a NULL json, still goes to `snprintf()`. Checked against `snprintf()` on 20 million addresses and buffer sizes (the same bytes and length), and jansson's 215 tests. Every node that builds from source reruns `./extrae.sh && ./configure-libs.sh`. |

## Results

### Instructions (`callgrind`, deterministic), the proposals -> round 5

| Benchmark | Before | After | Change |
|-----------|--------|-------|--------|
| treedb reads (the scratch benchmark, all its cases) | 14.87 G | 12.38 G | **-16.7%** |
| ... of which the views of the nodes (`node_collapsed_view()`) | 8.41 G | 5.89 G | **-30.0%** |
| Inter-yuno round trips (`perf_ievent`) | 45.51 G | 44.81 G | -1.5% |
| MQTT QoS 0 (`perf_mqtt`) | 17.88 G | 17.89 G | 0.0% (it dumps few containers) |

### Time (A/B, alternated), the proposals -> round 5

The order turned every round, a `sync` and a 2 s pause before each run; mean
± standard deviation, t = the change over its standard error. The treedb
reads, 16 rounds (8 were not enough on this VM to tell a 10% move from its
noise), µs of CPU:

| Case | Before | After | Change | t |
|------|--------|-------|--------|---|
| The view of a user, one by one (`get_node` + `node_collapsed_view()`) | 7.76 ± 1.29 | 6.49 ± 0.75 | **-16.4%** | 3.4 |
| The views of every user, as a list, per user | 12.37 ± 2.26 | 10.74 ± 1.50 | **-13.2%** | 2.4 |
| The views of every department (40 users in a hook), as a list, per department | 215.3 ± 17.7 | 192.5 ± 14.7 | **-10.6%** | 4.0 |
| A list, without and with a filter, per node | 0.114 / 0.897 | 0.122 / 0.930 | noise | < 1 |
| The open, per node | 82.2 ± 11.9 | 79.8 ± 10.6 | -2.9% | 0.6 |

The rest, 8 rounds: `perf_ievent` (+0.0%), `perf_mqtt` (-1.8%),
`perf_auth_bff` (-1.1%), `perf_tr_treedb` and `perf_c_treedb` moved inside
their noise (|t| < 1.5). The inter-yuno round trip's 1.5% fewer
instructions are below what this VM can show; so is the filtered list's
part (1.3% of the benchmark), and the open's share of `decode_parent_ref()`.

### Nothing changed

The whole ctest suite: 318 tests, the same 9 failures as the base, all of
the environment (round 1), and the treedb ones among them compared message
by message with the base and with the branch before this round: identical.
The benchmarks of the suite pass.

## Found, not done

The first, the third and the fourth were done in
[round 6](round-6-jansson-parse-treedb-reads.md), on the user's decision.


- **`json2gbuf()` makes a gbuffer of 4 KB, zeroed, for every json it
  dumps** (`gbuffer_create(4*1024, ...)`), whatever its size: two per
  inter-yuno message, 2.3% of the round trip. Sized to the dump (or grown
  from a small one), the bytes are the same, but the free space of the
  buffer is not: a consumer that appends to the buffer it receives would
  grow it instead of finding the rest of 4 KB free. The same kind of change
  as the TLS read buffer and the HTTP body buffer (applied with the
  proposals): a proposal.
- **The search of a publisher's subscriptions is linear** in their number
  (28 µs to make and withdraw one on a publisher with 1 000). An index by
  subscriber would make it constant, but it is a structure more in the core
  of gobj-c for a cost that is paid when a gobj subscribes, not per message:
  left.
- **jansson's parse** is now the largest part of an inter-yuno round trip (the lexer
  reads a byte at a time through a function call: `lex_scan()`,
  `stream_get()`, `strbuffer_append_bytes()` and around them, 26% of the
  round trip).
  Reading the runs of a number or of whitespace at once, as round 2 did for
  the runs of a string, would answer the same; a change in the fork of
  jansson, for a next round.
- **A benchmark of the treedb reads in the tree.** The one of this round
  lives in the scratch of the audit; `performance/c/` measures the treedb
  writes only (`perf_tr_treedb`). It can be added, as `perf_ievent` and
  `perf_mqtt` were.
