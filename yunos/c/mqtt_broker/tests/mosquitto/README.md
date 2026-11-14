# Mosquitto's broker test suite against `mqtt_broker`

Eclipse Mosquitto ships about 270 broker tests in Python (`test/broker/*.py`).
Each test starts a broker with `$BUILD_ROOT/src/mosquitto -c <conf> -p <port>`,
talks raw MQTT to it, and compares the bytes it receives with the bytes it expects.
This directory runs those tests against the yuneta `mqtt_broker`, with a stand-in
for the `mosquitto` binary.

The suite is **not** copied into this repository. `setup.sh` fetches `test/` from
the Mosquitto repository at a pinned commit (Mosquitto is EPL-2.0 OR BSD-3-Clause).
Only the scripts, the stand-in, a small patch and the reference results live here.

## Files

| File | What it is |
|------|------------|
| `setup.sh` | Fetches `test/` of Mosquitto at the pinned commit into the work directory and applies `mosq_test.patch`. Run it once. |
| `run_all.sh` | Runs the tests one at a time (60 s limit each) and writes one output per test plus `summary.txt`. |
| `standin_mosquitto.py` | The stand-in for the `mosquitto` binary. It reads the test's `-p` and `-c <conf>`, writes a yuneta config from `../mqtt_broker.json`, and runs the broker. |
| `mosq_test.patch` | Patch to the suite's `mosq_test.py`: compares whole packets and accepts the encodings the spec allows (see the patch header). The tests themselves are not changed. |
| `features.env` | The suite's feature switches. A test of a feature that is OFF exits 77 (skipped). |
| `classify.py` | Classifies a run, and compares it with a reference run. |
| `digest.py` | Short form of one test output: the test's lines and the broker's WARNING/ERROR messages. |
| `baseline.txt` | `summary.txt` of the reference run (see *Current results*). |

## Prerequisites

- `git`, `patch`, Python 3 (the suite needs no extra Python package for these tests).
- A built `mqtt_broker` (`yunetas build`). By default the scripts use
  `$YUNETAS_YUNOS/mqtt_broker`, else `/yuneta/development/yunetas/outputs/yunos/mqtt_broker`.
- TCP ports 1888 to 1895 free. A broker on 1883 does not interfere.

## Set up (once)

```bash
cd /yuneta/development/yunetas/yunos/c/mqtt_broker/tests/mosquitto
./setup.sh                          # work dir: $MOSQ_WORK, default ~/.yuneta/mosquitto-suite
./setup.sh /path/to/work_dir        # or name it
```

The work directory holds the Mosquitto clone (`mosquitto-src/`, only `test/`
checked out), the patched copy (`test/`), the broker under test and the results.
Running `setup.sh` again recreates `test/` from the clone.

## Run

```bash
./run_all.sh                                        # every test, about 7 minutes
./run_all.sh 07-will-disconnect-with-will.py 05-session-expiry-v5.py   # some tests

MQTT_BROKER=/other/build/mqtt_broker ./run_all.sh   # another binary
OUT=~/.yuneta/mosquitto-suite/results-x ./run_all.sh  # choose the results directory
MOSQ_WORK=/path/to/work_dir ./run_all.sh            # another work directory
```

Each run copies the broker binary, `../mqtt_broker.json` and the stand-in into the
work directory. Thus a rebuild during a run does not change what is tested.
The results go to `$MOSQ_WORK/results-<date>/` (or `$OUT`):

- `<test>.py.txt`: the output of the test, with the broker's log.
- `summary.txt`: one line per test, `<test> <exit code> <time>`.
- `broker.txt`: the `--version` of the broker under test.

Exit codes: **0** = pass, **77** = skipped (feature OFF), anything else = fail.

At the end of a full run, `run_all.sh` prints the counts per class and every test
whose exit code changed against `baseline.txt`. A test that fails and is not in the
table of `classify.py` is printed as `NEW`: a regression, or a test the suite added.

## Read the results

```bash
R=~/.yuneta/mosquitto-suite/results-<date>
awk '{print $2}' $R/summary.txt | sort | uniq -c       # counts per exit code
python3 classify.py $R/summary.txt baseline.txt        # changes, counts, one row per test
python3 digest.py $R/05-session-expiry-v5.py.txt       # short form of one failure
```

Classes of `classify.py`:

| Class | Meaning |
|-------|---------|
| `PASS` | Exit 0. |
| `U` | Feature the yuneta broker does not have: TLS, websockets, unix sockets, plugins, password and ACL files, bridges, enhanced authentication (AUTH), `$CONTROL`, `mount_point`... Skipped with 77, or fails on it. |
| `U*` | Exit 0, but not a pass: the test never runs its case, or passes by coincidence. |
| `L` | The test checks Mosquitto itself: its files, its command line, its config parser. |
| `S` | The spec allows both behaviours and the test expects Mosquitto's choice. |
| `T` | The test itself does not run (a bug in the suite). |
| `NEW` | Fails and is not classified. Look at it. |

