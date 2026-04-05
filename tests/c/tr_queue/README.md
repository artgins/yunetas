# tr_queue test

Tests the **msg2db queue** on top of `timeranger2`: enqueue, dequeue, ack, persistence across restarts. This is the queue used by the MQTT broker for retained sessions, subscriptions and in-flight messages.

## Run

```bash
ctest -R test_tr_queue --output-on-failure --test-dir build
```
