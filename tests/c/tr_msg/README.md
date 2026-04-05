# tr_msg test

Tests the **msg2db** wrapper on top of `timeranger2` — a dict-style message store where each `pkey` / `pkey2` holds a single value. Covers encoding, storage, retrieval and overwrite semantics.

## Run

```bash
ctest -R test_tr_msg --output-on-failure --test-dir build
```
