# c_mqtt test

MQTT GClass test. Spins up an embedded MQTT broker and a client inside the same process and exercises a **QoS 0 publish/subscribe round-trip** to verify the client-side protocol and broker integration.

## Run

```bash
ctest -R test_c_mqtt --output-on-failure --test-dir build
```

Requires `CONFIG_MODULE_MQTT=y`.
