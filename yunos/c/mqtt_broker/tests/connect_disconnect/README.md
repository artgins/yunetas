# CONNECT / DISCONNECT protocol tests

`mqtt_connect_disconnect_test.py` crafts raw MQTT packets to test the edge cases
of CONNECT and DISCONNECT against the yuneta `mqtt_broker` (22 rows). Python 3,
no extra package.

## Run

```bash
cd yunos/c/mqtt_broker/tests
rm -rf /tmp/mqtt_broker_test
mqtt_broker --config-file=mqtt_broker.json &

cd connect_disconnect
./test_connect_disconnect.sh                       # 127.0.0.1:1810, timeout 3 s
python3 mqtt_connect_disconnect_test.py --host 127.0.0.1 --port 1810
```

The script exits 1 when a row fails.

## What it tests

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

Against `../mqtt_broker.json`: 22 of 22 (HiveMQ, in `../hivemq`, 25 of 25) (every row runs), and
the broker logs no ERROR. In 7.30.26 the Python script printed PASS for the three MQTT 5.0
DISCONNECT rows without running them, and its `0x80` row wrapped the properties twice
(`e0098007061f0003627965`): a malformed packet that the broker answers with DISCONNECT
0x81 (and, in 7.30.26, with two ERRORs in its log).