## Debug one test

Run one test by hand from the work directory:

```bash
W=~/.yuneta/mosquitto-suite
. features.env
cd $W/test/broker
BUILD_ROOT=$W/root python3 07-will-disconnect-with-will.py
```

`run_all.sh` must have run once, because it installs the broker and the stand-in
in `$W`. Environment variables of the stand-in:

| Variable | Effect |
|----------|--------|
| `YBROKER_KEEP=1` | Keep the broker's run directory (`$W/runs/ybroker-*`: its config, store and logs). |
| `YBROKER_TRACE='{"C_PROT_MQTT2":["show-decode"]}'` | GClass trace levels for the broker (`yuno.trace_levels`). |
| `YBROKER_ARGS='--global-trace=machine'` | Extra arguments for the broker. |
| `STANDIN_SUBACK_REORDER=1` | What-if pass of the patched `mosq_test.py`: a PUBLISH received before the expected SUBACK is kept for later (see `S` below). |

The stand-in writes `STANDIN: listener <port> pool <n> prot {...}` and every option
it cannot translate (`STANDIN: unsupported option ...`) to stderr, which goes into
the test output.

## What the stand-in translates

- One `C_TCP_S` per `listener` / `port` / `-p`, each with its own pool of
  `C_CHANNEL` / `C_PROT_MQTT2`. Thus the per-listener options stay per listener.
  The pool size is `max_connections` (100 if not set). `global_max_connections`
  makes one pool for all listeners.
- Options with a yuneta equivalent go into the `C_PROT_MQTT2` kw: `max_keepalive`,
  `message_size_limit`, `max_inflight_messages`, `max_inflight_bytes`,
  `max_queued_messages`, `max_queued_bytes`, `max_packet_size`,
  `retain_available`, `max_qos`, `max_topic_alias`, `allow_anonymous` (with
  `per_listener_settings`), `allow_zero_length_clientid`.
- Mosquitto 2 defaults where the yuneta default is different: `max_keepalive`
  65535, no limit for subscriptions per client and topic filter length,
  `max_packet_size` 2000000. Anonymous clients are allowed only when the config
  file defines no listener. `Authz.allow_anonymous_in_localhost` is false.
- `persistence true`: the store is kept across the broker restarts of one test.
  It is cleared when the test deletes its persistence file.
- `include_dir` is read.
- Logging, tuning and file options of Mosquitto are ignored; all other options are
  reported as unsupported.

## Current results

Reference run (`baseline.txt`): Mosquitto `fda14f2c` (2.1.2, develop, 2026-11-05),
`mqtt_broker` at yunetas `1bf82a8f0`, on the dev machine, 2026-11-14:

| Exit 0 | Exit 1 | Exit 77 | Total |
|--------|--------|---------|-------|
| 83 | 82 | 105 | 270 |

By class: 73 `PASS`, 10 `U*`, 171 `U`, 4 `L`, 10 `S`, 2 `T`.
With 7.31.1 (before the fixes of `e49f1c7a3`) 68 tests exited 0.

The 10 `S` failures:

- **B1, retained messages before the SUBACK** (6 tests, e.g.
  `02-subpub-qos0-send-retain`). The broker sends the retained PUBLISHes of a new
  subscription before its SUBACK. MQTT-3.8.4 permits this ("The Server is
  permitted to start sending PUBLISH packets matching the Subscription before the
  Server sends the SUBACK"). Mosquitto sends the SUBACK first and the tests expect
  that order. With `STANDIN_SUBACK_REORDER=1` four of them pass.
- `01-connect-575314`: the CONNECT of 65 KB is accepted, but its client id of
  65000 characters is refused. In this broker a client id is a directory of the
  store, so it must be shorter than `NAME_MAX - 4`. MQTT-3.1.3-5 lets a server
  refuse it.
- `01-connect-max-keepalive` (v3.1.1 keepalive above `max_keepalive`: Mosquitto
  refuses, the yuneta broker accepts), `03-publish-qos2-reuse-mid` (QoS 2 PUBLISH
  that reuses an unreleased packet id: the yuneta broker keeps the first,
  MQTT-4.3.3-9), `07-will-oversize-payload` (`message_size_limit` is not applied to
  the will payload at CONNECT).

## Update the pinned commit

1. Change `MOSQ_COMMIT` in `setup.sh` and run it. If `mosq_test.patch` does not
   apply, adapt it to the new `test/mosq_test.py`.
2. Run the whole suite. Classify each `NEW` test: a broker defect (fix it), or a
   class of the table in `classify.py` (add it there).
3. Copy the new `summary.txt` over `baseline.txt` and update *Current results*.
