# c_mqtt test

MQTT GClass test. Spins up an embedded MQTT broker and a client inside the same process and exercises a **QoS 0 publish/subscribe round-trip** to verify the client-side protocol and broker integration.

`test_mqtt_acl` (`main_acl.c` + `c_acl.c`) drives the broker's publish/subscribe ACL through `EV_MQTT_ACL_CHECK`. It also checks `list-queues queue=<name>` of a queue that exists and cannot be opened (its `keys/` of mode 0, SKIPPED as root): `-1`, *"cannot open the queue"*. Up to 7.25.4 it answered `0` with an empty list. And `list-queues` / `clean-queues` of a store whose directory cannot be listed (mode 0, SKIPPED as root): `-1`, *"cannot list the topics of the store[, nothing cleaned], see the log"* -- a cause of their own, not the last ERROR of the process read back (which said *"...: Success"*, the `errno` of the log call, not of the listing).

`test_mqtt_malformed` sends malformed MQTT packets to the broker.

## Run

```bash
ctest -R '^c_mqtt/' --output-on-failure --test-dir build
```

Requires `CONFIG_MODULE_MQTT=y`.
