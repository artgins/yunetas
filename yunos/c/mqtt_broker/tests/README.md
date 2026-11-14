# MQTT Broker tests

Three test batteries for the yuneta `mqtt_broker`, one per directory:

| Directory | What it is | Result now |
|-----------|------------|------------|
| [`hivemq/`](hivemq/README.md) | HiveMQ MQTT CLI: its spec compliance checks (MQTT 3.1.1 and 5.0), functional tests and performance (25 tests). | 25 of 25 |
| [`connect_disconnect/`](connect_disconnect/README.md) | Raw-packet CONNECT / DISCONNECT edge cases (Python, 22 rows). | 22 of 22 |
| [`mosquitto/`](mosquitto/README.md) | Eclipse Mosquitto's own broker test suite (about 270 tests), through a stand-in for the `mosquitto` binary. It starts its own brokers. | 83 pass, see its README |

`mqtt_broker.json` (here) is the broker configuration of the three: `hivemq/` and
`connect_disconnect/` run against a broker started with it, and `mosquitto/`
uses it as the template of the config of each test.

## Run them

`hivemq/` and `connect_disconnect/` need a broker started with `mqtt_broker.json`
(it listens on 1810, its store is `/tmp/mqtt_broker_test`):

```bash
cd yunos/c/mqtt_broker/tests
rm -rf /tmp/mqtt_broker_test               # start with an empty store
mqtt_broker --config-file=mqtt_broker.json &

(cd hivemq && ./run_hivemq_tests.sh)                 # downloads mqtt-cli.jar if missing (needs Java 11+)
(cd connect_disconnect && ./test_connect_disconnect.sh)

kill -INT %1                               # stop the broker (orderly shutdown)
```

`mosquitto/` starts a broker for each test (ports 1888-1895), so it needs no
broker running:

```bash
cd yunos/c/mqtt_broker/tests/mosquitto
./setup.sh                                 # once: fetches Mosquitto's test/ and patches it
./run_all.sh                               # every test, about 7 minutes
```

`mqtt_broker` is the binary in `outputs/yunos/` (`yunetas build`); with
`yunetas-env.sh` sourced, `$YUNETAS_YUNOS/mqtt_broker`. Each directory's README
says how to run one test and how to read the results.

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
