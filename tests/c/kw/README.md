# kw test

Unit tests for the `kw_*` keyword / JSON helpers in `kernel/c/gobj-c/src/kwid.c`. These are the workhorse functions every GClass uses to read config values, pattern-match events and build responses — a regression here breaks almost everything.

## Run

```bash
ctest -R test_kw --output-on-failure --test-dir build
```
