# `trq_open()`

**Description:**
Opens a queue using a specified topic name and parameters. Creates the topic if it does not exist.

**Prototype:**
```c
PUBLIC tr_queue trq_open(
    json_t *tranger,
    const char *topic_name,
    const char *pkey,
    const char *tkey,
    system_flag2_t system_flag,
    size_t backup_queue_size
);
```

**Parameters:**
- `tranger` (`json_t *`): The TimeRanger instance.
- `topic_name` (`const char *`): Name of the topic.
- `pkey` (`const char *`): Primary key for the topic.
- `tkey` (`const char *`): Secondary key for the topic.
- `system_flag` (`system_flag2_t`): System flags for topic configuration.
- `backup_queue_size` (`size_t`): The backup queue size.
