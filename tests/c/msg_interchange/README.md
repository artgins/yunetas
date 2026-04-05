# msg_interchange test

Tests the **inter-yuno message interchange** layer (`msg_ievent` / `iev_msg`) — the encoding that turns an event + `kw` payload into bytes on the wire and back. Uses a JSON-driven test runner so cases can be added without recompiling.

## Run

```bash
ctest -R test_msg_interchange --output-on-failure --test-dir build
```
