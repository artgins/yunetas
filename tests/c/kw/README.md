# kw test

Unit tests for the `kw_*` keyword / JSON helpers in `kernel/c/gobj-c/src/kwid.c`. These are the workhorse functions every GClass uses to read config values, pattern-match events and build responses — a regression here breaks almost everything.

Among them: `kw_find_path()` walks a path of any depth and any segment
length in a loop, logging nothing for a path that is not there with `verbose`
FALSE (`test_reg_f003_kw_find_path_depth`: it recursed, and was cut at 64
segments and 255 bytes with an ERROR); and `json_refcounts_report()` answers
what the refcounts say, each json once, a cycle walked once, logging nothing
-- also a cycle whose refcounts are corrupted below the truth, which it
walked round until its frames passed `MAX_BLOCK`, and which it now REPORTS as
corrupted (a json reached more times than its refcount), as it does a dict
and a string `json_object_set_new()`'d into two places
(`test_json_refcounts_report`). `json_nul_fault()` finds a NUL down a record,
nothing in a clean one, and refuses a json 3000 levels deep.

## Run

```bash
ctest -R '^kw/' --output-on-failure --test-dir build
```
