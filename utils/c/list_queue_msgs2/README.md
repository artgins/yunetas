# list_queue_msgs2

`list_queue_msgs2` lists the messages of a msg2db queue. A msg2db queue is a message queue that timeranger2 keeps on disk. The tool can list all the messages or one record, with local timestamps and metadata.

## Usage

```bash
list_queue_msgs2 --path <queue-path> [--rowid <id>] [options]
```

It is the companion tool of the `msg2db` API. The MQTT broker and other queue consumers use that API.
