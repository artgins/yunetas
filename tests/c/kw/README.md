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

`test_kw_binary_depth` checks that a binary field (`gbuffer`) counts only at
the TOP level of a kw: `kw_duplicate()`, `kw_filter_metadata()` and
`kw_filter_private()` take no reference of a gbuffer nested in a dict or a
list (up to 7.30.16 they did, and a peer's `{"x": {"gbuffer": 4096}}` crashed
in `gbuffer_incref()`), and still take one of the top-level one. It also
checks the frontier: `gbuf2json_from_peer()`, `anystring2json_from_peer()`
and `iev_create_from_gbuffer()` leave no `"gbuffer"` key at any depth in a
peer's json (spelled with a `\u` escape too), with ONE warning:

```
--- a nested gbuffer is data, not the kw's binary field ---
  ok  kw_duplicate: no reference taken
...
--- a json of a peer has no binary field name, at any depth ---
  ok  anystring2json_from_peer drops them all
  ok  anystring2json_from_peer: one warning
...
all passed
```

## Run

```bash
ctest -R '^kw/' --output-on-failure --test-dir build
```
