# tr_queue test

Tests the **msg2db queue** on top of `timeranger2`: enqueue, dequeue, ack, persistence across restarts. This is the queue used by the MQTT broker for retained sessions, subscriptions and in-flight messages.

`test_tr_queue_load_failed` covers a queue whose load cannot read every
pending message (the md2 of its key cut behind the running tranger's back):
`trq_load()` / `tr2q_load()` answer -1 and do not move `first_rowid`, the
periodic backup (`trq_check_backup()` / `tr2q_check_backup()`, called while
the queue is empty) is refused and says so once, and after the md2 is put back
a restart loads every pending message.

## Run

```bash
ctest -R test_tr_queue --output-on-failure --test-dir build
```
