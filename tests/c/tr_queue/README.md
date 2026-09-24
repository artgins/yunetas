# tr_queue test

Tests the **msg2db queue** on top of `timeranger2`: enqueue, dequeue, ack, persistence across restarts. This is the queue used by the MQTT broker for retained sessions, subscriptions and in-flight messages.

`test_tr_queue_load_failed` covers a queue whose load cannot read every
pending message (the md2 of its key cut behind the running tranger's back):
`trq_load()` / `tr2q_load()` answer -1 and do not move `first_rowid`, the
periodic backup (`trq_check_backup()` / `tr2q_check_backup()`, called while
the queue is empty) is refused and says so once, and after the md2 is put back
a restart loads every pending message.

`test_tr_queue_backup_failed` covers a backup that FAILS (the backup name
taken by a file: the `rename()` fails): `trq_check_backup()` /
`tr2q_check_backup()` answer -1, the queue keeps its topic (the backup opens
it again), and appends, reads and acks work after it. Up to 7.25.4 the queue
was left with no topic and the call answered 0; `tranger2_backup_topic()`
also leaked the `topic_var` it had loaded (the memory check at the end).
And a backup that fails AFTER the move: the new topic cannot be created (the
`mkdir` of its directory fails with `ENOSPC`, by `--wrap=mkdir`). The backup
is moved back, the queue keeps its topic whole, and the next call backs it up.
Before this fix the messages stayed in the backup and the queue had no topic.
And a create that fails only at the `mkdir` of its `keys/`:
`tranger2_create_topic()` answers `NULL` and leaves nothing on disk or in
memory, the next create makes it whole, and a queue backup that meets it keeps
the queue's topic. Before this fix the create answered a topic with no `keys/`,
and the backup took it as the queue's new topic.

## Run

```bash
ctest -R test_tr_queue --output-on-failure --test-dir build
```
