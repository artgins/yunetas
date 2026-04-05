# list_queue_msgs2

List messages from a **msg2db queue** (message queue persisted on timeranger2). Supports listing all messages or a specific record, with local timestamps and metadata.

## Usage

```bash
list_queue_msgs2 --path <queue-path> [--rowid <id>] [options]
```

Companion to the `msg2db` API used by the MQTT broker and other queue consumers.
