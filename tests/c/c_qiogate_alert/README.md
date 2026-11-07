# c_qiogate_alert test

Tests the queue-size alerts of `C_QIOGATE`: the ERROR *"ALERT Queuing N msgs"*
and its email, at each step of `alert_queue_size` the queue climbs.

One yuno holds a `C_MQIOGATE` (`method: broadcast`) with two `C_QIOGATE`
children (`alert_queue_size` 10), each a persistent queue (under
`/tmp/test_c_qiogate_alert`) towards `tcp://127.0.0.1:7745`, where nobody
listens. The acks are made by the test: what the peer would answer, sent to
the first gate as from its bottom gate (the queue is new and its messages have
one key, so their rowids are 1, 2, 3...). Then:

- 10 messages to the `C_MQIOGATE`: both gates alert at 10, and both log
  *"Service 'emailsender' not found"* (the yuno has none) -- the email limit,
  one a minute, is each gate's;
- 10 more to the first gate: one alert at 20;
- 50 times an ack (19) and a message (20): no alert;
- 6 acks (14, half a step below 20) and 6 messages: one alert at 20;
- 16 acks (4, half a step below 10) and 6 messages: one alert at 10.

The log is checked in order: those seven ERRORs and nothing else.

Up to 7.30.3 the alert was "the size is a multiple of `alert_queue_size`" at
each enqueue, so the swings alerted 50 times (red: 50 unexpected ERRORs);
and the email limit was one `static` timer for every gate of the yuno, so
the second gate never logged that there is no emailsender (red: an expected
entry nobody matched).

## Run

```bash
ctest -R test_c_qiogate_alert --output-on-failure --test-dir build
```
