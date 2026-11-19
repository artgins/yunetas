# CONNECT / DISCONNECT protocol tests

`mqtt_connect_disconnect_test.py` makes raw MQTT packets to test the edge cases
of CONNECT and DISCONNECT against the yuneta `mqtt_broker` (22 rows). It needs
Python 3 and no extra package.

## Run

```bash
cd yunos/c/mqtt_broker/tests
rm -rf /tmp/mqtt_broker_test
mqtt_broker --config-file=mqtt_broker.json &

cd connect_disconnect
./test_connect_disconnect.sh                       # 127.0.0.1:1810, timeout 3 s
python3 mqtt_connect_disconnect_test.py --host 127.0.0.1 --port 1810
```

If a row fails, the script exits 1.

## What it tests

These are low-level tests. They make raw MQTT packets to test the edge cases of the protocol:

- MQTT v3.1.1 CONNECT: clean session, will flag/QoS/retain, username/password, keepalive edges, empty client ID, reserved flag abuse, wrong protocol name/level, malformed remaining length.
- MQTT v5.0 CONNECT: clean start, will with properties, session expiry, receive maximum, max packet size, invalid flags, oversized fields, and a password with no username. MQTT 5.0 accepts that password (3.1.2.9). MQTT 3.1.1 refuses it.
- MQTT v3.1.1 DISCONNECT: Basic `E0 00` packet.
- MQTT v5.0 DISCONNECT: Reason codes `0x00`, `0x04`, and `0x80` with a Reason String property.

### Pass Criteria

- Invalid CONNECT: the row passes if the broker closes the connection (end of stream or reset), or
  sends a CONNACK with a failure code. These results fail: a timeout (the connection stays open
  and silent), any packet that is not a CONNACK, and a CONNACK cut short. In 7.30.26 they passed.
  Thus a broker that never refused an invalid CONNECT got 22 of 22.
- Valid CONNECT: the broker sends a success CONNACK (code 0).
- DISCONNECT: each row opens its own connection (its CONNECT must succeed) and sends
  the DISCONNECT. The row passes if the broker sends nothing back. The broker can close the
  connection, or it can wait for the client to close it, as the MQTT 5.0 specification permits.
  Any byte back fails the row, for example a DISCONNECT 0x81 Malformed Packet. Then the test
  closes the connection, as the specification asks of the sender of a DISCONNECT.
- A row that cannot run FAILS. If a row fails, the script exits 1.

Against `../mqtt_broker.json`, the result is 22 of 22, and every row runs. HiveMQ, in `../hivemq`,
gets 25 of 25. The broker logs no ERROR. In 7.30.26 the Python script printed PASS for the three
MQTT 5.0 DISCONNECT rows, but it did not run them. Its `0x80` row also wrapped the properties two
times (`e0098007061f0003627965`). The broker answers that malformed packet with DISCONNECT 0x81.
In 7.30.26 the broker also wrote two ERRORs in its log for it.
