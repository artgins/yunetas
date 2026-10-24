# Round 14: the name of an MQTT property, looked up among the names of its length

The last round of the audit. After round 13, the paths that rounds 10 to
13 did not touch (the TCP echo, the inter-yuno messages, MQTT, the logins of
the BFF) were profiled again, to see what was left in them. One local item
was worth a round, and the user chose it alone and closed the audit with it
(2026-10-24); what else was found is written down in the
[README](README.md#what-is-left).

Base: round 13 (`5734099`), on the machine and with the method of the
[README](README.md).

## What the profile showed

In `perf_mqtt` (100 000 messages QoS 0 through the embedded broker),
`strcasecmp()` was 4% of the instructions, nearly all of it under
`mosquitto_string_to_property_info()` (`modules/c/mqtt/src/c_prot_mqtt2.c`):
the name of a property is compared with the 27 names of MQTT v5, one after
the other, about 1 070 instructions per lookup. A message goes through it
three times: the read of its properties (`property_read_all()`, which turns
the identifier read from the wire into its name and the name back into its
type), and the length and the write of the ones it is sent on with
(`property__get_length()`, `property__write()`, from the name of each key of
its property list). 600 000 lookups, 641 million instructions, 4.1%.

## The change (one commit, `eba1b8a`)

**The names are a table with their length, and a name is compared, with the
same `strcasecmp()`, only with the names of its length.**

- **Why the same answers.** `strcasecmp()` compares two strings byte by
  byte, each one put in lower case by the locale: two strings it finds equal
  have the same length, whatever the locale. So the names of another length
  could never answer, and the ones of the same length are asked of
  `strcasecmp()` as before. The order of the table is the order of the old
  chain of `if`s; two different names cannot both be equal to one string,
  so the order decides nothing either.
- **What stays.** An empty or NULL name logs "Mqtt empty property" and
  answers 0 and 0; an unknown one logs "Mqtt unknown property" with the
  name. The deprecated `C_PROT_MQTT` keeps its own copy, untouched.

What a caller can see: nothing. The same identifiers, types, logs and
errors.

### How it was checked

- **The old and the new function side by side**, compiled from the two
  versions of the file, on 10.7 million names: the 27 names with the case of
  each letter chosen at random (200 times each), in upper and in lower case;
  every byte (1 to 255) put in every position of every name; every prefix;
  every name with a byte added; NULL and the empty string; and 2 million
  random strings of 1 to 40 bytes, of letters, `-`, `_`, Latin-1 letters and
  any byte. In five locales: `C`, `C.utf8`, `tr_TR.ISO-8859-9`,
  `tr_TR.UTF-8` (where `strcasecmp()` does not take `I` for `i`, and
  `SUBSCRIPTION-IDENTIFIER` is not `subscription-identifier`) and
  `es_ES.ISO-8859-1`. No difference in the return, the identifier, the type,
  or the log.
- **The suite of the SDK** on the result (every yuno rebuilt), 319 tests:
  the 9 failures of this machine, each one with the same messages as in
  rounds 9 to 13, and nothing else. A first run had four more, all
  "inotify_init1() FAILED": the container had restarted and taken the limit
  of inotify instances back to 128. With it at 8192 again, those four pass
  and the whole suite was run again.

## Measured

### Instructions (`callgrind`), round 13 -> round 14

| Benchmark | Round 13 | Round 14 | Change |
|-----------|----------|----------|--------|
| `perf_mqtt` (100 000 messages, QoS 0) | 15.87 G | 15.34 G | **-3.3%** |

The lookup: 1 070 -> 215 instructions, 641 M -> 129 M in all. The other
benchmarks do not run MQTT.

### Time (A/B, alternated), round 13 -> round 14

The two binaries in turn, 20 rounds, the order turned every round, with a
`sync` and a 2 s pause before each run. Mean ± standard deviation; t is the
change over its standard error (|t| < 3 is noise).

| Case | Round 13 | Round 14 | Change, t |
|------|----------|----------|-----------|
| `perf_mqtt`, QoS 0 round trips/s | 27 911 ± 2 262 | 27 702 ± 2 244 | -0.8%, -0.3 |

Within its noise (8% of the case): 3% of the instructions does not show
against it, and a round trip spends much of its time in the kernel, which
`callgrind` does not count. The instructions are the figure of this round.
