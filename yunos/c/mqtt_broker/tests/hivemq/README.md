# HiveMQ MQTT CLI battery

25 tests of the yuneta `mqtt_broker` driven by the HiveMQ MQTT CLI: the CLI's own
spec compliance checks for MQTT 3.1.1 and 5.0 (tests 1-2), functional tests
(3-20) and performance (21-25, with `mqtt_benchmark.py`).

## Prerequisites

- **Java 11+** (`java -version`)
- **mqtt-cli.jar** (HiveMQ MQTT CLI 4.57.0) in this directory. It is not in git:
  `run_hivemq_tests.sh` downloads it when it is missing, or run
  `./get_mqtt_cli.sh` (pinned version, sha256 checked; `--force` downloads it
  again). To use another version, change `MQTT_CLI_VERSION` and
  `MQTT_CLI_SHA256` in `get_mqtt_cli.sh`.
- `curl` (for the download) and `nc` (the check that the broker is up).
- **Python 3** (tests 22-25)
- **mqtt_broker** running with [`../mqtt_broker.json`](../mqtt_broker.json)
  (see [`../README.md`](../README.md))

## Quick Start

```bash
cd yunos/c/mqtt_broker/tests
rm -rf /tmp/mqtt_broker_test
mqtt_broker --config-file=mqtt_broker.json &

cd hivemq
./get_mqtt_cli.sh                        # optional: run_hivemq_tests.sh does it if needed
./run_hivemq_tests.sh                    # run all tests with defaults
./test_hivemq_mqtt_cli.sh --test 3,5     # run tests 3 through 5
./test_hivemq_mqtt_cli.sh --list         # list all available tests
```

## Files

| File | Description |
|------|-------------|
| `get_mqtt_cli.sh` | Downloads `mqtt-cli.jar` (pinned version, sha256 checked) |
| `test_hivemq_mqtt_cli.sh` | Main test suite (25 tests) using HiveMQ MQTT CLI |
| `run_hivemq_tests.sh` | Wrapper that checks prerequisites before running tests |
| `mqtt_benchmark.py` | Python raw-socket performance benchmark (used by tests 22-25) |

## Usage

```bash
./test_hivemq_mqtt_cli.sh [--host HOST] [--port PORT] [--timeout TIMEOUT]
./test_hivemq_mqtt_cli.sh --test N       # run only test N
./test_hivemq_mqtt_cli.sh --test N,M     # run tests N through M
./test_hivemq_mqtt_cli.sh --list         # list available tests
```

Defaults: `--host 127.0.0.1 --port 1810 --timeout 5`

## Tests 1-20: Protocol Compliance and Functional

| # | Test | What it validates |
|---|------|-------------------|
| 1 | MQTT v3.1.1 Spec Compliance | `mqtt test` against broker |
| 2 | MQTT v5.0 Spec Compliance | `mqtt test --all` against broker |
| 3 | QoS 0 Pub/Sub (v3.1.1) | Fire-and-forget delivery |
| 4 | QoS 1 Pub/Sub (v3.1.1) | At-least-once delivery |
| 5 | QoS 2 Pub/Sub (v3.1.1) | Exactly-once delivery |
| 6 | Retained Message | Retained flag and delivery to new subscribers |
| 7 | Wildcard `+` | Single-level topic wildcard matching |
| 8 | Wildcard `#` | Multi-level topic wildcard matching |
| 9 | Multiple Subscribers | Fan-out to 3 subscribers |
| 10 | QoS 1 Pub/Sub (v5.0) | MQTT v5 at-least-once delivery |
| 11 | User Properties (v5.0) | MQTT v5 user property propagation |
| 12 | Message Expiry (v5.0) | Messages expire after interval |
| 13 | Denied Topic | Broker rejects subscription to `test/nosubscribe` |
| 14 | Shared Subscriptions (v5.0) | `$share` group: each message to exactly one member |
| 15 | Will Message (v3.1.1) | Last will delivered on ungraceful disconnect |
| 16 | Large Payload | 100KB message delivery |
| 17 | Topic Alias (v5.0) | Topic alias assignment and reuse |
| 18 | Persistent Session (v3.1.1) | Queued message delivery on reconnect (clean_session=0) |
| 19 | Session Expiry (v5.0) | Session persistence within expiry window |
| 20 | High-volume Publish | Burst of 50 messages, all delivered |

## Tests 21-25: Performance

| # | Test | What it measures |
|---|------|-----------------|
| 21 | Concurrent Connections | 100 simultaneous connect+pub+disconnect cycles (JVM per connection) |
| 22 | Message Throughput | 1000 messages, 1 pub, 1 sub (Python raw sockets) |
| 23 | Fan-out Scalability | 1 pub to 20 subs, 50 messages = 1000 deliveries (Python raw sockets) |
| 24 | Burst Publish | 10 parallel pubs x 50 msgs = 500 total (Python raw sockets) |
| 25 | **Max Speed Benchmark** | 10000 messages per QoS level (0, 1, 2) (Python raw sockets) |

Test 21 uses HiveMQ CLI (one JVM per connection). Tests 22-25 use `mqtt_benchmark.py` with raw TCP sockets for zero overhead, measuring actual broker throughput.

A test passes only on its evidence, and a test that cannot run FAILS: none is skipped.
Tests 13, 14, 15, 18 and 19 run their clients with `-d` and read the CONNACK and the
SUBACK each client got: test 13 passes only when its subscriber was accepted, its
SUBSCRIBE was refused (0x80) and the message did not arrive; test 14 only when both
members subscribed and each message arrived once across the group; test 15 only when the
will arrives after its client (a `sub` with a will, killed with SIGKILL once in session)
is gone, and not before. Tests 1 and 2 fail when the CLI prints no verdict, and test 2
needs both `MQTT 3: OK` and `MQTT 5: OK`. Test 25 needs a `RESULT` line with
`status=PASS` for each of QoS 0, 1 and 2.

### Performance Benchmark (mqtt_benchmark.py)

Python raw-socket MQTT v3.1.1 benchmark supporting configurable publishers, subscribers, QoS, and message count:

```bash
# Single connection throughput (test 22)
python3 mqtt_benchmark.py --count 1000 --qos 0

# Fan-out: 1 pub to 20 subs (test 23)
python3 mqtt_benchmark.py --count 50 --subs 20 --qos 0

# Burst: 10 parallel pubs (test 24)
python3 mqtt_benchmark.py --count 50 --pubs 10 --qos 0

# Full benchmark: all QoS levels (test 25)
python3 mqtt_benchmark.py --count 10000 --payload 64
```

Reports: publish rate (msg/s), receive count, loss %, end-to-end rate.
Pass thresholds: QoS 0 >=80% (fire-and-forget may drop), QoS 1/2 >=95%.

The spec compliance tests (1 and 2) read the CLI's verdict (`MQTT 3: OK`) from its stdout:
the JVM writes its own warnings on stderr (Java 22+ prints several), and merged with
`2>&1` they can land in the middle of that line.
