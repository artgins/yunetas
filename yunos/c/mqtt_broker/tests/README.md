# MQTT Broker Test Suite

Tests for the yuneta MQTT broker covering MQTT v3.1.1 and v5.0 protocol compliance, functional behavior, and performance.

Mosquitto's own broker test suite (about 270 tests) runs against `mqtt_broker` too:
see [`mosquitto/README.md`](mosquitto/README.md).

## Prerequisites

- **Java 11+** (`java -version`)
- **mqtt-cli.jar** (HiveMQ MQTT CLI) in this directory
- **Python 3** (for low-level protocol tests)
- **mqtt_broker** running on the target host/port

## Quick Start

```bash
# Start the broker with the test configuration (foreground; store in /tmp/mqtt_broker_test)
rm -rf /tmp/mqtt_broker_test
mqtt_broker --config-file=mqtt_broker.json &

# Then:
./run_hivemq_tests.sh                    # run all tests with defaults
./test_hivemq_mqtt_cli.sh --test 3,5     # run tests 3 through 5
./test_hivemq_mqtt_cli.sh --list         # list all available tests

# Low-level protocol tests
python mqtt_connect_disconnect_test.py --host 127.0.0.1 --port 1810
```

## Files

| File | Description |
|------|-------------|
| `test_hivemq_mqtt_cli.sh` | Main test suite (25 tests) using HiveMQ MQTT CLI |
| `run_hivemq_tests.sh` | Wrapper that checks prerequisites before running tests |
| `mqtt_benchmark.py` | Python raw-socket performance benchmark (used by tests 22-25) |
| `mqtt_connect_disconnect_test.py` | Low-level CONNECT/DISCONNECT protocol tests (Python) |
| `test_connect_disconnect.sh` | Simple wrapper for the Python tests |
| `mqtt_broker.json` | Broker configuration used for testing |

## Test Suite (test_hivemq_mqtt_cli.sh)

### Usage

```bash
./test_hivemq_mqtt_cli.sh [--host HOST] [--port PORT] [--timeout TIMEOUT]
./test_hivemq_mqtt_cli.sh --test N       # run only test N
./test_hivemq_mqtt_cli.sh --test N,M     # run tests N through M
./test_hivemq_mqtt_cli.sh --list         # list available tests
```

Defaults: `--host 127.0.0.1 --port 1810 --timeout 5`

### Tests 1-20: Protocol Compliance and Functional

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

### Tests 21-25: Performance

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

#### Performance Benchmark (mqtt_benchmark.py)

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

## Python Protocol Tests (mqtt_connect_disconnect_test.py)

Low-level tests that craft raw MQTT packets to test protocol edge cases:

- **MQTT v3.1.1 CONNECT**: clean session, will flag/QoS/retain, username/password, keepalive edges, empty client ID, reserved flag abuse, wrong protocol name/level, malformed remaining length
- **MQTT v5.0 CONNECT**: clean start, will with properties, session expiry, receive maximum, max packet size, invalid flags, oversized fields, and a password with no username (valid in MQTT 5.0, 3.1.2.9; refused in 3.1.1)
- **MQTT v3.1.1 DISCONNECT**: Basic `E0 00` packet
- **MQTT v5.0 DISCONNECT**: Reason codes `0x00`, `0x04`, and `0x80` with a Reason String property

### Pass Criteria

- **Invalid CONNECT**: the broker closes the connection (end of stream or reset) or sends a
  CONNACK with a failure code. A timeout (the connection left open and silent), any packet
  that is not a CONNACK, or a CONNACK cut short fails: in 7.30.26 they passed, so a broker
  that never refused an invalid CONNECT got 22 of 22.
- **Valid CONNECT**: broker sends success CONNACK (code 0)
- **DISCONNECT**: each row opens its own connection (its CONNECT must succeed) and sends
  the DISCONNECT. It passes when the broker sends nothing back: the broker may close the
  connection, or wait for the client to close it, as an MQTT 5.0 server may. Any byte back
  (a DISCONNECT 0x81 Malformed Packet, say) fails. Then the test closes the connection, as
  the spec asks of the sender of a DISCONNECT.
- A row that cannot run FAILS. The script exits 1 when a row fails.

Against `mqtt_broker.json` (below): HiveMQ 25 of 25, Python 22 of 22 (every row runs), and
the broker logs no ERROR. In 7.30.26 the Python script printed PASS for the three MQTT 5.0
DISCONNECT rows without running them, and its `0x80` row wrapped the properties twice
(`e0098007061f0003627965`): a malformed packet that the broker answers with DISCONNECT
0x81 (and, in 7.30.26, with two ERRORs in its log).

The spec compliance tests (1 and 2) read the CLI's verdict (`MQTT 3: OK`) from its stdout:
the JVM writes its own warnings on stderr (Java 22+ prints several), and merged with
`2>&1` they can land in the middle of that line.

## Broker Configuration (mqtt_broker.json)

A standalone broker (no agent) that the suites can use as it is:

- Listens on `mqtt://0.0.0.0:1810` (the scripts' default port), with 200 channels:
  test 21 opens 100 connections and test 23 holds 21 at once.
- `"yuno": {"autoplay": true}`: a standalone yuno has no agent to play it. The broker opens
  its database and finds its `__input_side__` when it starts, but it hands the input side
  its queues and starts it (the listener) only when it plays.
- `C_PROT_MQTT2` with `"iamServer": true`: without it the protocol is a client and sends a
  CONNECT to every peer.
- `Authz.allow_anonymous_in_localhost`: a client with no credentials, from the same host,
  is accepted whatever the Linux user that runs the broker (without it only `yuneta` is).
- `Mqtt_broker.enable_new_clients`: an unknown client id is created at its first CONNECT
  (without it every CONNECT of a new id is closed, with the INFO *"Client not exist and no
  auto-create"*). Set it in `global`: a different `kw` in the `services` list makes the
  config merge create the service twice.
- `C_MQTT_BROKER` with `deny_subscribes: ["test/nosubscribe"]` (test 13).
- An empty `__top_side__`: the broker looks for that service at play.
- `work_dir` `/tmp/mqtt_broker_test`: the store of the run. Remove it to start clean.
