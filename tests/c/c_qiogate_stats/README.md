# c_qiogate_stats test

Tests the queue gauges of `C_QIOGATE` (`msgs_in_queue`, `pending_acks`), read
through the stats of a `C_MQIOGATE`.

One yuno holds a `C_MQIOGATE` (`method: broadcast`) with two `C_QIOGATE`
children, each a persistent queue (under `/tmp/test_c_qiogate_stats`) towards
`tcp://127.0.0.1:7746`, where nobody listens. The test sends 5 messages to the
`C_MQIOGATE`: each queue holds 5. Then:

- `gobj_stats()` of the `C_MQIOGATE` answers, for each child, `msgs_in_queue`
  5 and `pending_acks` 0;
- each child's attribute `msgs_in_queue` is 5;
- `gobj_stats()` of a `C_QIOGATE` (`output-q1`) and of a `C_IOGATE`
  (`output-iogate-q1`, the gate below it) answers the envelope: `result` 0,
  and the counters (`msgs_in_queue`, `txMsgs`) under `data`.

Up to 7.25.21 the two counts were only in the `C_QIOGATE`'s own `mt_stats`,
which a `C_MQIOGATE` never asks (it reads its children's stats attributes with
`build_stats()`): the size of a persistent link's queue could not be read
through it (red: the three checks fail for both children). And up to the
same release both gates' `mt_stats` returned the bare counters, so
`stats-yuno` on either answered `-1` (red: no `result`, no `data`).

Then the test shows that a gate whose queue cannot save a message refuses it
and goes on. It sets `min_free_disk_percentage` of `output-q1` to `101` (a
disk that is always too full) and sends 3 more messages:

- each `EV_SEND_MESSAGE` returns `-1` (the broadcast returns the error of
  `output-q1`);
- `output-q1` counts them in `dropped_msgs` (3), and in the stats of the
  `C_MQIOGATE`; its queue keeps its 5 messages, and it logs one ERROR,
  *"Queue refuses new messages"*;
- `output-q2` takes them (8 messages, `dropped_msgs` 0).

With `min_free_disk_percentage` `0`, the poll of `output-q1` accepts messages
again (one INFO), and the next message goes into its queue. Up to 7.28.3 the
gate had no such threshold, and a message its queue could not save aborted
the yuno (red: the messages go in, and the checks fail).

## Run

```bash
ctest -R test_c_qiogate_stats --output-on-failure --test-dir build
```
