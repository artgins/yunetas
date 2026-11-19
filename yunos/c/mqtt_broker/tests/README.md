# MQTT Broker tests

This directory holds three test batteries for the yuneta `mqtt_broker`, one in each subdirectory:

| Directory | What it is | Result now |
|-----------|------------|------------|
| [`hivemq/`](hivemq/README.md) | HiveMQ MQTT CLI: its spec compliance tests (MQTT 3.1.1 and 5.0), functional tests, and performance tests (25 tests). | 25 of 25 |
| [`connect_disconnect/`](connect_disconnect/README.md) | Raw-packet CONNECT / DISCONNECT edge cases (Python, 22 rows). | 22 of 22 |
| [`mosquitto/`](mosquitto/README.md) | The broker test suite of Eclipse Mosquitto (about 270 tests). It runs through a stand-in for the `mosquitto` binary. It starts its own brokers. | 83 pass, see its README |

`mqtt_broker.json` (in this directory) is the broker configuration of the three
batteries. `hivemq/` and `connect_disconnect/` run against a broker that you
start with it. `mosquitto/` uses it as the template of the configuration of each test.

## Run them

`hivemq/` and `connect_disconnect/` need a broker that runs with `mqtt_broker.json`.
That broker listens on port 1810, and its store is `/tmp/mqtt_broker_test`:

```bash
cd yunos/c/mqtt_broker/tests
rm -rf /tmp/mqtt_broker_test               # start with an empty store
mqtt_broker --config-file=mqtt_broker.json &

(cd hivemq && ./run_hivemq_tests.sh)                 # downloads mqtt-cli.jar if missing (needs Java 11+)
(cd connect_disconnect && ./test_connect_disconnect.sh)

kill -INT %1                               # stop the broker (orderly shutdown)
```

`mosquitto/` starts a broker for each test (ports 1888-1895). It does not need a
broker that runs before it:

```bash
cd yunos/c/mqtt_broker/tests/mosquitto
./setup.sh                                 # once: fetches Mosquitto's test/ and patches it
./run_all.sh                               # every test, about 7 minutes
```

`mqtt_broker` is the binary in `outputs/yunos/` (`yunetas build`). If you sourced
`yunetas-env.sh`, its path is `$YUNETAS_YUNOS/mqtt_broker`. The README of each
subdirectory tells how to run one test and how to read the results.

## Broker Configuration (mqtt_broker.json)


The file configures a standalone broker (no agent). The suites use it with no changes:

- It listens on `mqtt://0.0.0.0:1810` (the default port of the scripts), with 200 channels.
  Test 21 opens 100 connections, and test 23 keeps 21 open at the same time.
- `"yuno": {"autoplay": true}`: a standalone yuno has no agent to play it. The broker opens
  its database and finds its `__input_side__` when it starts. But it gives the input side
  its queues and starts it (the listener) only when it plays.
- `C_PROT_MQTT2` with `"iamServer": true`: without this key, the protocol is a client and
  sends a CONNECT to every peer.
- `Authz.allow_anonymous_in_localhost`: the broker accepts a client with no credentials
  from the same host, for any Linux user that runs the broker. Without this key, the
  broker accepts such a client only when the user is `yuneta`.
- `Mqtt_broker.enable_new_clients`: the broker creates an unknown client id at its first
  CONNECT. Without this key, the broker closes every CONNECT of a new id, with the INFO
  *"Client not exist and no auto-create"*. Set this key in `global`. A different `kw` in
  the `services` list makes the configuration merge create the service two times.
- `C_MQTT_BROKER` with `deny_subscribes: ["test/nosubscribe"]` (test 13).
- An empty `__top_side__`: the broker looks for that service when it plays.
- `work_dir` `/tmp/mqtt_broker_test`: the store of the run. To start with an empty store, remove it.
