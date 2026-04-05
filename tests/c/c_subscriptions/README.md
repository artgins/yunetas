# c_subscriptions test

Tests subscribe / unsubscribe / publish semantics of the GObj framework. Verifies that events reach every subscriber and that unsubscribes (explicit and automatic on `gobj_destroy`) clean up properly.

## Run

```bash
ctest -R test_c_subscriptions --output-on-failure --test-dir build
```
