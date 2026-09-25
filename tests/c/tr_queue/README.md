# tr_queue test

Tests the **msg2db queue** on top of `timeranger2`: enqueue, dequeue, ack, persistence across restarts. This is the queue used by the MQTT broker for retained sessions, subscriptions and in-flight messages.

`test_tr_queue_load_failed` covers a queue whose load cannot read every
pending message (the md2 of its key cut behind the running tranger's back):
`trq_load()` / `tr2q_load()` answer -1 and do not move `first_rowid`, the
periodic backup (`trq_check_backup()` / `tr2q_check_backup()`, called while
the queue is empty) is refused and says so once, and after the md2 is put back
a restart loads every pending message.

`test_tr2q_queued` covers the QUEUED messages of an mqtt queue (above
`max_inflight_messages`, on disk without their content). A queued message
whose content cannot be read (the content file cut) stays queued:
`tr2q_move_from_queued_to_inflight()` answers -1 with *"Cannot load the content
of a queued message: it stays queued"*. Up to 7.25.4 it was moved in flight
first and left there with no content (red: 2 in flight, 0 queued). And
`tr2q_check_backup()` with nothing in flight and a message queued answers 0 and
does not back up: the queued message keeps its content. Up to 7.25.4 the
backup was made and the content was lost (red: topic size 0).

`test_tr_queue_backup_failed` covers a backup that FAILS (the backup name
taken by a file: the `rename()` fails): `trq_check_backup()` /
`tr2q_check_backup()` answer -1, the queue keeps its topic (the backup opens
it again), and appends, reads and acks work after it. Up to 7.25.4 the queue
was left with no topic and the call answered 0; `tranger2_backup_topic()`
also leaked the `topic_var` it had loaded (the memory check at the end).
And a backup that fails AFTER the move: the new topic cannot be created (the
`mkdir` of its directory fails with `ENOSPC`, by `--wrap=mkdir`). The backup
is moved back, the queue keeps its topic whole, and the next call backs it up.
In 7.25.4 the messages stayed in the backup and the queue had no topic.
And a create that fails only at the `mkdir` of its `keys/`:
`tranger2_create_topic()` answers `NULL` and leaves nothing on disk or in
memory, the next create makes it whole, and a queue backup that meets it keeps
the queue's topic. In 7.25.4 the create answered a topic with no `keys/`,
and the backup took it as the queue's new topic.
And a backup that fails and cannot open the topic again
either (its `topic_desc.json` of mode 0 for a moment; SKIPPED as root): the
queue has no topic, a read and an ack fail and say it once (*"Queue without
topic, it cannot be opened"*; the next calls log nothing, where up to this fix
each one logged the three errors of the open again), and once the file can be
read the queue takes
its topic again by name (*"Queue topic taken again"*) and the next check backs
it up; for `tr_queue` and for `tr2q`. Before this fix the topic stayed `NULL`
for good. And a tranger opened with `on_critical_error` `LOG_OPT_EXIT_ZERO`
(the MQTT broker's queues): a backup whose new topic cannot be created does
not exit (an `atexit()` handler turns such an exit into a failure), moves the
backup back, and the queue keeps its message. And a plain create with `LOG_OPT_EXIT_ZERO` whose `keys/` cannot be made,
in a child process: the child exits(0), as told, but only after it removed
what it made, and the next create makes the topic whole. Up to this fix it
exited in the log of the failed `mkdir`, and the next start opened the half
topic. The CRITICAL *"Cannot create TimeRanger subdir. mkrdir()
FAILED"* is checked to name `ENOSPC` (*"No space left on device"*): up to this
fix the log of `mkrdir()` before it changed `errno`, and it said *"Success"*.

## Run

```bash
ctest -R '^tr_queue/' --output-on-failure --test-dir build
```
