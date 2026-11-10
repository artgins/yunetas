# c_mqtt test

MQTT GClass test. Spins up an embedded MQTT broker and a client inside the same process and exercises a **QoS 0 publish/subscribe round-trip** to verify the client-side protocol and broker integration.

`test_mqtt_acl` (`main_acl.c` + `c_acl.c`) drives the broker's publish/subscribe ACL through `EV_MQTT_ACL_CHECK`. It also checks `list-queues queue=<name>` of a queue that exists and cannot be opened (its `keys/` of mode 0, SKIPPED as root): `-1`, *"cannot open the queue"*. Up to 7.25.4 it answered `0` with an empty list. And `list-queues` / `clean-queues` of a store whose directory cannot be listed (mode 0, SKIPPED as root): `-1`, *"cannot list the topics of the store[, nothing cleaned], see the log"* -- a cause of their own, not the last ERROR of the process read back (which said *"...: Success"*, the `errno` of the log call, not of the listing).

`test_mqtt_malformed` sends malformed MQTT packets to the broker.

`test_mqtt_will_acl` (`main_will_acl.c` + `c_will_acl.c`) uses RAW MQTT 3.1.1
clients against the broker with `enable_acl` on. The group of `will_bad` and
`will_ok` may publish only to `allowed/#`; `will_sub` (a group with no
patterns) subscribes `forbidden/will` and `allowed/will`. `will_bad` connects
with a will to `forbidden/will` and goes away with no DISCONNECT: the broker
must refuse the will (the WARNING *"Will message refused by the ACL: not
published"*) and `will_sub` must get nothing. `will_ok` does the same with a
will to `allowed/will`, which `will_sub` must get. Up to 7.25.22 the will
bypassed the ACL (red: `will_sub` got the will to `forbidden/will`). Then
`will_ret` publishes a RETAINED message with a payload (QoS 1), and `will_late`
subscribes it afterwards: it must get it flagged retained, with its payload.
Each step waits for the packet it depends on (CONNACK, SUBACK, PUBACK, the
PUBLISH the subscriber gets); the timer is only a guard that names the step
that did not come. The memory check at the end caught a second defect: each
CONNECT with a will, and each retained PUBLISH with a payload, leaked its
payload (`C_NODE` handed the treedb the event's shared kw, whose `gbuffer` the
treedb takes; red: 3 x 357 bytes not freed). Last, `will_deny`, an MQTT 3.1
client (`MQIsdp`, level 3), subscribes `denied/topic`, in the broker's
`deny_subscribes`: the SUBACK must say `0x80`. Up to 7.30.26 a 3.1 client got
the QoS it asked for, as if granted (red: code 0).

The leak checks of these tests (*"system memory not free"* at the end) work
only in a build with `CONFIG_DEBUG_TRACK_MEMORY`; where it is off (the nodes'
builds, a staging node's release run) they pass whatever leaks.

`test_mqtt_queued_in` (`main_queued_in.c` + `c_queued_in.c`) uses a RAW client
(a `C_TCP` that writes MQTT 3.1.1 packets by hand). Session 1 (clean session 0)
sends four QoS 2 PUBLISH, ids 1..4, gets four PUBREC, and goes away without
PUBREL. The broker's `max_inflight_messages` goes down to 2, and session 2
reloads the four: 1 and 2 in flight, 3 and 4 QUEUED. PUBREL 1..4 must be
answered `PUBREC 3, PUBCOMP 1, PUBREC 4, PUBCOMP 2, PUBCOMP 3, PUBCOMP 4`, with
no error: a queued message goes in flight when a slot frees, with its own
packet id, as in mosquitto. Up to 7.25.4 the quota test was inverted and the
moved message got a NEW packet id: the broker answered `PUBREC 1`, `PUBREC 2`,
and the PUBREL of 3 and 4 logged *"Message not found"* (never released).

`test_mqtt_client_queues` (`main_client_queues.c` + `c_client_queues.c`) is the
other side: a C_PROT_MQTT2 CLIENT with persistent queues (`tranger_queues`)
against a RAW broker (a `C_PROT_RAW` channel of the driver that writes the MQTT
3.1.1 packets by hand). A. `PUBLISH 5 'a'` (QoS 2), again with DUP=1 (its
PUBREC 'lost'), `PUBREL 5`, then `PUBLISH 5 'b'` and `PUBREL 5`: the user must
get `a` once and then `b`, with no error. Up to 7.25.4 the duplicate was
searched by the rowid of its queue record: the old copy stayed, and the second
PUBREL delivered `a` again (red: *"a,a,"*); and every QoS 2 PUBREL logged
*"QoS mismatch"* (the flag bits compared with the qos level). B. 22 QoS 1
messages of the user with 20 in flight, `m21` with `expiry_interval` 1: after
2.5 s, `PUBACK 1` frees a slot, `m21` goes in flight expired and is discarded,
and `m22` must be sent. Up to 7.25.4 nothing moved after the expiry (red: `m22`
never sent).

`test_mqtt_out_flight` (`main_out_flight.c` + `c_out_flight.c`) uses a RAW MQTT 5
client against the broker, whose `max_inflight_messages` is 0 ("no maximum").
The client connects with Receive Maximum 2 (no clean start), subscribes to
`t/o` with QoS 1 and publishes four QoS 1 messages to it. A. The broker must
send TWO of them, and the other two after the client's PUBACKs: up to 7.25.4
it checked only its own maximum and sent all four. B. With three acked,
`list-queues queue=oflight_cl-OUT qos=1` must list ONE message: up to 7.25.4
`qos=` replaced the pending condition and listed the delivered ones too. C. The
fourth is acked with a PUBCOMP (the ack of QoS 2): the broker must say *"QoS
mismatch"* as a WARNING, answer DISCONNECT 0x82 and keep the message pending.
Up to 7.25.4 it logged an ERROR and removed the message as delivered.

`test_mqtt_wrong_ack` (`main_wrong_ack.c` + `c_wrong_ack.c`) uses a RAW MQTT 5
client against the broker, and a log handler of `main` that reads EVERY line,
traces included. A. `auth_data` written on the broker's `C_PROT_MQTT2` must be
shown as `********` by `view-attrs attribute=auth_data` and `view-gobj`: up to
7.25.20 it lacked `SDF_SECRET` and showed in clear. B. With the `show-decode`
trace armed, a CONNECT with username `wack_user` and a 300-byte password: the
password must appear in no line of the log, and the username must be printed
as `'wack_user'`. Up to 7.25.20 the trace printed the password, and read the
username past its end (the packet is not NUL-terminated): *"username
'wack_user,PASSWORD...'"*. C. A QoS 1 PUBLISH 20, then PUBREL 20: the broker
has no QoS 2 message 20, so it must answer PUBCOMP 20 and say *"Message not
found"* as a WARNING. D. PUBREC of the QoS 1 message the broker delivered: a
*"QoS mismatch"* WARNING and DISCONNECT 0x82. Up to 7.25.20 both C and D were
ERRORs (C with a stack); the test fails on any ERROR in the log. E. A second
raw client sends the same CONNECT with one byte too many: the broker refuses
it and dumps the frame, and the dump must not carry the password (the scanner
also looks for a 16-byte row of it, as a dump splits it). Up to 7.25.20 the
whole CONNECT was dumped.

`test_mqtt_gate_prot` (`main_gate_prot.c` + `c_gate_prot.c`) drives
`C_PROT_MQTT2` as the input of a gate: no broker, no queues
(`tranger_queues` NULL), `allow_anonymous`, `max_subscriptions_per_client` 3,
`max_topic_filter_length` 16, `max_inflight_messages` 2. The driver is its
upper level, as a gate's decoder is: it answers the CONNECT, writes the reason
codes of a (UN)SUBSCRIBE, takes the messages (returns 1), and sends one to the
client. On a `C_FAKE_TRANSPORT`, with the packets written by hand:

1. MQTT 3.1.1 with a username and a password nobody checks: CONNACK 0.
2. PUBLISH QoS 0, 1 and 2, each delivered once (PUBACK, PUBREC); the QoS 2
   sent again with DUP before its PUBREL gets PUBREC and is not delivered
   again; its PUBREL gets PUBCOMP.
3. A PUBREL, PUBACK, PUBREC and PUBCOMP of ids the server never used: PUBCOMP
   and PUBREL, one WARNING for the connection. No crash.
4. SUBSCRIBE of 5 filters: the one of 22 bytes and the one past 3 get `0x80`,
   the others QoS 0; a filter held is granted again and not counted; an
   UNSUBSCRIBE makes room.
5. A QoS 1 message to the client goes in QoS 0.
6. Two QoS 2 without PUBREL, then a third: the client is dropped at once
   (3.1.1: no DISCONNECT from the server), with a WARNING.
7. MQTT 5 on a new connection: `0x8F` and `0x97` in the SUBACK (the counts
   are of the connection), PUBACK Success for a message taken.

No log of an ACL (there is no broker). Up to 7.30.25: CONNACK 5 (the CONNECT
was authenticated against the broker's treedb) and nothing else.

`test_mqtt_gate_limits` (`main_gate_limits.c` + `c_gate_limits.c`) is what a
peer can do to the same gate input, on a `C_FAKE_TRANSPORT`, with
`max_subscriptions_per_client` 3, `message_size_limit` 16, `max_packet_size`
70000, `max_qos` 1 and `timeout_close` 2 s. Every line was red on 7.30.26:

1. MQTT 5 CONNACK carries Maximum QoS 1 (it was not sent, and an ERROR
   *"Mqtt property int16 unknown"* was logged).
2. A QoS 1 PUBLISH with three user properties past `message_size_limit`:
   PUBACK `0x95`, and no leak (its properties were not freed: the memory check
   at the end).
3. A SUBSCRIBE of 13000 copies of `a`: all go up, 13000 codes in the SUBACK
   (a 12 KB buffer: 8426 pairs of ERRORs and a SUBACK of mixed codes).
4. A filter the upper level refuses (`0x87`) is not held: two filters more fit
   under 3, the third gets `0x97`.
5. An UNSUBSCRIBE of 20000 filters: 20000 codes in the UNSUBACK (12287).
6. The upper level writes no code for one filter: ERROR *"The upper level did
   not answer one reason code per filter"*, and `0x80` for it (the UNSUBACK
   was short of a code, and nothing was said).
7. `EV_SEND_MESSAGE` to `cmnd/x/#`: `-1` and an ERROR (it was published).
8. Five acks of unknown packet ids: one WARNING (five).
9. `EV_DROP` with `session_take_over`: DISCONNECT `0x8E`. Then, while the
   connection is closing, `EV_SEND_MESSAGE` is `-1` (twice, one WARNING) and another
   `EV_DROP` drops the transport at once (both were *"Event NOT DEFINED"*
   ERRORs, and the drop waited for `timeout_close`).
10. A PUBLISH of 20000 bytes written in two parts, cut after its 1st to 5th
    byte: delivered every time (cut after the 3rd, the second byte of the
    length was read twice and the connection ended as malformed).
11. A DISCONNECT `e0 02 00 00` and a PUBLISH behind it in the same write: no
    ERROR, and the PUBLISH is not read (*"kw must be list or dict"* with a
    stack, and the PUBLISH delivered).
12. A CONNECT with client id `.`: CONNACK `0x85`, and `main` finds no dump
    *"MQTT malformed packet: CMD_CONNECT"* in the log (there was one, and an
    ERROR *"Mqtt proplist NULL"*).

`test_mqtt_client_noqueue` (`main_client_noqueue.c` + `c_client_noqueue.c`)
drives `C_PROT_MQTT2` as a CLIENT without queues on a `C_FAKE_TRANSPORT`; the
driver plays the broker (MQTT 5). Its user publishes in QoS 1 inside
`EV_ON_OPEN`: `-1` and an ERROR *"qos not supported"* (it went to the NULL
queue: SIGSEGV). A QoS 0 message goes out; a QoS 1 PUBLISH of the broker is
delivered and acked; a QoS 2 one disconnects the client (DISCONNECT `0x93`,
a WARNING) and is not delivered (its payload was released twice).

The tests of `C_PROT_MQTT` (`legacy_prot` and `legacy_sessions`) went with it
after 7.30.26: the gclass was removed once every gate was on `C_PROT_MQTT2`.

`test_mqtt_client_pubrec` (`main_client_pubrec.c` + `c_client_pubrec.c`) drives
`C_PROT_MQTT2` as a CLIENT on a `C_FAKE_TRANSPORT`, with its queues in a
`C_TRANGER` of the driver; the driver plays the broker. After CONNACK: a PUBREC
of a packet id the client never used must be said once, as the WARNING *"Mqtt:
Received PUBREC for an unknown packet ID"*, and answered with PUBREL (up to
7.25.20 the client also logged an ERROR *"Message not found"* with a stack).
Then a QoS 1 PUBLISH of the client, and PUBREC of it: *"QoS mismatch"*, a
WARNING that must name the client (`client_id`, checked in the log), and
DISCONNECT 0x82.

The tests that keep a store (`acl`, `queued_in`, `client_queues`,
`out_flight`, `wrong_ack`, `client_pubrec`, `gate_prot`, `gate_limits`,
`client_noqueue`) work in a dir of
their own run, `<$TMPDIR or /tmp>/test_mqtt_<name>.<pid>.<n>`
(`test_work_dir.c`), removed at the end; each binary has a port of its own.
So they run at once (`ctest -j`). Up to 7.25.20 each wiped a fixed
`/tmp/test_mqtt_<name>` at its start.

## Run

```bash
ctest -R '^c_mqtt/' --output-on-failure --test-dir build
```

Requires `CONFIG_MODULE_MQTT=y`.
