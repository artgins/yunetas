# MQTT Broker Test Suite

Tests for the yuneta MQTT broker covering MQTT v3.1.1 and v5.0 protocol compliance, functional behavior, and performance.

## Prerequisites

- **Java 11+** (`java -version`)
- **mqtt-cli.jar** (HiveMQ MQTT CLI) in this directory
- **Python 3** (for low-level protocol tests)
- **mqtt_broker** running on the target host/port

> **⚠️ Warning:** If you don't use 'yuneta' as linux user, you must set the attribute `allow_anonymous_in_localhost` of C_AUTHZ to true.

## Quick Start

```bash
# Start the broker first, then:
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
| 14 | Shared Subscriptions (v5.0) | `$share` group load balancing |
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
- **MQTT v5.0 CONNECT**: clean start, will with properties, session expiry, receive maximum, max packet size, invalid flags, oversized fields
- **MQTT v3.1.1 DISCONNECT**: Basic `E0 00` packet
- **MQTT v5.0 DISCONNECT**: Reason codes (`0x00`, `0x04`, `0x80`) with properties

### Pass Criteria

- **Invalid CONNECT**: broker closes socket or sends non-success CONNACK
- **Valid CONNECT**: broker sends success CONNACK (code 0)
- **DISCONNECT**: broker closes connection without error

## Broker Configuration (mqtt_broker.json)

Test configuration includes:
- Authorization service (`C_AUTHZ`)
- MQTT broker service (`C_MQTT_BROKER`) with `deny_subscribes: ["test/nosubscribe"]`
- TCP server on configurable port with `C_PROT_MQTT2` protocol handler
